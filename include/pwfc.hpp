#pragma once

#include <array>
#include <bit>
#include <bitset>
#include <cstdint>
#include <iostream>
#include <random>

namespace PWFC 
{
    constexpr int CHUNK_LEN  = 16;
    constexpr int CHUNK_SIZE = CHUNK_LEN * CHUNK_LEN;

    constexpr int PATTERN_SIZE = 3;
    constexpr int PATTERN_COUNT = 16;
    constexpr int MAX_ENTROPY = PATTERN_COUNT;

    constexpr bool TILE_PATTERNS[][PATTERN_SIZE][PATTERN_SIZE]
    {
        {
            { 0, 0, 0},
            { 0, 0, 0},
            { 0, 0, 0},
        },    
        {     
            { 0, 1, 0},
            { 1, 1, 1},
            { 0, 1, 0},
        },    
        {
            { 0, 1, 0},
            { 1, 1, 1},
            { 0, 0, 0},
        },    
        {     
            { 0, 1, 0},
            { 0, 1, 1},
            { 0, 1, 0},
        },    
        {     
            { 0, 0, 0},
            { 1, 1, 1},
            { 0, 1, 0},
        },
        {     
            { 0, 1, 0},
            { 1, 1, 0},
            { 0, 1, 0},
        },    
        {     
            { 0, 0, 0},
            { 1, 1, 1},
            { 0, 0, 0},
        },    
        {
            { 0, 1, 0},
            { 0, 1, 0},
            { 0, 1, 0},
        },    
        {     
            { 0, 1, 0},
            { 0, 1, 1},
            { 0, 0, 0},
        },
        /* WALLS */
        {     
            { 0, 0, 0},
            { 0, 1, 1},
            { 0, 1, 0},
        },    
        {
            { 0, 0, 0},
            { 1, 1, 0},
            { 0, 1, 0},
        },    
        {     
            { 0, 1, 0},
            { 1, 1, 0},
            { 0, 0, 0},
        },    
        {     
            { 0, 1, 0},
            { 0, 1, 0},
            { 0, 0, 0},
        },
        {     
            { 0, 0, 0},
            { 0, 1, 1},
            { 0, 0, 0},
        },    
        {
            { 0, 0, 0},
            { 0, 1, 0},
            { 0, 1, 0},
        },    
        {     
            { 0, 0, 0},
            { 1, 1, 0},
            { 0, 0, 0},
        }    
    };

    constexpr int PARSE_PATTERN(int i)
    {
        int val = 0;
        for (int j = 0; j < PATTERN_SIZE; j++) 
            for (int k = 0; k < PATTERN_SIZE; k++) 
                val |= TILE_PATTERNS[i][j][k] << (j * PATTERN_SIZE + k);
        
        return val;
    }
    enum PatternMask : int
    {
        PATTERN_FILLED          = PARSE_PATTERN(0),
        PATTERN_CROSS           = PARSE_PATTERN(1),
        PATTERN_FORK_UP         = PARSE_PATTERN(2),
        PATTERN_FORK_RIGHT      = PARSE_PATTERN(3),
        PATTERN_FORK_DOWN       = PARSE_PATTERN(4),
        PATTERN_FORK_LEFT       = PARSE_PATTERN(5),
        PATTERN_LINE_HORIZONTAL = PARSE_PATTERN(6),
        PATTERN_LINE_VERTICAL   = PARSE_PATTERN(7),
        PATTERN_CURVE_UP        = PARSE_PATTERN(8),
        PATTERN_CURVE_RIGHT     = PARSE_PATTERN(9),
        PATTERN_CURVE_DOWN      = PARSE_PATTERN(10),
        PATTERN_CURVE_LEFT      = PARSE_PATTERN(11),
        PATTERN_BLIND_UP        = PARSE_PATTERN(12),
        PATTERN_BLIND_RIGHT     = PARSE_PATTERN(13),
        PATTERN_BLIND_DOWN      = PARSE_PATTERN(14),
        PATTERN_BLIND_LEFT      = PARSE_PATTERN(15),
    };
    enum Direction : int 
    {
        DIR_UP    = 0,
        DIR_RIGHT = 1, 
        DIR_DOWN  = 2,
        DIR_LEFT  = 3 
    };
    enum ErrCode : int 
    {
        ERR_OK,
        ERR_ZERO_ENTROPY,
        ERR_OUT_OF_CHUNK,
        ERR_INVALID_INPUT
    };
    
    class Pattern 
    {
    public:
        inline constexpr Pattern() : _Raw{0} { }
        inline constexpr Pattern(PatternMask mask) : _Raw{(int)mask} { }
        inline constexpr operator int() const { return _Raw; }
        inline constexpr bool Get(int x, int y) { return _Raw & (1 << (y * PATTERN_SIZE + x)); }
    private:
        int _Raw;
    };
    
    constexpr static std::array<Pattern, MAX_ENTROPY> PATTERNS = 
    {
        PATTERN_FILLED,
        PATTERN_CROSS,
        PATTERN_FORK_UP,
        PATTERN_FORK_RIGHT,
        PATTERN_FORK_DOWN,
        PATTERN_FORK_LEFT,
        PATTERN_LINE_HORIZONTAL,
        PATTERN_LINE_VERTICAL,
        PATTERN_CURVE_UP,
        PATTERN_CURVE_RIGHT,
        PATTERN_CURVE_DOWN,
        PATTERN_CURVE_LEFT,
        PATTERN_BLIND_UP,
        PATTERN_BLIND_RIGHT,
        PATTERN_BLIND_DOWN,
        PATTERN_BLIND_LEFT,
    };

    inline int FindFirst(std::bitset<MAX_ENTROPY> bitset, int i = -1)
    {
        static_assert(MAX_ENTROPY == sizeof(uint16_t) * 8, "MAX_ENTROPY is no more 16!");

        int count = std::countr_zero((uint16_t)(bitset.to_ulong() >> (i + 1)));
        return count == 16 ? count : i + count + 1;
    }
    class QPattern
    {
    public:
        QPattern() { _State.set(); }

        struct Iterator 
        { 
            int Idx;
            const std::bitset<MAX_ENTROPY> *Ref;
            inline void operator++() { Idx = FindFirst(*Ref, Idx); } 
            inline bool operator!=(const Iterator &other) const { return this->Idx != other.Idx; }
        };
        Iterator begin() const { return { FindFirst(_State), &_State }; }
        Iterator end()   const { return { (int)_State.size(), &_State }; }

        inline void Clear(int i)          { _State.reset(i); }
        inline Pattern State()         const { return Entropy() ? PATTERNS[FindFirst(_State)] : Pattern(PATTERN_FILLED); }
        inline Pattern State(int i)    const { return PATTERNS[i]; }
        inline bool HasState(int i) const { return _State.test(i); }
        inline int  Entropy()       const { return _State.count(); }
        inline bool IsCollapsed()   const { return Entropy() == 1; }
        void Collapse(std::mt19937 &rand);
        void Collapse(int state);
    private:
        std::bitset<MAX_ENTROPY> _State;
    };
    class QChunk 
    {
    public:
        inline QPattern &Get(int x, int y) { return _Data[y * CHUNK_LEN + x]; }
        inline QPattern &Get(int idx) { return _Data[idx]; }
        inline const QPattern &Get(int x, int y) const { return _Data[y * CHUNK_LEN + x]; }
        inline const QPattern &Get(int idx) const { return _Data[idx]; }
        
        inline std::array<QPattern, CHUNK_SIZE>::iterator begin() { return _Data.begin(); }
        inline std::array<QPattern, CHUNK_SIZE>::iterator end() { return _Data.end(); }
    private:
        std::array<QPattern, CHUNK_SIZE> _Data = { };
    };
    
    ErrCode Generate(std::array<QChunk*, 9> &chunks, unsigned int seed);
    bool Validate(const std::vector<QChunk*> &chunks, int len);
    void PrintQChunk(QChunk& chunk, std::ostream &stream = std::cout);
}
