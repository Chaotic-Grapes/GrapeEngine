/* Start Header *****************************************************************/
/*!
\file   SpriteMetadata.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   26th September 2025
\brief
Sprite metadata describes how a sprite is trimmed from a texture, where its
pivot (anchor) lies, and what collider shape it uses. This information is
typically supplied via JSON.

The JSON entry for each sprite must include:
- "x", "y" : Top-left offset of the trimmed rect in pixels.
- "w", "h" : Width and height of the trimmed rect in pixels.
- "pivot"  : Optional [x,y] normalized anchor point (0..1 range).
- "collider" : Optional collider definition
*/
/* End Header *******************************************************************/

#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

enum class ColliderType {
    None,
    AABB,
    Circle
};

struct ColliderMetadata {
    ColliderType type = ColliderType::None;

    // For AABB
    glm::ivec2 offset{ 0,0 };
    glm::ivec2 size{ 0,0 };

    // For Circle
    glm::vec2 center{ 0,0 };
    float radius = 0.0f;
};

struct SpriteMetadata {
    glm::ivec2 offset{ 0,0 };   // trimmed rect offset
    glm::ivec2 size{ 0,0 };     // trimmed rect size
    glm::ivec2 fullSize{ 0,0 }; // full texture size
    glm::vec2 pivot{ 0.5f,0.5f };

    ColliderMetadata collider;
};

// Function declarations
SpriteMetadata loadSingleSpriteMetadata(const nlohmann::json& rect, int texWidth, int texHeight);

std::unordered_map<std::string, SpriteMetadata>
loadSpriteMetadataFile(const std::string& jsonPath, int texWidth, int texHeight);