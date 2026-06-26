
#include <cassert>
#include <chrono>
#include <random>
#include <set>
#include <vector>

#include "pwfc/pwfc.hpp"
#include "pwfc/types.hpp"
#include "pwfc/debug.hpp"

namespace PWFC 
{
/* ----------------------- QPATTERN ------------------------------ */

    #define CEIL_DIV(n, d) (((n) + (d) - 1) / (d))

    inline static void ClearOne(BitsetBlock *bits, int i) { bits[i / BITS_PER_BLOCK] &= ~(BitsetBlock(1) << (i % BITS_PER_BLOCK)); }
    inline static void ClearAll(BitsetBlock *bits, int n) { std::fill(bits, bits + CEIL_DIV(n, BITS_PER_BLOCK), BitsetBlock(0)); }
    inline static void Set(BitsetBlock *bits, int i)   { bits[i / BITS_PER_BLOCK] |= BitsetBlock(1) << (i % BITS_PER_BLOCK); }
    inline static bool Get(const BitsetBlock *bits, int i) { return bits[i / BITS_PER_BLOCK] & (BitsetBlock(1) << (i % BITS_PER_BLOCK)); }
    inline static int  Count(const BitsetBlock *bits, int n)
    {
        int acc = 0;

#if defined(__GNUC__) || defined(__clang__)
        int numBlocks = CEIL_DIV(n, BITS_PER_BLOCK);
        for(int b = 0; b < numBlocks; b++)
            acc += __builtin_popcountll((unsigned long long)bits[b]);
#else 
        for(int i = 0; i < _Size; i++)
            acc += Get(i);
#endif

        return acc;
    }
    static int FindFirst(const BitsetBlock *bits, int n, int i = -1)
    {
#if defined(__GNUC__) || defined(__clang__) 

        int startIdx = i + 1;
        if (startIdx >= n) return n;

        int blockIdx = startIdx / BITS_PER_BLOCK;
        int bitOffset = startIdx % BITS_PER_BLOCK;

        // Process the first block (which might be partially masked)
        BitsetBlock block = bits[blockIdx];

        // Mask out the bits before 'bitOffset'
        block &= ~((BitsetBlock(1) << bitOffset) - 1); 

        if (block != 0) 
        {
            int tz = __builtin_ctzll((unsigned long long)block);
            int result = blockIdx * BITS_PER_BLOCK + tz;
            return (result < n) ? result : n;
        }

        // Fast-scan the remaining full blocks
        int numBlocks = CEIL_DIV(n, BITS_PER_BLOCK);
        for(int b = blockIdx + 1; b < numBlocks; b++)
        {
            if (bits[b] != 0) // Skip empty blocks instantly
            {
                int tz = __builtin_ctzll((unsigned long long)bits[b]);
                int result = b * BITS_PER_BLOCK + tz;
                return (result < n) ? result : n;
            }
        }

        return n;

#else 
        for(int j = i + 1; j < n; j++)
            if(Get(bits, j))
                return j;

        return n;
#endif
    }

    /** A quantum representation of a Pattern. */
    class Cell
    {
    public:
        Cell(BitsetBlock *state, int maxEntropy) : _State(state), _MaxEntropy(maxEntropy) { }

        struct Iterator 
        { 
            int Idx;
            const Cell *Ref;
            inline void operator++() { Idx = FindFirst(Ref->_State, Ref->_MaxEntropy, Idx); } 
            inline bool operator!=(const Iterator &other) const { return this->Idx != other.Idx; }
        };
        Iterator begin() const { return { FindFirst(_State, _MaxEntropy), this }; }
        Iterator end()   const { return { _MaxEntropy, this }; }

        inline void Clear(int i) { ClearOne(_State, i); }
        inline int  State() const { return IsCollapsed() ? FindFirst(_State, _MaxEntropy) : STATE_INVALID; }
        inline bool HasState(int i) const { return Get(_State, i); }
        inline int  Entropy()       const { return Count(_State, _MaxEntropy); }
        inline int  MaxEntropy()    const { return _MaxEntropy; }
        inline bool IsCollapsed()   const { return Entropy() == 1; }
        void Collapse(std::mt19937 &rand, const std::vector<float> weights);
        void Collapse(int state);
    private:
        BitsetBlock *_State;
        int _MaxEntropy;
    };

    class ConstCell
    {
    public:
        ConstCell(const BitsetBlock *state, int maxEntropy) : _State(state), _MaxEntropy(maxEntropy) { }

        inline int State() const { return IsCollapsed() ? FindFirst(_State, _MaxEntropy) : STATE_INVALID; }
        inline bool HasState(int i) const { return Get(_State, i); }
        inline int  Entropy()       const { return Count(_State, _MaxEntropy); }
        inline int  MaxEntropy()    const { return _MaxEntropy; }
        inline bool IsCollapsed()   const { return Entropy() == 1; }
    private:
        const BitsetBlock *_State;
        int _MaxEntropy;
    };
    void Cell::Collapse(std::mt19937 &rand, const std::vector<float> weights)
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
        ClearAll(_State, _MaxEntropy);
        Set(_State, state);
    }
    void Cell::Collapse(int state)
    {
        ClearAll(_State, _MaxEntropy);
        Set(_State, state);
    }

/* ----------------------------------- CHUNK ------------------------------------- */

    Chunk::Chunk(int len, int maxEntropy) : 
        _StateData(CEIL_DIV(maxEntropy, BITS_PER_BLOCK) * len * len, BitsetBlock(-1)), 
        _Length(len), _MaxEntropy(maxEntropy), _StatePadding(CEIL_DIV(maxEntropy, BITS_PER_BLOCK)) { }

    Cell Chunk::Get(int x, int y) { return Get(y * _Length + x); }
    Cell Chunk::Get(int idx) { return { &_StateData[idx * _StatePadding], _MaxEntropy }; }
    std::vector<int> Chunk::Build() const 
    {
        int size = Size();
        std::vector<int> out(size, STATE_INVALID);

        for (int i = 0; i < size; i++) 
        {
            const ConstCell p = { &_StateData[_StatePadding * i], _MaxEntropy };
            out[i] = p.State();
        }
        return out;
    }

/* ----------------------------------- GENERATION ---------------------------------- */

    static inline int iOpposite(int dir) { return (dir + 2) % 4; }
    //static inline int iIdx(int x, int y, int chunkLen) { return y * chunkLen + x; }
    
    static inline int iX(int idx, int chunkLen) { return idx % chunkLen; }
    static inline int iY(int idx, int chunkLen) { return idx / chunkLen; }

    static inline int iNextX(int x, int dir) { return x + (2 - dir) % 2; }
    static inline int iNextY(int y, int dir) { return y + (1 - dir) % 2; }

    struct iCellID
    {
        int ID;
        class Cell Cell;
        
        inline iCellID(int id, class Cell cell) : ID(id), Cell(cell) { } 
        inline bool operator<(const iCellID &other) const
        { 
            int thisEntropy  = Cell.Entropy();
            int otherEntropy = other.Cell.Entropy(); 
            if(thisEntropy == otherEntropy)
                return ID < other.ID;
            return thisEntropy < otherEntropy;
        }
    };

    static ErrCode iPropagate(std::set<iCellID> &uncollapsed, std::array<Chunk*, 9> &chunks, const RuleSet &ruleset, int centerX, int centerY)
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

            Chunk &fromChunk = *chunks[wave.Chunk];
            Chunk &toChunk   = *chunks[toChunkId];

            Cell from = fromChunk.Get(wave.FromX, wave.FromY);
            Cell to   = toChunk  .Get(toX, toY);
            
            // Current pattern in the uncollapsed set
            iCellID p = { toX + toY * chunkLen, to };

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
                p.Cell = to;
                uncollapsed.emplace(p);
            }
            // else it has collapsed

            for (int d = 0; d < 4; d++)
                if (d != iOpposite(wave.Dir))
                    stack.emplace_back(toChunkId, toX, toY, d);
        }

        return ERR_OK;
    }

    ErrCode Generate(std::array<Chunk*, 9>& chunks, const RuleSet &ruleset, unsigned int seed)
    {
        if(chunks[4] == nullptr)
            return ERR_INVALID_INPUT;
        
        auto &pivot = *chunks[4];
        std::mt19937 rand(seed ? seed : std::chrono::high_resolution_clock::now().time_since_epoch().count());
        int chunkLen = pivot.Length();
        
        std::set<iCellID> uncollapsed;
        for(int i = 0; i < pivot.Size(); i++)
        {
            Cell p = pivot.Get(i);
            if(p.Entropy() > 1)
                uncollapsed.emplace(i, p);
            else if(p.Entropy() == 0)
                return ERR_ZERO_ENTROPY;
        }

        while(!uncollapsed.empty())
        {
            iCellID p = *uncollapsed.begin();
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

    bool Validate(const std::vector<int> &chunks, const RuleSet &ruleset, int len)
    {
        if(len * len != (int)chunks.size())
        {
            PWFC_LOG_ERROR("Invalid chunk array size!");
            return false;
        }
        
        bool valid = true;
        for(int i = 0; i < (int)chunks.size(); i++)
        {
            int xi = i % len;
            int yi = i / len;
            int si = chunks[i];
            
            if(si == STATE_INVALID)
            {
                valid = false;
                PWFC_LOG_ERROR("Found a cell with invalid entropy at (%d,%d)", xi, yi);
            }
            for(int d = 0; d < 4; d++)
            {
                int xj = iNextX(xi, d);
                int yj = iNextY(yi, d);
                if(xj >= len || yj >= len || xj < 0 || yj < 0)
                    continue;
                int sj = chunks[xj + yj * len];
                if(sj == STATE_INVALID)
                {
                    PWFC_LOG_ERROR("Found a cell with invalid entropy at (%d,%d)", xj, yj);
                    valid = false;
                }
                else if(!ruleset.Allows(si, sj, d))
                {
                    PWFC_LOG_ERROR("Ruleset does not allow for %d-%d over direction %d at (%d,%d)", si, sj, d, xi, xj);
                    valid = false;
                }
            }
        }

        return valid;
    }
}
