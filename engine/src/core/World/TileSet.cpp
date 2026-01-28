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

void Tileset::DefineTile(TileID id, const TileUV& uv)
{
    // TileID 0 is reserved as EMPTY_TILE
    assert(id != EMPTY_TILE);

    m_tileUVs[id] = uv;
}

bool Tileset::GetTileUV(TileID id, TileUV& outUV) const
{
    auto it = m_tileUVs.find(id);
    if (it == m_tileUVs.end())
        return false;

    outUV = it->second;
    return true;
}

bool Tileset::HasTile(TileID id) const
{
    return m_tileUVs.find(id) != m_tileUVs.end();
}
