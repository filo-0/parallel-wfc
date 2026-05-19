#pragma once

namespace PWFC 
{
    /** This value determines the smaller edge length of a chunk. */
    constexpr int MIN_CHUNK_LENGTH = 16;
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
    enum ErrCode : int 
    {
        ERR_OK,
        ERR_ZERO_ENTROPY,
        ERR_OUT_OF_CHUNK,
        ERR_INVALID_INPUT
    };

}
