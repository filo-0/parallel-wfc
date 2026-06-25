/*
 * Sequential implementation of the Wave Function Collapse Algorithm.
 */

#include "maze16/maze16.hpp"

#include "pwfc/types.hpp"
#include "pwfc/pwfc.hpp"
#include "pwfc/debug.hpp"


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

    PWFC::Chunk chunk( length, MazeRule::MAX_ENTROPY);
    std::array<PWFC::Chunk*, 9> chunksref = {
        nullptr, nullptr, nullptr,
        nullptr, &chunk , nullptr,
        nullptr, nullptr, nullptr
    };
    
    PWFC::RuleSet ruleset = MazeRule::BuildRuleset(seed);
    auto t0 = std::chrono::high_resolution_clock::now();
    PWFC::Generate(chunksref, ruleset);
    auto t1 = std::chrono::high_resolution_clock::now();
    PWFC_LOG_INFO("Execution time: %f", std::chrono::duration<float>(t1 - t0).count());
    
    std::vector<int> map = chunk.Build();
    bool valid = PWFC::Validate(map, ruleset, length);
    if (!valid)
        PWFC_LOG_ERROR("There was an error in the final check :(");
    else 
    {
        PWFC_LOG_INFO("There was no error in the final check :)");
        MazeRule::SaveToImage(map, length, outpath);
    }   
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
