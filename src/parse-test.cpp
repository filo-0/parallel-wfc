#include "pwfc/types.hpp"
#include <string>
#include <pwfc/pwfc.hpp>

int main() {
    // Our mock JSON string
    std::string jsonPayload = R"({
        "tiles": [
            {"name": "grass", "symmetry": "X", "weight": 1.0},
            {"name": "road_straight", "symmetry": "I", "weight": 0.5}
        ],
        "neighbors": [
            {"left": "grass", "right": "grass"},
            {"left": "grass", "right": "road_straight"}
        ]
    })";
    
    PWFC::RuleSet rs;
    PWFC::ParseString(rs, jsonPayload);

    return 0;
}

