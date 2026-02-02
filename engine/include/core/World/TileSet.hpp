#pragma once

#include <unordered_map>
#include <cstdint>

#include "Export.h"
#include "TileTypes.hpp"

// Simple POD UV rectangle (normalized 0..1)
struct TileUV
{
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
};

enum class CollisionType {
    NONE,
    SOLID,
    DIAGONAL_TL,
    DIAGONAL_TR,
    DIAGONAL_BL,
    DIAGONAL_BR
};

struct TileDef {
    TileUV uv;
    CollisionType collision = CollisionType::NONE;
};

// Tileset:
// Maps TileID -> UVs in a texture atlas.
// No rendering, no editor logic, no collision (yet).
class GRAPEENGINE_API Tileset
{
public:
    // Texture atlas handle (engine-level ID, not a graphics object)
    explicit Tileset(uint32_t textureId);

    uint32_t GetTextureId() const;

    // Define a tile's UVs and Collision in the atlas
    void DefineTile(TileID id, const TileUV& uv, CollisionType collision = CollisionType::NONE);

    // Query UVs for a tile
    // Returns false if tile is not defined
    bool GetTileUV(TileID id, TileUV& outUV) const;

    // Query Collision for a tile
    CollisionType GetCollisionType(TileID id) const;

    // Get all defined tiles (for editor iteration)
    const std::unordered_map<TileID, TileDef>& GetTiles() const { return m_tiles; }

    // Convenience: check if tile exists
    bool HasTile(TileID id) const;

private:
    uint32_t m_textureId = 0;
    std::unordered_map<TileID, TileDef> m_tiles;
};
