#pragma once

#include <array>
#include <bit>
#include <bitset>
#include <random>

#include "consts.hpp"

namespace PWFC 
{    
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

    /** A quantum representation of a Pattern. */
    class QPattern
    {
    public:
        QPattern() { _State.set(); }

        struct Iterator 
        { 
            int Idx;
            const std::bitset<MAX_ENTROPY> *Ref;
            inline void operator++() { Idx = _FindFirst(*Ref, Idx); } 
            inline bool operator!=(const Iterator &other) const { return this->Idx != other.Idx; }
        };
        Iterator begin() const { return { _FindFirst(_State), &_State }; }
        Iterator end()   const { return { (int)_State.size(), &_State }; }

        inline void Clear(int i)          { _State.reset(i); }
        inline Pattern State()         const { return Entropy() ? PATTERNS[_FindFirst(_State)] : Pattern(PATTERN_FILLED); }
        inline Pattern State(int i)    const { return PATTERNS[i]; }
        inline bool HasState(int i) const { return _State.test(i); }
        inline int  Entropy()       const { return _State.count(); }
        inline bool IsCollapsed()   const { return Entropy() == 1; }
        void Collapse(std::mt19937 &rand);
        void Collapse(int state);
    private:
        std::bitset<MAX_ENTROPY> _State;
    
        inline static int _FindFirst(std::bitset<MAX_ENTROPY> bitset, int i = -1)
        {
            for(int j = i + 1; j < (int)bitset.size(); j++)
                if(bitset[j])
                    return j;
            return bitset.size();
        }
    };
    
    /** A chunk of QPatterns. */
    class QChunk 
    {
    public:
        QChunk(int len) : _Data(len * len), _Length(len) { }

        inline QPattern &Get(int x, int y) { return _Data[y * _Length + x]; }
        inline QPattern &Get(int idx) { return _Data[idx]; }
        inline const QPattern &Get(int x, int y) const { return _Data[y * _Length + x]; }
        inline const QPattern &Get(int idx) const { return _Data[idx]; }
        
        inline std::vector<QPattern>::iterator begin() { return _Data.begin(); }
        inline std::vector<QPattern>::iterator end() { return _Data.end(); }

        inline const QPattern *Data() const { return _Data.data(); }
        inline       QPattern *Data()       { return _Data.data(); }

        inline int Length() const { return _Length; }
        inline int Size()   const { return _Length * _Length; }
    private:
        std::vector<QPattern> _Data;
        int _Length;
    };
}
