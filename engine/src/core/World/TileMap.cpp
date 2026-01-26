// TileMap.cpp

#include "TileMap.hpp"

#include <cassert>
#include <cmath>

TileMap::TileMap(float tileSize)
    : m_tileSize(tileSize)
{
    assert(tileSize > 0.0f);
}

float TileMap::TileSize() const
{
    return m_tileSize;
}

uint32_t TileMap::LayerCount() const
{
    return static_cast<uint32_t>(m_layers.size());
}

uint32_t TileMap::AddLayer(uint32_t width, uint32_t height)
{
    m_layers.emplace_back(width, height);
    return static_cast<uint32_t>(m_layers.size() - 1);
}

const TileLayer& TileMap::GetLayer(uint32_t layerIndex) const
{
    assert(IsValidLayer(layerIndex));
    return m_layers[layerIndex];
}

TileID TileMap::GetTile(uint32_t layerIndex, uint32_t x, uint32_t y) const
{
    if (!IsValidLayer(layerIndex))
        return EMPTY_TILE;

    return m_layers[layerIndex].Get(x, y);
}

void TileMap::SetTile(uint32_t layerIndex, uint32_t x, uint32_t y, TileID id)
{
    if (!IsValidLayer(layerIndex))
        return;

    m_layers[layerIndex].Set(x, y, id);
}

// world -> tile (floor avoids off-by-one at boundaries)
uint32_t TileMap::WorldToTile(float worldCoord) const
{
    assert(worldCoord >= 0.0f);
    return static_cast<uint32_t>(std::floor(worldCoord / m_tileSize));
}

// tile -> world (tile origin)
float TileMap::TileToWorld(uint32_t tileCoord) const
{
    return tileCoord * m_tileSize;
}

bool TileMap::IsValidLayer(uint32_t layerIndex) const
{
    return layerIndex < m_layers.size();
}
