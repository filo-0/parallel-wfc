#include "maze16/maze16.hpp"
#include "pwfc/types.hpp"
#include "pwfc/debug.hpp"

#include <array>
#include <cmath>
#include "stb_image_write.h"

namespace Maze16 
{
    /** This value determines the smaller edge length of a chunk. */
    constexpr int PATTERN_LENGTH = 3;

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
    PWFC::RuleSet BuildRuleset(unsigned int seed) 
    {
        std::vector<float> weights(   Maze16::MAX_ENTROPY, 1);
        std::vector<PWFC::Rule> rules;
        for(int i = 0; i < Maze16::MAX_ENTROPY; i++)
        {
            for (int j = 0; j < Maze16::MAX_ENTROPY; j++) 
            {
                for(int d = 0; d < 4; d++)
                {
                    if(CanConnect(Maze16::PATTERNS[i], Maze16::PATTERNS[j], d))
                    {
                        rules.emplace_back(i, j, d);
                    }
                }
            }
        }
        return PWFC::RuleSet(rules, weights, Maze16::MAX_ENTROPY, seed);
    }

    void SaveToImage(const std::vector<int> &map, int length, const std::string &filepath)
    {
        if(filepath == "")
            return;
        
        int mapLength = (int)std::round(std::sqrt((double)map.size()));
        PWFC_ASSERT(mapLength * mapLength == (int)map.size(), "Invalid map size!");
        PWFC_ASSERT(mapLength >= length, "Invalid image length!");

        int width  = length * Maze16::PATTERN_LENGTH;
        int height = length * Maze16::PATTERN_LENGTH;

        std::vector<unsigned char> pixels(width * height * 3);
        
        int i = 0;
        for (int py = 0; py < height; ++py) 
        {
            for (int px = 0; px < width; ++px) 
            {
                int px_out = px / Maze16::PATTERN_LENGTH;
                int py_out = py / Maze16::PATTERN_LENGTH;
                int px_in  = px % Maze16::PATTERN_LENGTH;
                int py_in  = py % Maze16::PATTERN_LENGTH;

                int state = map[py_out * mapLength + px_out];
                
                if (state == PWFC::STATE_INVALID) 
                {
                    // Fallback color for uncollapsed/error cells (Red)
                    pixels[i + 0] = 255;
                    pixels[i + 1] = 0;
                    pixels[i + 2] = 0;
                } 
                else 
                {
                    // From your PrintQChunk: state.Get() returns true for ' ' and false for '#'
                    bool isBlack = Maze16::PATTERNS[state].Get(px_in, py_in); 
                    if (isBlack) 
                    {
                        pixels[i + 0] = 0;
                        pixels[i + 1] = 0;
                        pixels[i + 2] = 0;
                    } // Black pixel
                    else 
                    {
                        pixels[i + 0] = 255;
                        pixels[i + 1] = 255;
                        pixels[i + 2] = 255;
                    }
                }
                i += 3;
            }
        }

        int success = stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
        
        if(success)
            PWFC_LOG_INFO("Successfully saved map to %s", filepath.c_str());
        else 
            PWFC_LOG_ERROR("Could not write PNG file to %s", filepath.c_str());
    } 
}
