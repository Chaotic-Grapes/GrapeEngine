/* Start Header *****************************************************************/
/*!
\file   Tileset.hpp
\author Choi Meng Yew (100%)
\date   31st January 2026
\co-author Samantha Leong
\par    s.leong@digipen.edu
\date   3rd February 2026
\brief
Defines the Tileset class, which maps tile identifiers to texture atlas UVs
and per-tile collision metadata.

Details:
This file provides lightweight data structures for tile definition, including
normalized UV coordinates and collision type classification. The Tileset class
associates TileID values with regions in a texture atlas and optional collision
semantics, serving as a data source for rendering, collision, and editor systems.

The Tileset contains no rendering, editor, or physics logic; it acts purely as
a lookup table for tile properties shared across engine subsystems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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
