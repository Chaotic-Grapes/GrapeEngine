#pragma once

#include "../include/core/World/TileMap.hpp"
#include "../include/core/World/TileSet.hpp"
#include <glm/vec2.hpp>
#include <vector>

class Renderer; // forward declare your batch renderer

// Submits tile geometry into the renderer
class TileMapRenderer
{
public:
    void Submit(
        const TileMap& tileMap,
        const std::vector<const Tileset*>& tilesets,
        Renderer& renderer,
        const glm::vec2& worldOffset
    ) const;
};
