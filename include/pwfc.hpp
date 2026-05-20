#pragma once

#include <array>
#include <iostream>

#include "consts.hpp"
#include "types.hpp"

namespace PWFC 
{
    
    /**
     * Generates the central chunk (chunks[4]) using the Wave Function Collapse algorithm.
     * @param chunks The 3x3 matrix of chunks flattened in row major to a 9 elements array.
     * @param seed The seed of the random generator.
     */
    ErrCode Generate(std::array<QChunk*, 9> &chunks, unsigned int seed);

    /**
     * Checks whether the input map is coherent.
     * chunks The map chunk matrix flattened in row major to a vector.
     * len The map side length.
     */ 
    bool Validate(const std::vector<QChunk*> &chunks, int len);

    /**
     * Prints the chunk to a certain stream.
     * chunk The chunk to print.
     * stream The output stream, default is std::cout.
     */
    void PrintQChunk(QChunk& chunk, std::ostream &stream = std::cout);
}
