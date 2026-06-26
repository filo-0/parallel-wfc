#pragma once

#include <cstring>
#include <vector>


namespace PWFC 
{    
    class Cell;
    using BitsetBlock = size_t;
    constexpr int BITS_PER_BLOCK = sizeof(BitsetBlock) * 8;

    enum ErrCode : int 
    {
        ERR_OK,            // There wasn't any problem.
        ERR_ZERO_ENTROPY,  // The generation has encountered a conflict.
        ERR_OUT_OF_CHUNK,  // The generation has gone outside of the chunk borders.
        ERR_INVALID_INPUT, // The generation received an invalid input.
        ERR_BAD_FORMAT,    // The parser has encountered a wrong format.
    };
    constexpr int STATE_INVALID = -1;
    
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
        RuleSet() = default;
        inline RuleSet(const std::vector<Rule> &rules, const std::vector<float> &weights, int n) 
            : _Weights(weights), _Rules(n * n * 4, false), _MaxEntropy(n)
        {
            for(const Rule &rule : rules)
                _Rules[rule.AId * n * 4 + rule.BId * 4 + rule.A2B] = true;
        }

        inline bool Allows(int aid, int bid, int a2b) const { return _Rules[aid * _MaxEntropy * 4 + bid * 4 + a2b]; }
        inline float Weight(int id) const { return _Weights[id]; }
        inline const std::vector<float> &Weights() const { return _Weights; }
        inline int MaxEntropy() const { return _MaxEntropy; }
    private:
        std::vector<float> _Weights;
        std::vector<bool>  _Rules;
        int _MaxEntropy;
    };
    
    
    /** A chunk of QPatterns. */
    class Chunk 
    {
    public:
        Chunk(int len, int maxEntropy);

        Cell Get(int x, int y);
        Cell Get(int idx);

        inline const void *StateData() const { return _StateData.data(); }
        inline       void *StateData()       { return _StateData.data(); }

        inline size_t StateDataSize() const { return _StateData.size() * sizeof(BitsetBlock); }

        inline int Length() const { return _Length; }
        inline int Size()   const { return _Length * _Length; }

        std::vector<int> Build() const;
    private:
        std::vector<BitsetBlock> _StateData;
        int _Length;
        int _MaxEntropy;
        int _StatePadding;
    };
}
