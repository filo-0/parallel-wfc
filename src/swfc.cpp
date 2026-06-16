/*
 * Sequential implementation of the Wave Function Collapse Algorithm.
 */

#include "swfc.hpp"
#include "types.hpp"
#include "debug.hpp"

#include "maze-rules.hpp"

#include <array>
#include <chrono>
#include <ctime>
#include <vector>
#include "stb_image_write.h"

void iParseArgs(std::string &outpath, unsigned int &seed, int &length, char **argv, int argc);
int main(int argc, char **argv)
{
    std::string outpath = "";
    unsigned int seed = time(nullptr);
    int length = 16;
    iParseArgs(outpath, seed, length, argv, argc);

    PWFC::QChunk chunk( length, MazeRule::MAX_ENTROPY);
    std::array<PWFC::QChunk*, 9> chunksref = {
        nullptr, nullptr, nullptr,
        nullptr, &chunk , nullptr,
        nullptr, nullptr, nullptr
    };
    
    std::vector<float> weights(   MazeRule::MAX_ENTROPY, 1);
    std::vector<PWFC::Rule> rules;
    for(int i = 0; i < MazeRule::MAX_ENTROPY; i++)
    {
        for (int j = 0; j < MazeRule::MAX_ENTROPY; j++) 
        {
            for(int d = 0; d < 4; d++)
            {
                if(CanConnect(MazeRule::PATTERNS[i], MazeRule::PATTERNS[j], d))
                {
                    rules.emplace_back(i, j, d);
                }
            }
        }
    }
    PWFC::RuleSet ruleset(rules, weights, MazeRule::MAX_ENTROPY, seed);
    auto t0 = std::chrono::high_resolution_clock::now();
    PWFC::Generate(chunksref, ruleset);
    auto t1 = std::chrono::high_resolution_clock::now();
    PWFC_LOG_INFO("Execution time: %f", std::chrono::duration<float>(t1 - t0).count());

    PWFC::SaveToImage(chunk, outpath);
}
void iParseArgs(std::string &outpath, unsigned int &seed, int &length, char **argv, int argc)
{
    for(int i = 1; i < argc; i++)
    {
        std::string_view opt = argv[i];
        if(opt == "-o" || opt == "--output")
        {
            if(i + 1 >= argc)
                break;
            i++;
            outpath = argv[i];
        }
        else if(opt == "-s" || opt == "--seed")
        {
            if(i + 1 >= argc)
                break;
            i++;
            seed = std::stoul(argv[i]);
        }
        else if(opt == "-l" || opt == "--length")
        {
            if(i + 1 >= argc)
                break;
            i++;
            length = std::stoi(argv[i]);
        }
    }
}

namespace PWFC 
{
    void SaveToImage(const QChunk &chunk, const std::string &filepath)
    {
        if(filepath == "")
            return;

        int length = chunk.Length();
        int width  = length * MazeRule::PATTERN_LENGTH;
        int height = length * MazeRule::PATTERN_LENGTH;

        std::vector<unsigned char> pixels(width * height * 3);
        
        int i = 0;
        for (int py = 0; py < height; ++py) 
        {
            for (int px = 0; px < width; ++px) 
            {
                int px_out = px / MazeRule::PATTERN_LENGTH;
                int py_out = py / MazeRule::PATTERN_LENGTH;
                int px_in  = px % MazeRule::PATTERN_LENGTH;
                int py_in  = py % MazeRule::PATTERN_LENGTH;

                const PWFC::QPattern& q = chunk.Get(px_out, py_out);
                
                if (!q.IsCollapsed()) 
                {
                    // Fallback color for uncollapsed/error cells (Red)
                    pixels[i + 0] = 255;
                    pixels[i + 1] = 0;
                    pixels[i + 2] = 0;
                } 
                else 
                {
                    // From your PrintQChunk: state.Get() returns true for ' ' and false for '#'
                    bool isBlack = MazeRule::PATTERNS[q.State()].Get(px_in, py_in); 
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
