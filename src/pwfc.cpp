#include <cmath>
#include <ctime>
#include <format>
#include <fstream>
#include <ios>
#include <mpi.h>
#include <iostream>
#include <vector>

#include "debug.hpp"
#include "pwfc.hpp"



static inline int iChunkIdx(int i, int j) { return i * 3 + j; }
static inline int iRank(int x, int y, int dim) 
{
    if(x < 0 || y < 0 || x >= dim || y >= dim)
        return MPI_PROC_NULL;
    return y * dim + x;
}


static PWFC::QChunk *iShareChunks(int dim, int rank, int chunkIdx, int pivotX, int pivotY, int offsetX, int offsetY, 
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
    PWFC::QChunk* extChunk = &extChunks[iChunkIdx(extDY + 1, extDX + 1)];
    PWFC::QChunk* intChunk = &intChunks[iChunkIdx(intDY + 1, intDX + 1)];

    MPI_Status status;
    MPI_Sendrecv(
        out ? extChunk : intChunk, sizeof(PWFC::QChunk), MPI_BYTE, out ? extRank : intRank, 0,
        out ? intChunk : extChunk, sizeof(PWFC::QChunk), MPI_BYTE, out ? intRank : extRank, 0,
        MPI_COMM_WORLD, &status);

    if (extRank != MPI_PROC_NULL)
        PWFC_LOG_INFO("{} external chunk ({},{}) {} node {}", out ? "Sent back" : "Received", extDX + 1, extDY + 1, out ? "to" : "from", extRank);
    if (intRank != MPI_PROC_NULL)
        PWFC_LOG_INFO("{} internal chunk ({},{}) {} node {}", out ? "Received back" : "Sent", intDX + 1, intDY + 1, out ? "from" : "to", intRank);

    PWFC::QChunk* sharedChunk = out ? intChunk : extChunk;
    return extRank == MPI_PROC_NULL ? nullptr : sharedChunk;
}

int main(int argc, char** argv) 
{
    // 1) Initialize the MPI environment
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    bool saveRun = true;
  
    int dim = (int)std::round(std::sqrt((double) size));
    if (dim * dim != size) 
    {
        if (rank == 0) 
        {
            std::println("Error: This application must be run with a perfect sqared processes count.");
            std::println("Usage: mpirun -np <num_proc> <exec_path>");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int rankX = rank % dim;
    int rankY = rank / dim;

    std::vector<PWFC::QChunk> localChunksData(9); // The 3x3 cells of local chunks
    std::vector<PWFC::QChunk> sharedChunksData(9); // The 3x3 buffer of chunks to load from neighbor nodes
    std::array<PWFC::QChunk*, 9> localChunksRef  = { }; // The 3x3 array of chunk references to send to PWFC

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
                chunk = iShareChunks(dim, rank, chunkIdx, pivotX, pivotY, offsetX, offsetY, localChunksData, sharedChunksData, false);
            else chunk = &localChunksData[iChunkIdx(offsetY, offsetX)];

            localChunksRef[chunkIdx] = chunk;
        }
        
        // Execute the algorithm
        int seed = (int)time(nullptr); 
        int err = PWFC::Generate(localChunksRef, seed + rank + round * 199);
        if (err)
        {
            PWFC_LOG_ERROR("WFC Generation failed!");
        }

        if (saveRun)
        {
            std::string fileName = std::format("runs/run_{}_node_{}.txt", round, rank);
            std::fstream file(fileName, std::ios::out);
            PWFC::PrintQChunk(*localChunksRef[4], file);
        }


        // Send back and recieve updated chunks
        for (int chunkIdx = 0; chunkIdx < 9; chunkIdx++) 
        {
            if(chunkIdx == 4) // No need to exchange from central chunk
                continue;

            int offsetX = pivotX + chunkIdx % 3 - 1;
            int offsetY = pivotY + chunkIdx / 3 - 1;
            if(offsetX < 0 || offsetY < 0 || offsetX >= 3 || offsetY >= 3) // activate send and recieve 
                iShareChunks(dim, rank, chunkIdx, pivotX, pivotY, offsetX, offsetY, localChunksData, sharedChunksData, true);
        }
    }

    int sendingBytes = 9 * sizeof(PWFC::QChunk);

    // Final global buffer
    std::vector<PWFC::QChunk> globalChunksData;
    if (rank == 0) 
        globalChunksData.resize(9 * size); 

    // Gather all local 3x3 sections into the global map on Rank 0
    MPI_Gather(
        localChunksData.data(), sendingBytes, MPI_BYTE,  // Send buffer
        globalChunksData.data(), sendingBytes, MPI_BYTE, // Receive buffer (only relevant on root)
        0,                                                     // Root node receiving the data
        MPI_COMM_WORLD                                         // Communicator
    );

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

                buf.push_back(&globalChunksData[i * 3 + j + rankX * 9 + 9 * dim * rankY]);
            }
        }
        
        bool validated = PWFC::Validate(buf, 3 * dim);
        if (!validated)
            PWFC_LOG_ERROR("There was an error in the final check :(");
        else 
            PWFC_LOG_INFO("There was no error in the final check :)");
    }


    // Clean up and finalize
    MPI_Finalize();
    return 0;
}
