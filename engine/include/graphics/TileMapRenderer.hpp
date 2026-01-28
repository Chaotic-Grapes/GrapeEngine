#pragma once

#include "../src/core/World/TileMap.hpp"
#include "../src/core/World/TileSet.hpp"

class Renderer; // forward declare your batch renderer

// Submits tile geometry into the renderer
class TileMapRenderer
{
public:
    void Submit(
        const TileMap& tileMap,
        const Tileset& tileset,
        Renderer& renderer
    ) const;
};
