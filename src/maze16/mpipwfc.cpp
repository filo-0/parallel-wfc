#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <mpi.h>
#include <string>
#include <string_view>
#include <vector>
#include "stb_image_write.h"


#include "pwfc/debug.hpp"
#include "pwfc/types.hpp"
#include "pwfc/pwfc.hpp"

#include "maze16/maze16.hpp"
#include "maze16/mpiwfc.hpp"

void iParseArgs(std::string &outpath, unsigned int &seed, int &length, char **argv, int argc);
int main(int argc, char** argv) 
{
    int dim, size, rank;
    
    int length = 128;
    unsigned int seed = time(nullptr);
    std::string outpath = "out.png";
    iParseArgs(outpath, seed, length, argv, argc);

    int error = Maze16::InitMPI(rank, dim, size, argc, argv);
    if (error) 
    {
        if (rank == 0) 
        {
            printf("Error: This application must be run with a perfect sqared processes count.\n");
            printf("Usage: mpirun -np <num_proc> <exec_path>\n");
        }
        MPI_Abort(MPI_COMM_WORLD, error);
    }

    std::vector<PWFC::Chunk> result;
    error = Maze16::RunMPI(result, rank, dim, size, length, seed);
    if(error)
    {
        PWFC_LOG_ERROR("A node encountered an error generating the map!");
        MPI_Abort(MPI_COMM_WORLD, error);
    }


    if (rank == 0 && !result.empty()) // Build, validate and output file
    {
        PWFC_LOG_INFO("Successfully generated and gathered %d chunks from %d nodes.", 9 * size, size);
        
        int chunkLength = result[0].Length();
        int mapLength = chunkLength * dim * 3;
        std::vector<std::vector<int>> buffer(result.size());
        for(size_t i = 0; i < result.size(); i++) 
            buffer[i] = result[i].Build();


        std::vector<int> map(mapLength * mapLength);
        for(int p = 0; p < (int)map.size(); p++)
        {
            int mapX = p % mapLength;
            int mapY = p / mapLength;

            int chunkX = mapX % chunkLength;
            int chunkY = mapY % chunkLength;
            
            int rankX = (mapX / chunkLength) / 3;
            int rankY = (mapY / chunkLength) / 3;

            int j = (mapX / chunkLength) % 3;
            int i = (mapY / chunkLength) % 3;
            
            map[p] = buffer[(rankY * dim + rankX) * 9 + i * 3 + j][chunkY * chunkLength + chunkX];
        }
        
        
        bool validated = PWFC::Validate(map, Maze16::BuildRuleset(), mapLength);
        if (!validated)
            PWFC_LOG_ERROR("There was an error in the final check :(");
        else 
            PWFC_LOG_INFO("There was no error in the final check :)");

        Maze16::SaveToImage(map, length, outpath);
    }


    Maze16::QuitMPI();
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

namespace Maze16 
{
    constexpr int MIN_CHUNK_LENGTH = 16;
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
    static PWFC::Chunk *iShareChunks(double &syncTime, int dim, int rank, int chunkIdx, int pivotX, int pivotY, int offsetX, int offsetY, 
            std::vector<PWFC::Chunk> &intChunks, std::vector<PWFC::Chunk> &extChunks, bool out)
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
        PWFC::Chunk &extChunk = extChunks[iChunkIdx(extDY + 1, extDX + 1)];
        PWFC::Chunk &intChunk = intChunks[iChunkIdx(intDY + 1, intDX + 1)];
        int chunkDataSize = intChunk.StateDataSize();

        MPI_Status status;
        double syncT0 = MPI_Wtime();
        MPI_Sendrecv(
                out ? extChunk.StateData() : intChunk.StateData(), chunkDataSize, MPI_BYTE, out ? extRank : intRank, 0,
                out ? intChunk.StateData() : extChunk.StateData(), chunkDataSize, MPI_BYTE, out ? intRank : extRank, 0,
                MPI_COMM_WORLD, &status);
        double syncT1 = MPI_Wtime();
        syncTime += syncT1 - syncT0;

        if (extRank != MPI_PROC_NULL)
            PWFC_LOG_INFO("[NODE_%d] %s external chunk (%d,%d) %s node %d", rank, out ? "Sent back" : "Received", extDX + 1, extDY + 1, out ? "to" : "from", extRank);
        if (intRank != MPI_PROC_NULL)
            PWFC_LOG_INFO("[NODE_%d] %s internal chunk (%d,%d) %s node %d", rank, out ? "Received back" : "Sent", intDX + 1, intDY + 1, out ? "from" : "to", intRank);

        PWFC::Chunk* sharedChunk = out ? &intChunk : &extChunk;
        return extRank == MPI_PROC_NULL ? nullptr : sharedChunk;
    }

    
    int RunMPI(std::vector<PWFC::Chunk> &result, int rank, int dim, int size, int length, unsigned int seed)
    {
        double execTime = 0.0;
        double syncTime = 0.0;
        double gatherTime = 0.0;
        auto execT0 = MPI_Wtime();

        int chunkLength = std::max(iCeilDiv(length, dim * 3), Maze16::MIN_CHUNK_LENGTH);
        std::vector<PWFC::Chunk> localChunksData; // The 3x3 cells of local chunks
        std::vector<PWFC::Chunk> sharedChunksData; // The 3x3 buffer of chunks to load from neighbor nodes
        std::array<PWFC::Chunk*, 9> localChunksRef = { }; // The 3x3 array of chunk references to send to PWFC
        
        localChunksData.reserve(9); 
        sharedChunksData.reserve(9);
        for(int i = 0; i < 9; i++)
        {
            localChunksData.emplace_back(chunkLength, Maze16::MAX_ENTROPY);
            sharedChunksData.emplace_back(chunkLength, Maze16::MAX_ENTROPY);
        }

        PWFC_LOG_INFO("[NODE_%d] Starting WFC with chunks of dimension %dx%d", rank, chunkLength, chunkLength);
        PWFC_LOG_INFO("[NODE_%d] Computation will produce a map %f%% bigger", rank, (float)(chunkLength * dim * 3 - length) / length * 100);

        PWFC::RuleSet ruleset = Maze16::BuildRuleset();
        for(int round = 0; round < 9; round++)
        {
            int pivotX = round % 3;
            int pivotY = round / 3;

            // Select the chunks to use during this round
            for (int chunkIdx = 0; chunkIdx < 9; chunkIdx++) 
            {
                int offsetX = pivotX + chunkIdx % 3 - 1;
                int offsetY = pivotY + chunkIdx / 3 - 1;

                PWFC::Chunk *chunk = nullptr;
                if (offsetX < 0 || offsetY < 0 || offsetX >= 3 || offsetY >= 3) // activate send and recieve 
                    chunk = iShareChunks(syncTime, dim, rank, chunkIdx, pivotX, pivotY, offsetX, offsetY, localChunksData, sharedChunksData, false);
                else chunk = &localChunksData[iChunkIdx(offsetY, offsetX)];

                localChunksRef[chunkIdx] = chunk;
            }

            // Execute the algorithm
            PWFC::ErrCode err = Generate(localChunksRef, ruleset, seed + round * 33 + rank * 17);
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

        double gatherT0 = MPI_Wtime();
        int sendingBytes = localChunksData[0].StateDataSize();
        int stateBlockCount = sendingBytes / sizeof(PWFC::BitsetBlock);
        std::vector<PWFC::BitsetBlock> localBuffer(sendingBytes / sizeof(PWFC::BitsetBlock) * size); 
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
                        (PWFC::BitsetBlock*)result[r * 9 + i].StateData()
                    );
                }
            }
        }

        // Rank 0 now possesses the entire map and can process it
        double execT1 = MPI_Wtime();
        
        execTime = execT1 - execT0;
        gatherTime = execT1 - gatherT0;
        
        PWFC_LOG_INFO("[NODE_%d] Total Execution time: %.3lfs", rank, execTime);
        PWFC_LOG_INFO("[NODE_%d] Total Sync time: %.3lfs", rank, syncTime);
        PWFC_LOG_INFO("[NODE_%d] Total Gather time: %.3lfs", rank, gatherTime);

        return 0;
    }
}
