#include <cmath>
#include <ctime>
#include <format>
#include <fstream>
#include <ios>
#include <mpi.h>
#include <iostream>
#include <vector>

#include "pwfc.hpp"

static inline int iChunkIdx(int i, int j) { return i * 3 + j; }
static inline int iRank(int x, int y, int dim) 
{
    if(x < 0 || y < 0 || x >= dim || y >= dim)
        return MPI_PROC_NULL;
    return y * dim + x;
}
int main(int argc, char** argv) 
{
    // 1) Initialize the MPI environment
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Enforce exactly 4 computing nodes
    //
    int dim = (int)std::round(std::sqrt((double) size));
    if (size != 16) 
    {
        if (rank == 0) 
        {
            std::cerr << "Error: This application must be run with exactly 4 processes." << std::endl;
            std::cerr << "Usage: mpirun -np 4 ./your_executable" << std::endl;
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int rankX = rank % dim;
    int rankY = rank / dim;

    // 2) Allocate local memory for the 3x3 chunk section (9 chunks total per node)
    std::vector<PWFC::QChunk> localChunksData(9);
    std::vector<PWFC::QChunk> sharedChunksData(9);
    std::array<PWFC::QChunk*, 9> localChunksRef;

    for(int run = 0; run < 9; run++)
    {
        int pivotX = run % 3;
        int pivotY = run / 3;

        // Gather neighbor chunks 
        for (int chunkIdx = 0; chunkIdx < 9; chunkIdx++) 
        {
            int offsetX = pivotX + chunkIdx % 3 - 1;
            int offsetY = pivotY + chunkIdx / 3 - 1;

            PWFC::QChunk *chunk = nullptr;
            if(offsetX < 0 || offsetY < 0 || offsetX >= 3 || offsetY >= 3) // activate send and recieve 
            {
                int recvDY = chunkIdx / 3 - 1;
                int recvDX = chunkIdx % 3 - 1;

                int rankDX = (offsetX >= 3) ? 1 : (offsetX < 0) ? -1 : 0;
                int rankDY = (offsetY >= 3) ? 1 : (offsetY < 0) ? -1 : 0;

                int sendDX;
                int sendDY;
                if(rankDX == 0 && rankDY != 0)
                {
                    sendDX = recvDX + pivotX - 1;
                    sendDY = -recvDY;
                }
                else if(rankDY == 0 && rankDX != 0)
                {
                    sendDX = -recvDX;
                    sendDY = recvDY + pivotY - 1;
                }
                else
                {
                    sendDX = -recvDX;
                    sendDY = -recvDY;
                }

                int from = iRank(rankX + rankDX, rankY + rankDY, dim);
                int to   = iRank(rankX - rankDX, rankY - rankDY, dim); // opposite side 
                
                // send 
                
                PWFC::QChunk *recvChunk = &sharedChunksData[iChunkIdx(recvDY + 1,  recvDX + 1)];
                PWFC::QChunk *sendChunk = &localChunksData[iChunkIdx(sendDY + 1, sendDX + 1)];

                if(from == MPI_PROC_NULL) 
                    chunk = nullptr;
                else 
                    chunk = recvChunk;
                
                MPI_Status status;
                MPI_Sendrecv(
                        sendChunk, sizeof(PWFC::QChunk), MPI_BYTE, to  , 0,
                        recvChunk, sizeof(PWFC::QChunk), MPI_BYTE, from, 0,
                        MPI_COMM_WORLD, &status);

                if(to != MPI_PROC_NULL)
                    std::cout << "Node " << rank << " sent chunk [" << sendDX + 1 << "," << sendDY + 1 << "] to node " << to << "\n";
                if (from != MPI_PROC_NULL) 
                    std::cout << "Node " << rank << " received chunk [" << recvDX + 1 << "," << recvDY + 1 << "] from node " << status.MPI_SOURCE << "\n";
            }
            else chunk = &localChunksData[iChunkIdx(offsetY, offsetX)];

            if(chunkIdx == 4 && rank == 0)
            {
                std::cout << "Running wfc on node " << rank << ", pivot chunk " << iChunkIdx(offsetY, offsetX) << "\n";
            }

            localChunksRef[chunkIdx] = chunk;
        }
        
        // Execute the algorithm
        int seed = time(nullptr); 
        int err = PWFC::Generate(localChunksRef, seed + rank + run * 199);
        if(err)
            std::cout << "Wave function collapse encounterd an error!\n";

        std::string fileName = std::format("runs/run_{}_node_{}.txt", run, rank);
        std::fstream file(fileName, std::ios::out);
        PWFC::PrintQChunk(*localChunksRef[4], file);

        // Send back and recieve updated chunks
        for (int chunkIdx = 0; chunkIdx < 9; chunkIdx++) 
        {
            if(chunkIdx == 4) 
                continue;

            int offsetX = pivotX + chunkIdx % 3 - 1;
            int offsetY = pivotY + chunkIdx / 3 - 1;
            if(offsetX < 0 || offsetY < 0 || offsetX >= 3 || offsetY >= 3) // activate send and recieve 
            {
                int sendDY = chunkIdx / 3 - 1;
                int sendDX = chunkIdx % 3 - 1;

                int rankDX = (offsetX >= 3) ? 1 : (offsetX < 0) ? -1 : 0;
                int rankDY = (offsetY >= 3) ? 1 : (offsetY < 0) ? -1 : 0;

                int recvDX;
                int recvDY;
                if(rankDX == 0 && rankDY != 0)
                {
                    recvDX = sendDX + pivotX - 1;
                    recvDY = -sendDY; 
                }
                else if(rankDY == 0 && rankDX != 0)
                {
                    recvDX = -sendDX;
                    recvDY = sendDY + pivotY - 1;
                }
                else
                {
                    recvDX = -sendDX;
                    recvDY = -sendDY;
                }

                int to   = iRank(rankX + rankDX, rankY + rankDY, dim);
                int from = iRank(rankX - rankDX, rankY - rankDY, dim); // opposite side 
                
                // send 
                
                PWFC::QChunk *recvChunk = &localChunksData[iChunkIdx(recvDY + 1,  recvDX + 1)];
                PWFC::QChunk *sendChunk = &sharedChunksData[iChunkIdx(sendDY + 1, sendDX + 1)];

                MPI_Status status;
                MPI_Sendrecv(
                        sendChunk, sizeof(PWFC::QChunk), MPI_BYTE, to  , 0,
                        recvChunk, sizeof(PWFC::QChunk), MPI_BYTE, from, 0,
                        MPI_COMM_WORLD, &status);

                if(to != MPI_PROC_NULL)
                    std::cout << "Node " << rank << " sent back chunk [" << sendDX + 1 << "," << sendDY + 1 << "] to node " << to << "\n";
                if (from != MPI_PROC_NULL) 
                    std::cout << "Node " << rank << " received back chunk [" << recvDX + 1 << "," << recvDY + 1 << "] from node " << status.MPI_SOURCE << "\n";
            }
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        std::cout << std::endl;

        MPI_Barrier(MPI_COMM_WORLD);
    }

    size_t sendingBytes = 9 * sizeof(PWFC::QChunk);

    // 4) Prepare memory on the root node to receive the 9 * 4 = 36 chunks
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
        std::cout << "Successfully generated and gathered " << 9 * size << " chunks from " << size << " nodes." << std::endl;
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
        if(!validated)
            std::cout << "There was an error in the final check :(\n";
        else 
            std::cout << "There was no error in the final check :)\n";
    }


    // Clean up and finalize
    MPI_Finalize();
    return 0;

    /*std::vector<PWFC::QChunk> localChunksData(25);
    std::array<PWFC::QChunk*, 9> localChunkRef;
    for(int r = 0; r < 9; r++)
    {
        int px = r % 3 + 1;
        int py = r / 3 + 1;
        for(int c = 0; c < 9; c++)
        {
            int i = py + c / 3 - 1;
            int j = px + c % 3 - 1;

            PWFC::QChunk *chunk = nullptr;
            chunk = &localChunksData[i * 5 + j];
            
            localChunkRef[c] = chunk;
        }

        int err = PWFC::Generate(localChunkRef, 1234 + r);
        if(err)
            std::cout << "Error generating chunk " << px + py * 5 << "\n";
    }

    PWFC::PrintQChunk(localChunksData[17]);
    PWFC::PrintQChunk(localChunksData[12]);
    PWFC::PrintQChunk(localChunksData[7]);*/ 

}
