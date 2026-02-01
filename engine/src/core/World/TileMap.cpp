// TileMap.cpp

#include "TileMap.hpp"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>

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

bool TileMap::SaveMap(const std::string& filepath) const
{
    std::ofstream out(filepath, std::ios::binary);
    if (!out) return false;

    // Header: 1. Tile Size (float), 2. Layer Count (uint32)
    out.write(reinterpret_cast<const char*>(&m_tileSize), sizeof(float));
    
    uint32_t layerCount = static_cast<uint32_t>(m_layers.size());
    out.write(reinterpret_cast<const char*>(&layerCount), sizeof(uint32_t));

    for (const auto& layer : m_layers)
    {
        uint32_t w = layer.Width();
        uint32_t h = layer.Height();
        out.write(reinterpret_cast<const char*>(&w), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(&h), sizeof(uint32_t));

        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                TileID id = layer.Get(x, y);
                out.write(reinterpret_cast<const char*>(&id), sizeof(TileID));
            }
        }
    }
    
    return out.good();
}

bool TileMap::LoadMap(const std::string& filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in) return false;

    float fileTileSize;
    in.read(reinterpret_cast<char*>(&fileTileSize), sizeof(float));
    
    // Note: We ignore fileTileSize if it differs from m_tileSize because m_tileSize is const.
    // In a real editor, we might want to recreate the TileMap with the correct size.
    
    uint32_t layerCount;
    in.read(reinterpret_cast<char*>(&layerCount), sizeof(uint32_t));
    
    m_layers.clear();
    // Reserve to avoid reallocations
    m_layers.reserve(layerCount);
    
    for (uint32_t i = 0; i < layerCount; ++i)
    {
        uint32_t w, h;
        in.read(reinterpret_cast<char*>(&w), sizeof(uint32_t));
        in.read(reinterpret_cast<char*>(&h), sizeof(uint32_t));
        
        m_layers.emplace_back(w, h);
        TileLayer& layer = m_layers.back();
        
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                TileID id;
                in.read(reinterpret_cast<char*>(&id), sizeof(TileID));
                layer.Set(x, y, id);
            }
        }
    }

    return in.good();
}
