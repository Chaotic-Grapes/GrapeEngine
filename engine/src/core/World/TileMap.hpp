#pragma once

#include <vector>
#include <cstdint>
#include <cassert>

#include "TileLayer.hpp"
#include "TileTypes.hpp" // TileID, EMPTY_TILE

// Owns multiple TileLayer(s) and defines the world-units-per-tile scale.
// Thin coordinator: no rendering, ECS, collision, or metadata.
class TileMap
{
public:
    explicit TileMap(float tileSize);

    float TileSize() const;

    uint32_t LayerCount() const;

    // Adds a layer owned by the TileMap and returns its index
    uint32_t AddLayer(uint32_t width, uint32_t height);

    const TileLayer& GetLayer(uint32_t layerIndex) const;

    // Routes tile access; invalid layer or coordinates return EMPTY_TILE
    TileID GetTile(uint32_t layerIndex, uint32_t x, uint32_t y) const;

    // Routes tile mutation; invalid layer or coordinates are ignored
    void SetTile(uint32_t layerIndex, uint32_t x, uint32_t y, TileID id);

    // World to tile helpers
    uint32_t WorldToTile(float worldCoord) const;
    float    TileToWorld(uint32_t tileCoord) const;

private:
    const float m_tileSize;          // immutable after creation
    std::vector<TileLayer> m_layers; // TileMap owns its layers

    bool IsValidLayer(uint32_t layerIndex) const;
};
