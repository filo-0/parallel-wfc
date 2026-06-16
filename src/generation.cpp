
#include <cassert>
#include <random>
#include <set>
#include <functional>
#include <vector>

#include "pwfc.hpp"
#include "debug.hpp"
#include "types.hpp"

namespace PWFC 
{
    void QPattern::Collapse(std::mt19937 &rand, const std::vector<float> weights)
    {
        float maxWeight = 0.f;
        for(Iterator p = begin(); p != end(); ++p)
            maxWeight += weights[p.Idx];
        
        float pick = (float)rand() / rand.max() * maxWeight;
        
        Iterator p = begin();
        float    i = weights[p.Idx];
        while (i < pick)
        {
            ++p;
            PWFC_ASSERT(p != end(), "Out of bound collapse!");
            i += weights[p.Idx];
        }

        int state = p.Idx;
        _State.Clear();
        _State.Set(state);
    }
    void QPattern::Collapse(int state)
    {
        _State.Clear();
        _State.Set(state);
    }
    static inline int iOpposite(int dir) { return (dir + 2) % 4; }
    //static inline int iIdx(int x, int y, int chunkLen) { return y * chunkLen + x; }
    
    static inline int iX(int idx, int chunkLen) { return idx % chunkLen; }
    static inline int iY(int idx, int chunkLen) { return idx / chunkLen; }

    static inline int iNextX(int x, int dir) { return x + (2 - dir) % 2; }
    static inline int iNextY(int y, int dir) { return y + (1 - dir) % 2; }

    struct iQPatternID
    {
        int ID;
        QPattern Pattern;
        
        inline iQPatternID(int id, QPattern pattern) : ID(id), Pattern(pattern) { } 
        inline bool operator<(const iQPatternID &other) const
        { 
            int thisEntropy  = Pattern.Entropy();
            int otherEntropy = other.Pattern.Entropy(); 
            if(thisEntropy == otherEntropy)
                return ID < other.ID;
            return thisEntropy < otherEntropy;
        }
    };

    static ErrCode iPropagate(std::set<iQPatternID> &uncollapsed, std::array<QChunk*, 9> &chunks, const RuleSet &ruleset, int centerX, int centerY)
    {
        struct iWave 
        { 
            int Chunk, FromX, FromY, Dir; 
            inline iWave(int c, int x, int y, int d) :
                Chunk(c), FromX(x), FromY(y), Dir(d) { }
        };
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
            
            // Current pattern in the uncollapsed set
            iQPatternID p = { toX + toY * chunkLen, to };

            bool changed = false;
            for (auto i = to.begin(); i != to.end(); ++i)
            {
                bool match = false;
                for (auto j = from.begin(); j != from.end(); ++j)
                {
                    if (ruleset.Allows(j.Idx, i.Idx, wave.Dir))
                    {
                        match = true;
                        break;
                    }
                }

                if (!match)
                {
                    if(!changed && toChunkId == 4) // erase only first time
                        uncollapsed.erase(p);

                    to.Clear(i.Idx);
                    changed = true;
                }
            }
            if (!changed)
                continue;
            
            int updatedEntropy = to.Entropy();
            if(updatedEntropy == 0)
            {
                PWFC_LOG_ERROR("Cell was collapsed to zero entropy!");
                return ERR_ZERO_ENTROPY;
            }
            else if(updatedEntropy != 1 && toChunkId == 4)
            {
                p.Pattern = to;
                uncollapsed.emplace(p);
            }
            // else it has collapsed

            for (int d = 0; d < 4; d++)
                if (d != iOpposite(wave.Dir))
                    stack.emplace_back(toChunkId, toX, toY, d);
        }

        return ERR_OK;
    }

    ErrCode Generate(std::array<QChunk*, 9>& chunks, const RuleSet &ruleset)
    {
        if(chunks[4] == nullptr || chunks[4]->Length() < MIN_CHUNK_LENGTH)
            return ERR_INVALID_INPUT;
        
        auto &pivot = *chunks[4];
        std::mt19937 rand(ruleset.Seed());
        int chunkLen = pivot.Length();
        
        std::set<iQPatternID> uncollapsed;
        for(int i = 0; i < pivot.Size(); i++)
        {
            QPattern &p = pivot.Get(i);
            if(p.Entropy() > 1)
                uncollapsed.emplace(i, p);
            else if(p.Entropy() == 0)
                return ERR_ZERO_ENTROPY;
        }

        while(!uncollapsed.empty())
        {
            iQPatternID p = *uncollapsed.begin();
            uncollapsed.erase(uncollapsed.begin());
            int idx = p.ID;
            
            PWFC_ASSERT(pivot.Get(idx).Entropy() != 1, "Cell already collapsed!");
            PWFC_ASSERT(pivot.Get(idx).Entropy() != 0, "Found unhaldled zero entropy cell!");

            pivot.Get(idx).Collapse(rand, ruleset.Weights());

            ErrCode err = iPropagate(uncollapsed, chunks, ruleset, iX(idx, chunkLen), iY(idx, chunkLen));
            if(err)
                return err;
        }

        return ERR_OK;
    }

    bool iValidateBorders(const RuleSet &ruleset, const QChunk &a, const QChunk &b, int a2b)
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
            int stateA = a.Get(pax, pay).State();
            int stateB = b.Get(pbx, pby).State();
            if(!ruleset.Allows(stateA, stateB, a2b))
                return false;

            if(a2b % 2 == 0)
                pax = ++pbx;
            else 
                pay = ++pby;
        }
        return true;
    }
    bool Validate(const std::vector<QChunk*> &chunks, const RuleSet &ruleset, int len)
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
                    if(!iValidateBorders(ruleset, cur, other, dir))
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
