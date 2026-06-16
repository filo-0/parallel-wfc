#pragma once

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

#include "debug.hpp"

namespace PWFC 
{    

    enum ErrCode : int 
    {
        ERR_OK,
        ERR_ZERO_ENTROPY,
        ERR_OUT_OF_CHUNK,
        ERR_INVALID_INPUT
    };

    using BitsetBlock = size_t;
    constexpr int BITS_PER_BLOCK = sizeof(BitsetBlock) * 8;
    #define CEIL_DIV(n, d) (((n) + (d) - 1) / (d))
    class Bitset 
    {
    public:
        Bitset(void *data, int n) : _Raw((BitsetBlock*)data), _Size(n) { }
        inline void Clear(int i) { _Raw[i / BITS_PER_BLOCK] &= ~(BitsetBlock(1) << (i % BITS_PER_BLOCK)); }
        inline void Clear()      { std::fill(_Raw, _Raw + CEIL_DIV(_Size, BITS_PER_BLOCK), BitsetBlock(0)); }
        inline void Set(int i)   { _Raw[i / BITS_PER_BLOCK] |= BitsetBlock(1) << (i % BITS_PER_BLOCK); }
        inline bool Get(int i) const { return _Raw[i / BITS_PER_BLOCK] & (BitsetBlock(1) << (i % BITS_PER_BLOCK)); }
        inline int  Count() const
        {
            int acc = 0;

#if defined(__GNUC__) || defined(__clang__)
            int numBlocks = CEIL_DIV(_Size, BITS_PER_BLOCK);
            for(int b = 0; b < numBlocks; b++)
                acc += __builtin_popcountll((unsigned long long)_Raw[b]);
#else 
            for(int i = 0; i < _Size; i++)
                acc += Get(i);
#endif

            return acc;
        }
        int FindFirst(int i = -1) const
        {
#if defined(__GNUC__) || defined(__clang__) 

            int startIdx = i + 1;
            if (startIdx >= _Size) return _Size;

            int blockIdx = startIdx / BITS_PER_BLOCK;
            int bitOffset = startIdx % BITS_PER_BLOCK;

            // Process the first block (which might be partially masked)
            BitsetBlock block = _Raw[blockIdx];

            // Mask out the bits before 'bitOffset'
            block &= ~((BitsetBlock(1) << bitOffset) - 1); 

            if (block != 0) 
            {
                int tz = __builtin_ctzll((unsigned long long)block);
                int result = blockIdx * BITS_PER_BLOCK + tz;
                return (result < _Size) ? result : _Size;
            }

            // Fast-scan the remaining full blocks
            int numBlocks = CEIL_DIV(_Size, BITS_PER_BLOCK);
            for(int b = blockIdx + 1; b < numBlocks; b++)
            {
                if (_Raw[b] != 0) // Skip empty blocks instantly
                {
                    int tz = __builtin_ctzll((unsigned long long)_Raw[b]);
                    int result = b * BITS_PER_BLOCK + tz;
                    return (result < _Size) ? result : _Size;
                }
            }

            return _Size;

#else 
            for(int j = i + 1; j < _Size; j++)
                if(Get(j))
                    return j;

            return _Size;
#endif
        }
        inline int Size() const { return _Size; }
    private:
        BitsetBlock *_Raw;
        int          _Size;
    };

    /** A quantum representation of a Pattern. */
    class QPattern
    {
    public:
        QPattern() : _State(nullptr, 0) { }
        QPattern(void *stateData, int maxEntropy) : _State(stateData, maxEntropy) { }

        struct Iterator 
        { 
            int Idx;
            const Bitset *Ref;
            inline void operator++() { Idx = Ref->FindFirst(Idx); } 
            inline bool operator!=(const Iterator &other) const { return this->Idx != other.Idx; }
        };
        Iterator begin() const { return { _State.FindFirst(), &_State }; }
        Iterator end()   const { return { (int)_State.Size(), &_State }; }

        inline void Clear(int i) { _State.Clear(i); }
        inline int State() const { return _State.FindFirst(); }
        inline bool HasState(int i) const { return _State.Get(i); }
        inline int  Entropy()       const { return _State.Count(); }
        inline int  MaxEntropy()    const { return _State.Size(); }
        inline bool IsCollapsed()   const { return Entropy() == 1; }
        void Collapse(std::mt19937 &rand, const std::vector<float> weights);
        void Collapse(int state);
    private:
        Bitset _State;
    };

    
    /** 
     * This struct represents an allowance rule meaning:
     * Pattern a can connect to pattern b on direction a2b.
     */
    struct Rule 
    {
        int AId;
        int BId;
        int A2B;
        Rule(int aid, int bid, int a2b) : AId(aid), BId(bid), A2B(a2b) { }
    };
    
    /**
     * A class representing the ruleset of the generation.
     */ 
    class RuleSet
    {
    public:
        inline RuleSet(const std::vector<Rule> &rules, const std::vector<float> &weights, int n, unsigned int seed) 
            : _Weights(weights), _Rules(n * n * 4, false), _Seed(seed), _MaxEntropy(n)
        {
            for(const Rule &rule : rules)
                _Rules[rule.AId * n * 4 + rule.BId * 4 + rule.A2B] = true;
        }

        inline bool Allows(int aid, int bid, int a2b) const { return _Rules[aid * _MaxEntropy * 4 + bid * 4 + a2b]; }
        inline float Weight(int id) const { return _Weights[id]; }
        inline const std::vector<float> &Weights() const { return _Weights; }
        inline int MaxEntropy() const { return _MaxEntropy; }
        inline unsigned int Seed() const { return _Seed; }

        inline RuleSet &SetSeed(unsigned int seed) { _Seed = seed; return *this; }
    private:
        std::vector<float> _Weights;
        std::vector<bool>  _Rules;
        unsigned int _Seed;
        int _MaxEntropy;
    };
    
    
    constexpr int MIN_CHUNK_LENGTH = 16;
    /** A chunk of QPatterns. */
    class QChunk 
    {
    public:
        QChunk(int len, int maxEntropy) : 
            _StateData(CEIL_DIV(maxEntropy, BITS_PER_BLOCK) * len * len, BitsetBlock(-1)), _Patterns(len * len), 
            _Length(len), _MaxEntropy(maxEntropy)
        {
            _UpdatePatterns();
        }
        
        QChunk(const QChunk &other) : 
            _StateData(other._StateData), _Patterns(other.Size()), _Length(other._Length), 
            _MaxEntropy(other._MaxEntropy)
        {
            PWFC_LOG_INFO("Copying chunks...");
            _UpdatePatterns();
        } 
        QChunk &operator=(const QChunk &other)
        {
            PWFC_LOG_INFO("Copying chunks...");
            this->_StateData  = other._StateData;
            this->_Length     = other._Length;
            this->_MaxEntropy = other._MaxEntropy;

            this->_Patterns.resize(Size());
            _UpdatePatterns();
            return *this;
        }

        inline QPattern &Get(int x, int y) { return _Patterns[y * _Length + x]; }
        inline QPattern &Get(int idx) { return _Patterns[idx]; }
        inline const QPattern &Get(int x, int y) const { return _Patterns[y * _Length + x]; }
        inline const QPattern &Get(int idx) const { return _Patterns[idx]; }
        
        inline std::vector<QPattern>::iterator begin() { return _Patterns.begin(); }
        inline std::vector<QPattern>::iterator end() { return _Patterns.end(); }

        inline const QPattern *Data() const { return _Patterns.data(); }
        inline       QPattern *Data()       { return _Patterns.data(); }
        inline const void *StateData() const { return _StateData.data(); }
        inline       void *StateData()       { return _StateData.data(); }

        inline size_t StateDataSize() const { return _StateData.size() * sizeof(BitsetBlock); }

        inline int Length() const { return _Length; }
        inline int Size()   const { return _Length * _Length; }
    private:
        std::vector<BitsetBlock> _StateData;
        std::vector<QPattern>    _Patterns;
        int _Length;
        int _MaxEntropy;

        inline void _UpdatePatterns()
        {
            int size = Size();
            int stateDataPadding = CEIL_DIV(_MaxEntropy, BITS_PER_BLOCK);
            for(int i = 0; i < size; i++)
                _Patterns[i] = QPattern(&_StateData[i * stateDataPadding], _MaxEntropy);
        }
    };
}
