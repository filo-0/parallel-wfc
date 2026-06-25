#include "pwfc/types.hpp"
#include <vector>
#include <string>

namespace Maze16 
{
    constexpr int MAX_ENTROPY = 16;
    PWFC::RuleSet BuildRuleset(unsigned int seed);
    void SaveToImage(const std::vector<int> &map, int length, const std::string &outpath);
}
