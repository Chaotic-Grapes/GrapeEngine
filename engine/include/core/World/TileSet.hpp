/* Start Header *****************************************************************/
/*!
\file   Tileset.hpp
\author Choi Meng Yew (90%)
        Samantha Leong (10%)
\par    choi.m@digipen.edu
        s.leong@digipen.edu
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
    /**
     * @brief Construct a tileset bound to a texture atlas id.
     * @param textureId Engine texture identifier for the atlas.
     */
    explicit Tileset(uint32_t textureId);

    /**
     * @brief Get texture atlas id backing this tileset.
     * @return Engine texture id.
     */
    uint32_t GetTextureId() const;

    // Define a tile's UVs and Collision in the atlas
    /**
     * @brief Define UV and collision metadata for a tile id.
     * @param id Tile id to define or overwrite.
     * @param uv Normalized atlas UV rectangle.
     * @param collision Collision classification for this tile.
     */
    void DefineTile(TileID id, const TileUV& uv, CollisionType collision = CollisionType::NONE);

    // Query UVs for a tile
    // Returns false if tile is not defined
    /**
     * @brief Look up UV coordinates for a tile id.
     * @param id Tile id to query.
     * @param outUV Output UV rectangle when tile exists.
     * @return True when tile was defined.
     */
    bool GetTileUV(TileID id, TileUV& outUV) const;

    // Query Collision for a tile
    /**
     * @brief Get collision type assigned to a tile id.
     * @param id Tile id to query.
     * @return Collision type, or default behavior for undefined tiles.
     */
    CollisionType GetCollisionType(TileID id) const;

    // Get all defined tiles (for editor iteration)
    /**
     * @brief Access internal tile definition map.
     * @return Const reference to tile definitions keyed by TileID.
     */
    const std::unordered_map<TileID, TileDef>& GetTiles() const { return m_tiles; }

    // Convenience: check if tile exists
    /**
     * @brief Check whether a tile id has been defined.
     * @param id Tile id to check.
     * @return True when tile exists in definition map.
     */
    bool HasTile(TileID id) const;

private:
    uint32_t m_textureId = 0;
    std::unordered_map<TileID, TileDef> m_tiles;
};
