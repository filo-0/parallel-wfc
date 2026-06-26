#pragma once

#include <array>
#include <string>
#include <vector>

#include "types.hpp"

namespace PWFC 
{
    /**
     * Parses a string and constructs a ruleset objects.
     * @returns A ERR_BAD_FORMAT error if the format is incorrect, ERR_OK otherwise.
     */ 
    ErrCode ParseString(RuleSet &ruleset, const std::string &input);

    /**
     * Generates the central chunk (chunks[4]) using the Wave Function Collapse algorithm.
     * @param chunks The 3x3 matrix of chunks flattened in row major to a 9 elements array.
     * @param seed The seed of the random generator.
     */
    ErrCode Generate(std::array<Chunk*, 9> &chunks, const RuleSet &ruleset, unsigned int seed);

    /**
     * Checks whether the input map is coherent.
     * chunks The map chunk matrix flattened in row major to a vector.
     * len The map side length.
     */ 
    bool Validate(const std::vector<int> &map, const RuleSet &ruleset, int len);
}
