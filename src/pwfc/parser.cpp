#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "pwfc/types.hpp"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include "pwfc/pwfc.hpp"
#include "pwfc/debug.hpp"

namespace PWFC 
{
    struct iParseTile 
    {
        int   Id;
        char  Symmetry;
        float Weight; 
        inline iParseTile() = default;
        inline iParseTile(int id, char symmetry, float weight) : Id(id), Symmetry(symmetry), Weight(weight) { }
    };

    struct iParseRule 
    {
        std::string Left;
        std::string Right;
    };

    template <class T1, class T2>
    struct iPairHash {
        std::size_t operator () (const std::pair<T1, T2>& p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
    static const std::vector<int> &iGetSocket(char symmetry, int rot)
    {
        static std::unordered_map<std::pair<char, int>, std::vector<int>, iPairHash<char, int>> _Sockets = 
        {
            { { 'X', 0 }, { 0, 1, 2, 3 } },

            { { 'I', 0 }, { 0, 2 } },
            { { 'I', 1 }, { 0, 2 } },

            { { '\\', 0 }, { 0, 2 } },
            { { '\\', 1 }, { 0, 2 } },
            
            { { 'T', 0 }, { 0, 1, 2 } },
            { { 'T', 1 }, { 0, 1, 3 } },
            { { 'T', 2 }, { 0, 2, 3 } },
            { { 'T', 3 }, { 0 } },
            
            { { 'L', 0 }, { 0, 1 } },
            { { 'L', 1 }, { 0, 3 } },
            { { 'L', 2 }, { 0, 1 } },
            { { 'L', 3 }, { 0, 3 } },
        };
        
        auto p = _Sockets.find({ symmetry, rot });
        PWFC_ASSERT(p != _Sockets.end(), "Symmetry and rotation were not found!");
        return p->second;
    }
    static int iGetId(const std::unordered_map<std::string, iParseTile> &parsedTiles, const std::string &name, int rotState = 0)
    {
        auto p = parsedTiles.find(rotState == 0 ? name : name + " " + std::to_string(rotState));
        if(p == parsedTiles.end())
            return PWFC::STATE_INVALID;
        return p->second.Id;
    }
    static int iGetNextRotationState(char symmetry, int currentState) 
    {
        switch (symmetry) 
        {
            case 'X': 
                return 0; 
            case 'I': 
            case '\\': 
                return (currentState + 1) % 2; 
            case 'T': 
            case 'L': 
                return (currentState + 1) % 4; 
            default: 
                return 0;
        }
    }
    static inline int iRotRight(int d) { return (d + 1) % 4; }
    ErrCode ParseString(RuleSet &ruleset, const std::string &input)
    {
        rapidjson::Document doc;
        std::unordered_map<std::string, iParseTile> parsedTiles;
        std::vector<iParseRule> parsedRules;
        

        // Parse the string and check for syntax errors
        if (doc.Parse(input.c_str()).HasParseError()) 
        {
            PWFC_LOG_ERROR("%s [offset=%zu].", rapidjson::GetParseError_En(doc.GetParseError()), doc.GetErrorOffset());
            return ERR_BAD_FORMAT;
        }

        // Ensure the root is an object
        if (!doc.IsObject()) 
        {
            PWFC_LOG_ERROR("Root of JSON is not an object.");
            return ERR_BAD_FORMAT;
        }

        // Parse Tiles
        if (doc.HasMember("tiles") && doc["tiles"].IsArray()) 
        {
            const rapidjson::Value& tilesArray = doc["tiles"];

            // Iterate through the array of tile objects
            for (rapidjson::SizeType i = 0; i < tilesArray.Size(); i++) 
            {
                const rapidjson::Value& tileObj = tilesArray[i];

                if (tileObj.IsObject()) 
                {
                    std::string name;
                    std::string symmetry;
                    float weight;

                    // Safely extract values, checking types
                    if (tileObj.HasMember("name") && tileObj["name"].IsString())
                        name = tileObj["name"].GetString();
                    
                    if (tileObj.HasMember("symmetry") && tileObj["symmetry"].IsString()) 
                        symmetry = tileObj["symmetry"].GetString();
                    
                    if (tileObj.HasMember("weight") && tileObj["weight"].IsNumber())
                        weight = tileObj["weight"].GetFloat();
                    
                    if(symmetry.size() > 1 || symmetry.size() == 0)
                    {
                        PWFC_LOG_ERROR("Invalid symmetry value! [symmetry=%s]", symmetry.c_str());
                        return ERR_BAD_FORMAT;
                    }
                    
                    switch (symmetry[0]) 
                    {
                    case 'X':
                        parsedTiles.try_emplace(name, (int)parsedTiles.size(), symmetry[0], weight);
                        break;
                    case '\\':
                    case 'I':
                        parsedTiles.try_emplace(name, (int)parsedTiles.size(), symmetry[0], weight);
                        parsedTiles.try_emplace(name + " 1", (int)parsedTiles.size(), symmetry[0], weight);
                        break;
                    case 'T':
                    case 'L':
                        parsedTiles.try_emplace(name, (int)parsedTiles.size(), symmetry[0], weight);
                        parsedTiles.try_emplace(name + " 1", (int)parsedTiles.size(), symmetry[0], weight);
                        parsedTiles.try_emplace(name + " 2", (int)parsedTiles.size(), symmetry[0], weight);
                        parsedTiles.try_emplace(name + " 3", (int)parsedTiles.size(), symmetry[0], weight);
                        break;
                    default:
                        PWFC_LOG_ERROR("Invalid symmetry value! [symmetry=%s]", symmetry.c_str());
                        return ERR_BAD_FORMAT;
                    }
                }
            }
        }

        // Parse Neighbors (Adjacency Rules)
        if ((doc.HasMember("neighbors") && doc["neighbors"].IsArray()) || (doc.HasMember("rules") && doc["rules"].IsArray())) 
        {
            const rapidjson::Value& neighborsArray = doc["neighbors"];

            for (rapidjson::SizeType i = 0; i < neighborsArray.Size(); i++) 
            {
                const rapidjson::Value& ruleObj = neighborsArray[i];

                if (ruleObj.IsObject()) {
                    iParseRule newRule;

                    if (ruleObj.HasMember("left") && ruleObj["left"].IsString())
                        newRule.Left =  ruleObj["left"].GetString();
                    if (ruleObj.HasMember("right") && ruleObj["right"].IsString())
                        newRule.Right = ruleObj["right"].GetString();

                    parsedRules.push_back(newRule);
                }
            }
        }
        
        PWFC_LOG_INFO("Successfully parsed WFC Data!");
        for (const auto&[name, tile] : parsedTiles)
            PWFC_LOG_INFO("Tile=%s | Id=%d | Symmetry=%c | Weight=%f", name.c_str(), tile.Id, tile.Symmetry, tile.Weight);

        std::cout << "\n--- Adjacency Rules ---\n";
        for (const auto& rule : parsedRules)
            PWFC_LOG_INFO("Tile %s <---> %s", rule.Left.c_str(), rule.Right.c_str());
        
        std::vector<Rule> rules;
        std::vector<float> weights(parsedTiles.size());

        for (const iParseRule &rule : parsedRules) 
        {
            const iParseTile &left  = parsedTiles[rule.Left];
            const iParseTile &right = parsedTiles[rule.Right];

            // Start both tiles at their base rotation state (0 degrees)
            int leftState = 0;
            int rightState = 0;

            // Loop through the 4 spatial rotations (0=Right, 1=Down, 2=Left, 3=Up)
            for (int d : iGetSocket(right.Symmetry, rightState)) 
            {
                rules.emplace_back(iGetId(parsedTiles, rule.Left, leftState), iGetId(parsedTiles, rule.Right, rightState), (1 + d) % 4);

                // Now, rotate both tiles internally for the next spatial direction
                leftState  = iGetNextRotationState(left.Symmetry, leftState);
                rightState = iGetNextRotationState(right.Symmetry, rightState);
            }
        }
        for(const auto&[name, tile] : parsedTiles)
            weights[tile.Id] = tile.Weight;
        
        ruleset = RuleSet(rules, weights, (int)weights.size());
        return ERR_OK;
    }
}
