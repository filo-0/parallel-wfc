
#include <cassert>
#include <random>
#include <vector>
#include <iostream>

#include "pwfc.hpp"
#include "debug.hpp"

namespace PWFC 
{
    void QPattern::Collapse(std::mt19937 &rand)
    {
        int pick = rand() % Entropy();
        Iterator p = begin();
        for (int i = 0; i < pick; i++)
            ++p;

        int state = p.Idx;
        _State.reset();
        _State.set(state);
    }
    void QPattern::Collapse(int state)
    {
        _State.reset();
        _State.set(state);
    }
    static inline int iOpposite(int dir) { return (dir + 2) % 4; }
    //static inline int iIdx(int x, int y, int chunkLen) { return y * chunkLen + x; }
    
    static inline int iX(int idx, int chunkLen) { return idx % chunkLen; }
    static inline int iY(int idx, int chunkLen) { return idx / chunkLen; }

    static inline int iNextX(int x, int dir) { return x + (2 - dir) % 2; }
    static inline int iNextY(int y, int dir) { return y + (1 - dir) % 2; }
    
    static inline bool iCanConnect(Pattern a, Pattern b, int a2b)
    {
        int b2a = iOpposite(a2b);

        bool aOpen = a.Get(iNextX(1, a2b), iNextY(1, a2b));
        bool bOpen = b.Get(iNextX(1, b2a), iNextY(1, b2a));

        return aOpen == bOpen;
    }

    
    static ErrCode iPropagate(std::array<QChunk*, 9> &chunks, int centerX, int centerY)
    {
        struct iWave { int Chunk, FromX, FromY, Dir; };
        std::vector<iWave> stack = 
        {
            {4, centerX, centerY, 0 },
            {4, centerX, centerY, 1 },
            {4, centerX, centerY, 2 },
            {4, centerX, centerY, 3 },
        };
        int chunkLen = chunks[4]->Length();
        while(!stack.empty())
        {
            iWave wave = stack.back();
            stack.pop_back();
            
            int toX = iNextX(wave.FromX, wave.Dir);
            int toY = iNextY(wave.FromY, wave.Dir);
            
            int toChunkId = wave.Chunk;
            if     (toX >= chunkLen) { toChunkId += 1; toX = 0; }
            else if(toY >= chunkLen) { toChunkId += 3; toY = 0; } 
            else if(toX < 0)          { toChunkId -= 1; toX = chunkLen - 1; }
            else if(toY < 0)          { toChunkId -= 3; toY = chunkLen - 1; }

            if(toChunkId < 0 || toChunkId >= 9)
            {
                PWFC_LOG_ERROR("Propagation went out of the 3x3 chunk scope!");
                return ERR_OUT_OF_CHUNK;
            }
            if(chunks[toChunkId] == nullptr)
                continue;

            QChunk &fromChunk = *chunks[wave.Chunk];
            QChunk &toChunk   = *chunks[toChunkId];

            const QPattern &from = fromChunk.Get(wave.FromX, wave.FromY);
                  QPattern &to   = toChunk  .Get(toX, toY);

            bool changed = false;
            for (auto i = to.begin(); i != to.end(); ++i)
            {
                bool match = false;
                for (auto j = from.begin(); j != from.end(); ++j)
                {
                    if (iCanConnect(from.State(j.Idx), to.State(i.Idx), wave.Dir))
                    {
                        match = true;
                        break;
                    }
                }

                if (!match)
                {
                    to.Clear(i.Idx);
                    changed = true;
                }
            }
            if (!changed)
                continue;

            if(to.Entropy() == 0)
            {
                PWFC_LOG_ERROR("Cell was collapsed to zero entropy!");
                return ERR_ZERO_ENTROPY;
            }

            for (int d = 0; d < 4; d++)
                if (d != iOpposite(wave.Dir))
                    stack.push_back({ toChunkId, toX, toY, d });
        }

        return ERR_OK;
    }

    ErrCode Generate(std::array<QChunk*, 9>& chunks, unsigned int seed)
    {
        if(chunks[4] == nullptr || chunks[4]->Length() < MIN_CHUNK_LENGTH)
            return ERR_INVALID_INPUT;

        auto &pivot = *chunks[4];
        std::vector<int> indices;
        std::mt19937 rand(seed);
        int chunkLen = pivot.Length();
        int chunkSize = chunkLen * chunkLen;

        while(true)
        {
            int minEntropy = MAX_ENTROPY;
            indices.clear();
            for(int idx = 0; idx < chunkSize; idx++)
            {
                QPattern &q = pivot.Get(idx);
                if(q.IsCollapsed())
                    continue;
                if(q.Entropy() == minEntropy)
                {
                    indices.push_back(idx);
                }
                else if(q.Entropy() < minEntropy)
                {
                    indices.clear();
                    minEntropy = q.Entropy();
                    indices.push_back(idx);
                }
            }
            if(indices.empty())
                break;
            
            int idx = indices[rand() % indices.size()];
            pivot.Get(idx).Collapse(rand);

            ErrCode err = iPropagate(chunks, iX(idx, chunkLen), iY(idx, chunkLen));
            if(err)
                return err;
        }

        return ERR_OK;
    }


    void PrintQChunk(QChunk& chunk, std::ostream &stream) 
    {
        int chunkLen = chunk.Length();
        std::vector<std::vector<char>> buffer(chunkLen * PATTERN_SIZE, std::vector<char>(chunkLen * PATTERN_SIZE));

        for (int y = 0; y < chunkLen; y++) 
        {
            for (int x = 0; x < chunkLen; x++) 
            {
                // Assuming Get(x, y) or Get(y * CHUNK_LEN + x) is available
                QPattern& q = chunk.Get(y * chunkLen + x);

                bool collapsed = q.IsCollapsed();
                // Fetch the collapsed state
                Pattern state = q.State();
                for (int i = 0; i < PATTERN_SIZE; i++)
                    for (int j = 0; j < PATTERN_SIZE; j++)
                        buffer[y * PATTERN_SIZE + i][x * PATTERN_SIZE + j] = collapsed ? (state.Get(j, i) ? ' ' : '#') : '?';
            }
        }
        for (int y = 0; y < chunkLen * PATTERN_SIZE; y++) 
        {
            for (int x = 0; x < chunkLen * PATTERN_SIZE; x++) 
                stream << ' ' << buffer[y][x];
            stream << "\n";
        }
    }

    bool iValidateBorders(const QChunk &a, const QChunk &b, int a2b)
    {
        PWFC_ASSERT(a.Length() == b.Length(), "Invalid chunk lengths!");

        int chunkLen = a.Length();
        int b2a = iOpposite(a2b);
        int pax = iNextX(1, a2b) / 2 * (chunkLen - 1);
        int pay = iNextY(1, a2b) / 2 * (chunkLen - 1);
        int pbx = iNextX(1, b2a) / 2 * (chunkLen - 1);
        int pby = iNextY(1, b2a) / 2 * (chunkLen - 1);
        
        for (int i = 0; i < chunkLen; i++) 
        {
            Pattern stateA = a.Get(pax, pay).State();
            Pattern stateB = b.Get(pbx, pby).State();
            if(!iCanConnect(stateA, stateB, a2b))
                return false;

            if(a2b % 2 == 0)
                pax = ++pbx;
            else 
                pay = ++pby;
        }
        return true;
    }
    bool Validate(const std::vector<QChunk*> &chunks, int len)
    {
        if(len * len != (int)chunks.size())
        {
            PWFC_LOG_ERROR("Invalid chunk array size!");
            return false;
        }

        for (QChunk* chunk : chunks) 
        {
            if(!chunk)
            {
                PWFC_LOG_ERROR("Null chunk found!");
                return false;
            }
            for (QPattern p : *chunk) 
            {
                if(!p.IsCollapsed())
                {
                    PWFC_LOG_ERROR("Found non collapsed cell!");
                    return false;
                }
            }
        }

        for (int y = 0; y < len; y++) 
        {
            for (int x = 0; x < len; x++) 
            {
                QChunk &cur = *chunks[y * len + x];
                for(int dir = 0; dir < 4; dir++)
                {
                    int ox = iNextX(x, dir);
                    int oy = iNextY(y, dir);

                    if(ox < 0 || oy < 0 || ox >= len || oy >= len)
                        continue;
                    const QChunk &other = *chunks[oy * len + ox];
                    if(!iValidateBorders(cur, other, dir))
                    {
                        PWFC_LOG_ERROR("Chunk (%d,%d) cant connect with chunk (%d,%d)", x, y, ox, oy);
                        return false;
                    }
                }
            }
        }
        return true;
    }
}
