/*
 * Sequential implementation of the Wave Function Collapse Algorithm.
 */

#include "swfc.hpp"
#include "consts.hpp"
#include "types.hpp"
#include "debug.hpp"

#include <array>
#include <ctime>
#include <fstream>

void iParseArgs(std::string &outpath, unsigned int &seed, int &length, char **argv, int argc);
int main(int argc, char **argv)
{
    std::string outpath = "out.ppm";
    unsigned int seed = time(nullptr);
    int length = 16;
    iParseArgs(outpath, seed, length, argv, argc);

    PWFC::QChunk chunk( length);
    std::array<PWFC::QChunk*, 9> chunksref = {
        nullptr, nullptr, nullptr,
        nullptr, &chunk, nullptr,
        nullptr, nullptr, nullptr
    };

    struct timespec T0, T1;
    clock_gettime( CLOCK_REALTIME, &T0);

    PWFC::Generate(chunksref, seed);
    
    clock_gettime( CLOCK_REALTIME, &T1);
    double elapsed = (double)(T1.tv_sec - T0.tv_sec) + (double)(T1.tv_nsec - T0.tv_nsec) / 1e9;
    PWFC_LOG_INFO("Total Execution time: %.3fs", elapsed);

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
        int length = chunk.Length();
        int width  = length * PATTERN_LENGTH;
        int height = length * PATTERN_LENGTH;

        std::ofstream out(filepath);
        if (!out.is_open()) 
        {
            PWFC_LOG_ERROR("Failed to open %s for writing!", filepath.c_str());
            return;
        }

        out << "P3\n" << width << " " << height << "\n255\n";

        for (int py = 0; py < height; ++py) 
        {
            for (int px = 0; px < width; ++px) 
            {
                int px_out = px / PATTERN_LENGTH;
                int py_out = py / PATTERN_LENGTH;
                int px_in  = px % PATTERN_LENGTH;
                int py_in  = py % PATTERN_LENGTH;

                const PWFC::QPattern& q = chunk.Get(px_out, py_out);
                
                if (!q.IsCollapsed()) 
                {
                    // Fallback color for uncollapsed/error cells (Red)
                    out << "255 0 0 "; 
                } 
                else 
                {
                    // From your PrintQChunk: state.Get() returns true for ' ' and false for '#'
                    bool isBlack = q.State().Get(px_in, py_in); 
                    if (isBlack) 
                        out << "0 0 0 ";       // Black pixel
                    else 
                        out << "255 255 255 "; // White pixel
                }
            }
            out << "\n"; // Newline after every row of pixels
        }

        out.close();
        PWFC_LOG_INFO("Successfully saved map to %s", filepath.c_str());
    }
}
