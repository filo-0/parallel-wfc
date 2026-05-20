#include <chrono>
#include <cmath>
#include <ctime>
#include <format>
#include <fstream>
#include <mpi.h>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "debug.hpp"
#include "pwfc.hpp"
#include "mpiwfc.hpp"

void iParseArgs(std::string &outpath, unsigned int &seed, int &length, char **argv, int argc);
int main(int argc, char** argv) 
{
    bool saveRun = true;
    int dim, size, rank;
    
    int length = 128;
    unsigned int seed = time(nullptr);
    std::string outpath = "out.ppm";
    iParseArgs(outpath, seed, length, argv, argc);

    int error = PWFC::InitMPI(rank, dim, size, argc, argv);
    if (error) 
    {
        if (rank == 0) 
        {
            std::println("Error: This application must be run with a perfect sqared processes count.");
            std::println("Usage: mpirun -np <num_proc> <exec_path>");
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
        int chunkDataSize = intChunk.Size() * sizeof(PWFC::QPattern);

        MPI_Status status;
        auto syncT0 = std::chrono::high_resolution_clock::now();
        MPI_Sendrecv(
                out ? extChunk.Data() : intChunk.Data(), chunkDataSize, MPI_BYTE, out ? extRank : intRank, 0,
                out ? intChunk.Data() : extChunk.Data(), chunkDataSize, MPI_BYTE, out ? intRank : extRank, 0,
                MPI_COMM_WORLD, &status);
        auto syncT1 = std::chrono::high_resolution_clock::now();
        syncTime += std::chrono::duration<float>(syncT1 - syncT0).count();

        if (extRank != MPI_PROC_NULL)
            PWFC_LOG_INFO("[NODE_{}] {} external chunk ({},{}) {} node {}", rank, out ? "Sent back" : "Received", extDX + 1, extDY + 1, out ? "to" : "from", extRank);
        if (intRank != MPI_PROC_NULL)
            PWFC_LOG_INFO("[NODE_{}] {} internal chunk ({},{}) {} node {}", rank, out ? "Received back" : "Sent", intDX + 1, intDY + 1, out ? "from" : "to", intRank);

        PWFC::QChunk* sharedChunk = out ? &intChunk : &extChunk;
        return extRank == MPI_PROC_NULL ? nullptr : sharedChunk;
    }
    int RunMPI(std::vector<QChunk> &result, int rank, int dim, int size, int length, unsigned int seed)
    {
        float execTime = 0;
        float syncTime = 0;
        auto execT0 = std::chrono::high_resolution_clock::now();

        int chunkLength = std::max(iCeilDiv(length, dim * 3), MIN_CHUNK_LENGTH);
        int chunkSize = chunkLength * chunkLength;
        std::vector<PWFC::QChunk> localChunksData(9, chunkLength); // The 3x3 cells of local chunks
        std::vector<PWFC::QChunk> sharedChunksData(9, chunkLength); // The 3x3 buffer of chunks to load from neighbor nodes
        std::array<PWFC::QChunk*, 9> localChunksRef = { }; // The 3x3 array of chunk references to send to PWFC

        PWFC_LOG_INFO("[NODE_{}] Starting WFC with chunks of dimension {}x{}", rank, chunkLength, chunkLength);
        PWFC_LOG_INFO("[NODE_{}] Computation will produce a map {}\% bigger", rank, (float)(chunkLength * dim * 3 - length) / length * 100);

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
            ErrCode err = Generate(localChunksRef, seed + rank + round * 33);
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

        PWFC_LOG_INFO("[NODE_{}] Successfully computed the 3x3 local chunks!", rank);

        // Final global buffer
        if (rank == 0) 
            result.resize(9 * size, chunkLength); 

        // Gather all local 3x3 sections into the global map on Rank 0
        int sendingBytes = chunkSize * sizeof(QPattern);
        std::vector<QPattern> localBuffer(chunkSize * size); 
        for(int i = 0; i < 9; i++)
        {
            MPI_Gather(
                localChunksData[i].Data(), sendingBytes, MPI_BYTE, // Send buffer
                localBuffer.data(), sendingBytes, MPI_BYTE,     // Receive buffer (only relevant on root)
                0,                                              // Root node receiving the data
                MPI_COMM_WORLD                                  // Communicator
            );

            // copy to the result buffer 
            if(rank == 0)
            {
                for(int r = 0; r < size; r++)
                {
                    std::copy(
                        localBuffer.begin() + r * chunkSize, 
                        localBuffer.begin() + (r + 1) * chunkSize,
                        result[r * 9 + i].begin()
                    );
                }
            }
        }

        // Rank 0 now possesses the entire map and can process/print it
        if (rank == 0) 
        {
            PWFC_LOG_INFO("Successfully generated and gathered {} chunks from {} nodes.", 9 * size, size);
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

            bool validated = PWFC::Validate(buf, 3 * dim);
            if (!validated)
                PWFC_LOG_ERROR("There was an error in the final check :(");
            else 
                PWFC_LOG_INFO("There was no error in the final check :)");
        }
        
        auto execT1 = std::chrono::high_resolution_clock::now();
        execTime = std::chrono::duration<float>(execT1 - execT0).count();

        PWFC_LOG_INFO("[NODE_{}] Total Execution time: {:.3f}s", rank, execTime);
        PWFC_LOG_INFO("[NODE_{}] Total Sync time: {:.3f}s", rank, syncTime);

        return 0;
    }

    void SaveToImage(const std::vector<PWFC::QChunk>& result, int dim, int length, const std::string& filepath) 
    {
        if(result.empty())
            return;


        int chunkLen = result[0].Length();
        int chunksPerSide = 3 * dim;
        int width  = length * PATTERN_SIZE;
        int height = length * PATTERN_SIZE;
        PWFC_ASSERT(chunksPerSide * chunkLen >= length, "Invalid chunk size, too small! [real={}, asked={}]", chunksPerSide * chunkLen, length);

        std::ofstream out(filepath);
        if (!out.is_open()) 
        {
            PWFC_LOG_ERROR("Failed to open {} for writing!", filepath);
            return;
        }

        out << "P3\n" << width << " " << height << "\n255\n";

        for (int py = 0; py < height; ++py) 
        {
            for (int px = 0; px < width; ++px) 
            {
                // Global chunk coordinates
                int cx = px / (chunkLen * PATTERN_SIZE);
                int cy = py / (chunkLen * PATTERN_SIZE);
                
                // Pattern coordinates inside the current chunk
                int p_x = (px / PATTERN_SIZE) % chunkLen;
                int p_y = (py / PATTERN_SIZE) % chunkLen;
                
                // Pixel coordinates inside the pattern
                int px_in = px % PATTERN_SIZE;
                int py_in = py % PATTERN_SIZE;
                
                // Calculate which Rank / Node this chunk came from
                // (Mirroring the collection logic in mpipwfc.cpp)
                int i = cy % 3;
                int j = cx % 3;
                int rankX = cx / 3;
                int rankY = cy / 3;
                
                // Index in the flattened MPI result vector
                int chunk_index = i * 3 + j + rankX * 9 + 9 * dim * rankY;
                
                // Extract the pixel state
                const PWFC::QChunk& chunk = result[chunk_index];
                
                // Assuming chunk.Get() accepts a linear index y * CHUNK_LEN + x
                const PWFC::QPattern& q = chunk.Get(p_y * chunkLen + p_x);
                
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
        PWFC_LOG_INFO("Successfully saved map to {}", filepath);
    }

}
