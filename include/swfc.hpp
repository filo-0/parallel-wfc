#pragma once 

#include "pwfc.hpp"

namespace PWFC 
{
    
    /**
     * Saves the input map to a ppm image file.
     * @param result The map flattened in row major to a vector.
     * @param dim The dimension of the compute graph side.
     * @length The number of cells on the side of the map.
     * @filepath The path to the output file to write.
     */ 
    void SaveToImage(const QChunk &chunk, const std::string &filepath);
}
