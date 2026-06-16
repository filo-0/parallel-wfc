#pragma once

#include <array>

#include "types.hpp"

namespace PWFC 
{
    
    /**
     * Generates the central chunk (chunks[4]) using the Wave Function Collapse algorithm.
     * @param chunks The 3x3 matrix of chunks flattened in row major to a 9 elements array.
     * @param seed The seed of the random generator.
     */
    ErrCode Generate(std::array<QChunk*, 9> &chunks, const RuleSet &ruleset);

    /**
     * Checks whether the input map is coherent.
     * chunks The map chunk matrix flattened in row major to a vector.
     * len The map side length.
     */ 
    bool Validate(const std::vector<QChunk*> &chunks, const RuleSet &ruleset, int len);
}
