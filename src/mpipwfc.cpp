#include <chrono>
#include <cmath>
#include <ctime>
#include <mpi.h>
#include <string>
#include <string_view>
#include <vector>
#include "stb_image_write.h"

#include "maze-rules.hpp"

#include "debug.hpp"
#include "pwfc.hpp"
#include "mpiwfc.hpp"
#include "types.hpp"

void iParseArgs(std::string &outpath, unsigned int &seed, int &length, char **argv, int argc);
int main(int argc, char** argv) 
{
    int dim, size, rank;
    
    int length = 128;
    unsigned int seed = time(nullptr);
    std::string outpath = "";
    iParseArgs(outpath, seed, length, argv, argc);

    int error = PWFC::InitMPI(rank, dim, size, argc, argv);
    if (error) 
    {
        if (rank == 0) 
        {
            printf("Error: This application must be run with a perfect sqared processes count.\n");
            printf("Usage: mpirun -np <num_proc> <exec_path>\n");
        }
        MPI_Abort(MPI_COMM_WORLD, error);
    }

    std::vector<PWFC::QChunk> result;
    error = PWFC::RunMPI(result, rank, dim, size, length, seed);
    if(error)
    {
        PWFC_LOG_ERROR("A node encountered an error generating the map!");
        MPI_Abort(MPI_COMM_WORLD, error);
    }
    
    if(rank == 0)
        PWFC::SaveToImage(result, dim, length, outpath);

    PWFC::QuitMPI();
    return 0;
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
    int InitMPI(int &rank, int &dim, int &size, int argc, char **argv)
    {
        int error = 0;
        error = MPI_Init(&argc, &argv);
        if(error)
            return error;

        error = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if(error)
            return error;
        error = MPI_Comm_size(MPI_COMM_WORLD, &size);
        if(error)
            return error;

        // Invalid graph size
        dim = (int)std::round(std::sqrt((double)size));
        if(dim * dim != size)
            error = 1;

        return error;
    }
    void QuitMPI() { MPI_Finalize(); }

    static inline int iCeilDiv(int x, int y) { return (x + y - 1) / y; }
    static inline int iChunkIdx(int i, int j) { return i * 3 + j; }
    static inline int iRank(int x, int y, int dim) 
    {
        if(x < 0 || y < 0 || x >= dim || y >= dim)
            return MPI_PROC_NULL;
        return y * dim + x;
    }
    static PWFC::QChunk *iShareChunks(float &syncTime, int dim, int rank, int chunkIdx, int pivotX, int pivotY, int offsetX, int offsetY, 
            std::vector<PWFC::QChunk> &intChunks, std::vector<PWFC::QChunk> &extChunks, bool out)
    {
        // The coordinate ot the current node
        int rankX = rank % dim;
        int rankY = rank / dim;

        // The direction of the receiving chunk in local coordinates
        int extDY = chunkIdx / 3 - 1;
        int extDX = chunkIdx % 3 - 1;

        // The direction to the node we recieve from
        int rankDX = (offsetX >= 3) ? 1 : (offsetX < 0) ? -1 : 0;
        int rankDY = (offsetY >= 3) ? 1 : (offsetY < 0) ? -1 : 0;

        // The direction of the sending chunk in local coordinates
        int intDX = (rankDX == 0 && rankDY != 0) ? (extDX + pivotX - 1) : -extDX;
        int intDY = (rankDY == 0 && rankDX != 0) ? (extDY + pivotY - 1) : -extDY;

        int extRank = iRank(rankX + rankDX, rankY + rankDY, dim);
        int intRank = iRank(rankX - rankDX, rankY - rankDY, dim); // opposite side 

        // send
        PWFC::QChunk &extChunk = extChunks[iChunkIdx(extDY + 1, extDX + 1)];
        PWFC::QChunk &intChunk = intChunks[iChunkIdx(intDY + 1, intDX + 1)];
        int chunkDataSize = intChunk.StateDataSize();

        MPI_Status status;
        auto syncT0 = std::chrono::high_resolution_clock::now();
        MPI_Sendrecv(
                out ? extChunk.StateData() : intChunk.StateData(), chunkDataSize, MPI_BYTE, out ? extRank : intRank, 0,
                out ? intChunk.StateData() : extChunk.StateData(), chunkDataSize, MPI_BYTE, out ? intRank : extRank, 0,
                MPI_COMM_WORLD, &status);
        auto syncT1 = std::chrono::high_resolution_clock::now();
        syncTime += std::chrono::duration<float>(syncT1 - syncT0).count();

        if (extRank != MPI_PROC_NULL)
            PWFC_LOG_INFO("[NODE_%d] %s external chunk (%d,%d) %s node %d", rank, out ? "Sent back" : "Received", extDX + 1, extDY + 1, out ? "to" : "from", extRank);
        if (intRank != MPI_PROC_NULL)
            PWFC_LOG_INFO("[NODE_%d] %s internal chunk (%d,%d) %s node %d", rank, out ? "Received back" : "Sent", intDX + 1, intDY + 1, out ? "from" : "to", intRank);

        PWFC::QChunk* sharedChunk = out ? &intChunk : &extChunk;
        return extRank == MPI_PROC_NULL ? nullptr : sharedChunk;
    }
    
    int RunMPI(std::vector<QChunk> &result, int rank, int dim, int size, int length, unsigned int seed)
    {
        float execTime = 0;
        float syncTime = 0;
        auto execT0 = std::chrono::high_resolution_clock::now();

        int chunkLength = std::max(iCeilDiv(length, dim * 3), MIN_CHUNK_LENGTH);
        std::vector<PWFC::QChunk> localChunksData; // The 3x3 cells of local chunks
        std::vector<PWFC::QChunk> sharedChunksData; // The 3x3 buffer of chunks to load from neighbor nodes
        std::array<PWFC::QChunk*, 9> localChunksRef = { }; // The 3x3 array of chunk references to send to PWFC
        
        localChunksData.reserve(9); 
        sharedChunksData.reserve(9);
        for(int i = 0; i < 9; i++)
        {
            localChunksData.emplace_back(chunkLength, MazeRule::MAX_ENTROPY);
            sharedChunksData.emplace_back(chunkLength, MazeRule::MAX_ENTROPY);
        }

        PWFC_LOG_INFO("[NODE_%d] Starting WFC with chunks of dimension %dx%d", rank, chunkLength, chunkLength);
        PWFC_LOG_INFO("[NODE_%d] Computation will produce a map %f%% bigger", rank, (float)(chunkLength * dim * 3 - length) / length * 100);

        // Create ruleset 
        std::vector<float> weights(MazeRule::MAX_ENTROPY, 1);
        std::vector<Rule> rules;
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
        RuleSet ruleset(rules, weights, MazeRule::MAX_ENTROPY, seed);

        for(int round = 0; round < 9; round++)
        {
            int pivotX = round % 3;
            int pivotY = round / 3;

            // Select the chunks to use during this round
            for (int chunkIdx = 0; chunkIdx < 9; chunkIdx++) 
            {
                int offsetX = pivotX + chunkIdx % 3 - 1;
                int offsetY = pivotY + chunkIdx / 3 - 1;

                PWFC::QChunk *chunk = nullptr;
                if (offsetX < 0 || offsetY < 0 || offsetX >= 3 || offsetY >= 3) // activate send and recieve 
                    chunk = iShareChunks(syncTime, dim, rank, chunkIdx, pivotX, pivotY, offsetX, offsetY, localChunksData, sharedChunksData, false);
                else chunk = &localChunksData[iChunkIdx(offsetY, offsetX)];

                localChunksRef[chunkIdx] = chunk;
            }

            // Execute the algorithm
            ruleset.SetSeed(ruleset.Seed() + round * 33 + rank * 17);
            ErrCode err = Generate(localChunksRef, ruleset);
            if (err)
                return err;

            // Send back and recieve updated chunks
            for (int chunkIdx = 0; chunkIdx < 9; chunkIdx++) 
            {
                if(chunkIdx == 4) // No need to exchange from central chunk
                    continue;

                int offsetX = pivotX + chunkIdx % 3 - 1;
                int offsetY = pivotY + chunkIdx / 3 - 1;
                if(offsetX < 0 || offsetY < 0 || offsetX >= 3 || offsetY >= 3) // activate send and recieve 
                    iShareChunks(syncTime, dim, rank, chunkIdx, pivotX, pivotY, offsetX, offsetY, localChunksData, sharedChunksData, true);
            }
        }

        PWFC_LOG_INFO("[NODE_%d] Successfully computed the 3x3 local chunks!", rank);

        // Final global buffer
        if (rank == 0) 
        {
            int resultSize = 9 * size;
            result.clear();
            result.reserve(resultSize);
            for (int i = 0; i < resultSize; i++)
                result.emplace_back(chunkLength, ruleset.MaxEntropy());
        }

        // Gather all local 3x3 sections into the global map on Rank 0
        int sendingBytes = localChunksData[0].StateDataSize();
        int stateBlockCount = sendingBytes / sizeof(BitsetBlock);
        std::vector<BitsetBlock> localBuffer(sendingBytes / sizeof(BitsetBlock) * size); 
        for(int i = 0; i < 9; i++)
        {
            MPI_Gather(
                localChunksData[i].StateData(), sendingBytes, MPI_BYTE, // Send buffer
                localBuffer.data(), sendingBytes, MPI_BYTE,        // Receive buffer (only relevant on root)
                0,                                                 // Root node receiving the data
                MPI_COMM_WORLD                                     // Communicator
            );

            // copy to the result buffer 
            if(rank == 0)
            {
                for(int r = 0; r < size; r++)
                {
                    std::copy(
                        localBuffer.data() + r * stateBlockCount, 
                        localBuffer.data() + (r + 1) * stateBlockCount,
                        (BitsetBlock*)result[r * 9 + i].StateData()
                    );
                }
            }
        }

        // Rank 0 now possesses the entire map and can process/print it
        if (rank == 0) 
        {
            PWFC_LOG_INFO("Successfully generated and gathered %d chunks from %d nodes.", 9 * size, size);
            std::vector<PWFC::QChunk*> buf;
            for (int y = 0; y < 3 * dim; y++) 
            {
                for (int x = 0; x < 3 * dim; x++)
                {
                    int i = y % 3;
                    int j = x % 3;
                    int rankX = x / 3;
                    int rankY = y / 3;

                    buf.push_back(&result[i * 3 + j + rankX * 9 + 9 * dim * rankY]);
                }
            }

            bool validated = PWFC::Validate(buf, ruleset, 3 * dim);
            if (!validated)
                PWFC_LOG_ERROR("There was an error in the final check :(");
            else 
                PWFC_LOG_INFO("There was no error in the final check :)");
        }
        
        auto execT1 = std::chrono::high_resolution_clock::now();
        execTime = std::chrono::duration<float>(execT1 - execT0).count();

        PWFC_LOG_INFO("[NODE_%d] Total Execution time: %.3fs", rank, execTime);
        PWFC_LOG_INFO("[NODE_%d] Total Sync time: %.3fs", rank, syncTime);

        return 0;
    }

    void SaveToImage(const std::vector<PWFC::QChunk>& result, int dim, int length, const std::string& filepath) 
    {
        if(result.empty() || filepath == "")
            return;
        
        PWFC_LOG_INFO("Building final PNG image");
        
        int chunkLen = result[0].Length();
        int chunksPerSide = 3 * dim;
        int width  = length * MazeRule::PATTERN_LENGTH;
        int height = length * MazeRule::PATTERN_LENGTH;
        PWFC_ASSERT(chunksPerSide * chunkLen >= length, "Invalid chunk size, too small! [real=%d, asked=%d]", chunksPerSide * chunkLen, length);

        std::vector<unsigned char> pixels(width * height * 3);

        for (int py = 0; py < height; ++py) 
        {
            for (int px = 0; px < width; ++px) 
            {
                // Global chunk coordinates
                int cx = px / (chunkLen * MazeRule::PATTERN_LENGTH);
                int cy = py / (chunkLen * MazeRule::PATTERN_LENGTH);

                // Pattern coordinates inside the current chunk
                int p_x = (px / MazeRule::PATTERN_LENGTH) % chunkLen;
                int p_y = (py / MazeRule::PATTERN_LENGTH) % chunkLen;

                // Pixel coordinates inside the pattern
                int px_in = px % MazeRule::PATTERN_LENGTH;
                int py_in = py % MazeRule::PATTERN_LENGTH;

                // Calculate which Rank / Node this chunk came from
                int i = cy % 3;
                int j = cx % 3;
                int rankX = cx / 3;
                int rankY = cy / 3;

                // Index in the flattened MPI result vector
                int chunk_index = i * 3 + j + rankX * 9 + 9 * dim * rankY;

                // Extract the pixel state
                const PWFC::QChunk& chunk = result[chunk_index];
                const PWFC::QPattern& q = chunk.Get(p_x, p_y);

                // Calculate the start index for this pixel in the 1D array
                int pixel_idx = (py * width + px) * 3;

                if (!q.IsCollapsed()) 
                {
                    // Fallback Red
                    pixels[pixel_idx + 0] = 255; // R
                    pixels[pixel_idx + 1] = 0;   // G
                    pixels[pixel_idx + 2] = 0;   // B
                } 
                else 
                {
                    bool isBlack = MazeRule::PATTERNS[q.State()].Get(px_in, py_in); 
                    unsigned char color = isBlack ? 0 : 255;

                    // Set RGB channels to either all 0 (black) or all 255 (white)
                    pixels[pixel_idx + 0] = color;
                    pixels[pixel_idx + 1] = color;
                    pixels[pixel_idx + 2] = color;
                }
            }
        }

        // Write image 
        int success = stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);

        if (success) 
            PWFC_LOG_INFO("Successfully saved PNG map to %s", filepath.c_str());
        else 
            PWFC_LOG_ERROR("Failed to write PNG to %s!", filepath.c_str());
    }
}
