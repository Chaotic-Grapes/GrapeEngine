/* Start Header *****************************************************************/
/*!
\file   SpriteMetadata.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Implements helper functions for loading sprite metadata from JSON files. Each
JSON entry specifies trimming information, pivot, and optional collider data,
which are parsed into SpriteMetadata structures.

Functions:
- loadSingleSpriteMetadata: Parses one JSON entry into a SpriteMetadata object.
- loadSpriteMetadataFile: Loads and parses a JSON file containing multiple
  sprite definitions into a dictionary keyed by sprite name.
*/
/* End Header *******************************************************************/

#include <nlohmann/json.hpp>
#include <fstream>
#include "graphics/SpriteMetaData.hpp"

using json = nlohmann::json;

SpriteMetadata loadSingleSpriteMetadata(const json& rect, int texWidth, int texHeight) {
    SpriteMetadata sm;
    sm.offset = { rect["x"], rect["y"] };
    sm.size = { rect["w"], rect["h"] };
    sm.fullSize = { texWidth, texHeight };

    if (rect.contains("pivot")) {
        sm.pivot = { rect["pivot"][0], rect["pivot"][1] };
    }

    if (rect.contains("collider")) {
        auto c = rect["collider"];
        std::string type = c["type"];

        if (type == "aabb") {
            sm.collider.type = ColliderType::AABB;
            sm.collider.offset = { c["offset"][0], c["offset"][1] };
            sm.collider.size = { c["size"][0],   c["size"][1] };
        }
        else if (type == "circle") {
            sm.collider.type = ColliderType::Circle;
            sm.collider.center = { c["center"][0], c["center"][1] };
            sm.collider.radius = c["radius"];
        }
    }

    return sm;
}

std::unordered_map<std::string, SpriteMetadata> loadSpriteMetadataFile(const std::string& jsonPath, int texWidth, int texHeight) {
    std::ifstream in(jsonPath);
    if (!in) {
        throw std::runtime_error("Could not open " + jsonPath);
    }

    json data;
    in >> data;

    std::unordered_map<std::string, SpriteMetadata> result;
    for (auto& [name, rect] : data.items()) {
        result[name] = loadSingleSpriteMetadata(rect, texWidth, texHeight);
    }
    return result;
}