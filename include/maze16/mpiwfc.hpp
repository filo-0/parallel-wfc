#pragma once

#include <vector>

#include "pwfc/types.hpp"

namespace Maze16
{
    /**
     * Initializes the MPI environment.
     * @param rank The output rank of the current compute node, this value is an output.
     * @param dim The output dimension of the compute graph side.
     * @param size The output number of total compute nodes.
     * @param argc The number of application input arguments.
     * @param argv The application input arguments.
     * @return 1 if there is an error initializing the node or the squared dim is not equal to size, 0 otherwise.
     */ 
    int InitMPI(int &rank, int &dim, int &size, int argc, char **argv);

    /**
     * Runs the MPI Wave Function Collapse on a compute node.
     * @param result The output vector where the generated chunks are collected.
     * @param rank The rank of the current compute node.
     * @param dim The dimension of the compute graph side.
     * @param size The number of compute nodes.
     * @param length The number of cells on the side of the final map.
     * @param seed The seed of the number generator.
     * @return An non zero value if there is an error, 0 otherwise.
     */ 
    int RunMPI(std::vector<PWFC::Chunk> &result, int rank, int dim, int size, int length, unsigned int seed);

    /** Terminates the MPI environment. */ 
    void QuitMPI();
}
