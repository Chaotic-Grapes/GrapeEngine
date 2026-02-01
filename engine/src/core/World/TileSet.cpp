#include "Tileset.hpp"

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
