#include <array>

namespace MazeRule 
{
    /** This value determines the smaller edge length of a chunk. */
    constexpr int PATTERN_LENGTH = 3;
    constexpr int PATTERN_COUNT = 16;
    constexpr int MAX_ENTROPY = PATTERN_COUNT;

    constexpr bool TILE_PATTERNS[][PATTERN_LENGTH][PATTERN_LENGTH]
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
        for (int j = 0; j < PATTERN_LENGTH; j++) 
            for (int k = 0; k < PATTERN_LENGTH; k++) 
                val |= TILE_PATTERNS[i][j][k] << (j * PATTERN_LENGTH + k);
        
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
    class Pattern 
    {
    public:
        inline constexpr Pattern() : _Raw{0} { }
        inline constexpr Pattern(PatternMask mask) : _Raw{(int)mask} { }
        inline constexpr operator int() const { return _Raw; }
        inline constexpr bool Get(int x, int y) const { return _Raw & (1 << (y * PATTERN_LENGTH + x)); }
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
    static inline int Opposite(int dir) { return (dir + 2) % 4; }
    static inline int NextX(int x, int dir) { return x + (2 - dir) % 2; }
    static inline int NextY(int y, int dir) { return y + (1 - dir) % 2; }
    static inline bool CanConnect(Pattern a, Pattern b, int a2b)
    {
        int b2a = Opposite(a2b);

        bool aOpen = a.Get(NextX(1, a2b), NextY(1, a2b));
        bool bOpen = b.Get(NextX(1, b2a), NextY(1, b2a));

        return aOpen == bOpen;
    }
}
