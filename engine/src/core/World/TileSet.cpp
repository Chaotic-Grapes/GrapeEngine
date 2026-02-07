/* Start Header *****************************************************************/
/*!
\file   TileSet.cpp
\author Choi Meng Yew
\date   31st January 2026
\brief
Implementation of the Tileset class, which stores per-tile atlas UVs and
collision metadata.

Details:
This file implements the data-side logic for defining and querying tiles
within a texture atlas. It provides functions to associate TileID values
with normalized UV rectangles and optional collision classifications, and
to query this data safely at runtime.

The implementation contains no rendering, editor, or physics logic and
serves purely as a lookup table backing higher-level tilemap, collision,
and rendering systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "../include/core/World/TileSet.hpp"

#include <cassert>

Tileset::Tileset(uint32_t textureId)
    : m_textureId(textureId)
{
    assert(textureId != 0 && "Tileset requires a valid texture ID");
}

uint32_t Tileset::GetTextureId() const
{
    return m_textureId;
}

void Tileset::DefineTile(TileID id, const TileUV& uv, CollisionType collision)
{
    // TileID 0 is reserved as EMPTY_TILE (if using 0, but user said EMPTY_TILE is 0xFFFF)
    // The previous code had: assert(id != EMPTY_TILE);
    
    m_tiles[id] = { uv, collision };
}

bool Tileset::GetTileUV(TileID id, TileUV& outUV) const
{
    auto it = m_tiles.find(id);
    if (it == m_tiles.end())
        return false;

    outUV = it->second.uv;
    return true;
}

CollisionType Tileset::GetCollisionType(TileID id) const
{
    auto it = m_tiles.find(id);
    if (it == m_tiles.end())
        return CollisionType::NONE;
        
    return it->second.collision;
}

bool Tileset::HasTile(TileID id) const
{
    return m_tiles.find(id) != m_tiles.end();
}
