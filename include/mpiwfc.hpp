#pragma once

#include <string>
#include <vector>
#include "pwfc.hpp"


namespace PWFC 
{
    int InitMPI(int &rank, int &dim, int &size, int argc, char **argv);
    int RunMPI(std::vector<QChunk> &result, int rank, int dim, int size);
    void SaveToImage(const std::vector<QChunk> &result, int dim, const std::string &filepath);
    void QuitMPI();
}
