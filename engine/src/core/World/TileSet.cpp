/* Start Header *****************************************************************/
/*!
\file   TileSet.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu

\brief
The definition of the Tileset class for managing tile definitions within a
texture atlas. Provides methods to define tiles, query their UVs and collision types.
*/
/* End Header *******************************************************************/

#include "core/World/TileSet.hpp"

#include <cassert>

// Handle tileset.
Tileset::Tileset(uint32_t textureId)
    // Store the texture ID for later lookups.
    : m_textureId(textureId)
{
    assert(textureId != 0 && "Tileset requires a valid texture ID");
}

// Return texture id.
uint32_t Tileset::GetTextureId() const
{
    return m_textureId;
}

// Define or update tile UVs and collision data.
void Tileset::DefineTile(TileID id, const TileUV& uv, CollisionType collision)
{
    // TileID 0 is reserved as EMPTY_TILE (if using 0, but user said EMPTY_TILE is 0xFFFF)
    // The previous code had: assert(id != EMPTY_TILE);
    
    m_tiles[id] = { uv, collision };
}

// Return tile uv.
bool Tileset::GetTileUV(TileID id, TileUV& outUV) const
{
    auto it = m_tiles.find(id);
    if (it == m_tiles.end())
        return false;

    outUV = it->second.uv;
    return true;
}

// Return collision type.
CollisionType Tileset::GetCollisionType(TileID id) const
{
    auto it = m_tiles.find(id);
    if (it == m_tiles.end())
        return CollisionType::NONE;
        
    return it->second.collision;
}

// Check for tile.
bool Tileset::HasTile(TileID id) const
{
    return m_tiles.find(id) != m_tiles.end();
}
