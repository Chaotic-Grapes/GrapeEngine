#pragma once

#include <unordered_map>
#include <cstdint>

#include "TileTypes.hpp"

// Simple POD UV rectangle (normalized 0..1)
struct TileUV
{
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
};

// Tileset:
// Maps TileID -> UVs in a texture atlas.
// No rendering, no editor logic, no collision (yet).
class Tileset
{
public:
    // Texture atlas handle (engine-level ID, not a graphics object)
    explicit Tileset(uint32_t textureId);

    uint32_t GetTextureId() const;

    // Define a tile's UVs in the atlas
    void DefineTile(TileID id, const TileUV& uv);

    // Query UVs for a tile
    // Returns false if tile is not defined
    bool GetTileUV(TileID id, TileUV& outUV) const;

    // Convenience: check if tile exists
    bool HasTile(TileID id) const;

private:
    uint32_t m_textureId = 0;
    std::unordered_map<TileID, TileUV> m_tileUVs;
};
