/* Start Header *****************************************************************/
/*!
\file   TileMap.cpp
\author Choi Meng Yew
\date   31st January 2026
\brief
Implementation of the TileMap class, providing tile-layer management,
coordinate conversion, dynamic resizing, and binary serialization.

Details:
This file implements the core logic for managing tile-based world data.
It handles layer creation and resizing, signed tile-space origins, safe
tile access and mutation, world–tile coordinate conversions, and dynamic
expansion of layers to accommodate out-of-bounds edits.

It also implements binary save/load support for tilemaps, including
backward-compatible deserialization, persistent tile origins, and
tileset path lists. No rendering, ECS, collision, or gameplay logic
is performed in this file.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "../include/core/World/TileMap.hpp"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <cstring>

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

	// Initialize collision grid if this is the first layer, using the same dimensions
    if (m_layers.size() == 1) {
        m_collisionWidth = width;
        m_collisionHeight = height;
        m_collisionMasks.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);  // Initialize all masks to 0 (no collision)
    }
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

void TileMap::ResizeLayer(uint32_t layerIndex, uint32_t newWidth, uint32_t newHeight)
{
    if (!IsValidLayer(layerIndex))
        return;

    if (newWidth == 0 || newHeight == 0)
        return;

    TileLayer& layer = m_layers[layerIndex];
    if (layer.Width() == newWidth && layer.Height() == newHeight)
        return;

    TileLayer resized(newWidth, newHeight);
    const uint32_t copyWidth = std::min(layer.Width(), newWidth);
    const uint32_t copyHeight = std::min(layer.Height(), newHeight);

    for (uint32_t y = 0; y < copyHeight; ++y) {
        for (uint32_t x = 0; x < copyWidth; ++x) {
            resized.Set(x, y, layer.Get(x, y));
        }
    }

    layer = std::move(resized);

	// If resizing the first layer, we also need to resize the collision grid to match the new dimensions
    if (layerIndex == 0) ResizeCollisionGrid(newWidth, newHeight, 0, 0);
}

int32_t TileMap::OriginX() const
{
    return m_originX;
}

int32_t TileMap::OriginY() const
{
    return m_originY;
}

void TileMap::SetOrigin(int32_t originX, int32_t originY)
{
    m_originX = originX;
    m_originY = originY;
}

void TileMap::ExpandLayerToFit(uint32_t layerIndex, int32_t tileX, int32_t tileY, uint32_t margin, uint32_t step)
{
    if (!IsValidLayer(layerIndex))
        return;

    TileLayer& layer = m_layers[layerIndex];
    const int32_t oldOriginX = m_originX;
    const int32_t oldOriginY = m_originY;
    const uint32_t oldWidth = layer.Width();
    const uint32_t oldHeight = layer.Height();

    // Compute the desired signed bounds around the target tile.
    const int32_t minX = tileX - static_cast<int32_t>(margin);
    const int32_t minY = tileY - static_cast<int32_t>(margin);
    const int32_t maxX = tileX + static_cast<int32_t>(margin);
    const int32_t maxY = tileY + static_cast<int32_t>(margin);

    int32_t newOriginX = oldOriginX;
    int32_t newOriginY = oldOriginY;
    uint32_t newWidth = oldWidth;
    uint32_t newHeight = oldHeight;

    const auto roundUpToStep = [step](uint32_t value) {
        if (step == 0) return value;
        const uint32_t rem = value % step;
        return rem == 0 ? value : value + (step - rem);
    };

    if (minX < newOriginX) {
        // Grow to the left by shifting the origin and widening the layer.
        const uint32_t expandLeft = roundUpToStep(static_cast<uint32_t>(newOriginX - minX));
        newOriginX -= static_cast<int32_t>(expandLeft);
        newWidth += expandLeft;
    }

    const int32_t maxXInclusive = newOriginX + static_cast<int32_t>(newWidth) - 1;
    if (maxX > maxXInclusive) {
        // Grow to the right by widening the layer without shifting origin.
        const uint32_t expandRight = roundUpToStep(static_cast<uint32_t>(maxX - maxXInclusive));
        newWidth += expandRight;
    }

    if (minY < newOriginY) {
        // Grow downward (negative) by shifting the origin and height.
        const uint32_t expandDown = roundUpToStep(static_cast<uint32_t>(newOriginY - minY));
        newOriginY -= static_cast<int32_t>(expandDown);
        newHeight += expandDown;
    }

    const int32_t maxYInclusive = newOriginY + static_cast<int32_t>(newHeight) - 1;
    if (maxY > maxYInclusive) {
        // Grow upward by increasing height without shifting origin.
        const uint32_t expandUp = roundUpToStep(static_cast<uint32_t>(maxY - maxYInclusive));
        newHeight += expandUp;
    }

    if (newOriginX == oldOriginX && newOriginY == oldOriginY &&
        newWidth == oldWidth && newHeight == oldHeight) {
        return;
    }

    TileLayer resized(newWidth, newHeight); // New layer with expanded bounds.
    const int32_t offsetX = oldOriginX - newOriginX;
    const int32_t offsetY = oldOriginY - newOriginY;

    for (uint32_t y = 0; y < oldHeight; ++y) {
        for (uint32_t x = 0; x < oldWidth; ++x) {
            const uint32_t newX = static_cast<uint32_t>(static_cast<int32_t>(x) + offsetX);
            const uint32_t newY = static_cast<uint32_t>(static_cast<int32_t>(y) + offsetY);
            resized.Set(newX, newY, layer.Get(x, y)); // Preserve existing tiles with the new origin offset.
        }
    }

    layer = std::move(resized);
    m_originX = newOriginX;
    m_originY = newOriginY;

	// Same thing; resize to match the new layer dimensions and shift existing masks if the origin moved
    if (layerIndex == 0) ResizeCollisionGrid(newWidth, newHeight, offsetX, offsetY);
}

// world -> tile (floor avoids off-by-one at boundaries)
uint32_t TileMap::WorldToTile(float worldCoord) const
{
    assert(worldCoord >= 0.0f);
    return static_cast<uint32_t>(std::floor(worldCoord / m_tileSize));
}

int32_t TileMap::WorldToTileSigned(float worldCoord) const
{
    return static_cast<int32_t>(std::floor(worldCoord / m_tileSize));
}

// tile -> world (tile origin)
float TileMap::TileToWorld(uint32_t tileCoord) const
{
    return tileCoord * m_tileSize; // Local-space conversion without origin offset.
}

float TileMap::TileToWorldSigned(int32_t tileCoord) const
{
    return static_cast<float>(tileCoord) * m_tileSize;
}

bool TileMap::IsTileInBounds(int32_t tileX, int32_t tileY) const
{
    if (m_layers.empty()) return false;

    const TileLayer& layer = m_layers[0];
    const int32_t ix = tileX - m_originX;
    const int32_t iy = tileY - m_originY;

    return ix >= 0 && iy >= 0 &&
        ix < static_cast<int32_t>(layer.Width()) &&
        iy < static_cast<int32_t>(layer.Height());
}

TileID TileMap::GetTileSigned(uint32_t layerIndex, int32_t tileX, int32_t tileY) const
{
    if (!IsValidLayer(layerIndex))
        return EMPTY_TILE;

    const int32_t ix = tileX - m_originX;
    const int32_t iy = tileY - m_originY;
    if (ix < 0 || iy < 0)
        return EMPTY_TILE;

    const TileLayer& layer = m_layers[layerIndex];
    if (ix >= static_cast<int32_t>(layer.Width()) || iy >= static_cast<int32_t>(layer.Height()))
        return EMPTY_TILE;

    return layer.Get(static_cast<uint32_t>(ix), static_cast<uint32_t>(iy));
}

void TileMap::SetTileSigned(uint32_t layerIndex, int32_t tileX, int32_t tileY, TileID id)
{
    if (!IsValidLayer(layerIndex))
        return;

    const int32_t ix = tileX - m_originX;
    const int32_t iy = tileY - m_originY;
    if (ix < 0 || iy < 0)
        return;

    TileLayer& layer = m_layers[layerIndex];
    if (ix >= static_cast<int32_t>(layer.Width()) || iy >= static_cast<int32_t>(layer.Height()))
        return;

    layer.Set(static_cast<uint32_t>(ix), static_cast<uint32_t>(iy), id);
}

// Collision masks are stored in a separate grid aligned with the first layer, 
// indexed by signed tile coordinates relative to the origin
// Each mask is 4 bits representing collision in 2x2 subcells of the tile
uint8_t TileMap::GetCollisionMaskSigned(int32_t tileX, int32_t tileY) const
{
	// If there are no collision masks, treat as empty (0)
    if (m_collisionMasks.empty()) return 0;

	// Calculate the index in the collision mask grid based on the signed tile coordinates and origin offset
    const int32_t ix = tileX - m_originX;
    const int32_t iy = tileY - m_originY;

	// If the coordinates are out of bounds of the collision mask grid, treat as empty
    if (ix < 0 || iy < 0) return 0;
    if (ix >= static_cast<int32_t>(m_collisionWidth) || iy >= static_cast<int32_t>(m_collisionHeight)) return 0;

	// Calculate the linear index into the collision mask vector and return the mask value
    const size_t index = static_cast<size_t>(iy) * m_collisionWidth + static_cast<size_t>(ix);
    return m_collisionMasks[index];
}

// Sets the collision mask for the tile at the given signed coordinates, if within bounds
void TileMap::SetCollisionMaskSigned(int32_t tileX, int32_t tileY, uint8_t mask)
{
    // Usual checks
    if (m_collisionMasks.empty()) return;

	// Same as above
    const int32_t ix = tileX - m_originX;
    const int32_t iy = tileY - m_originY;
    if (ix < 0 || iy < 0) return;
    if (ix >= static_cast<int32_t>(m_collisionWidth) || iy >= static_cast<int32_t>(m_collisionHeight)) return;

	// Store only the lower 4 bits of the mask, since each tile has a 4-bit collision mask for its 2x2 subcells
    const size_t index = static_cast<size_t>(iy) * m_collisionWidth + static_cast<size_t>(ix);
    m_collisionMasks[index] = static_cast<uint8_t>(mask & 0x0F);
}

uint8_t TileMap::AddTilesetPath(const std::string& path)
{
    if (path.empty()) {
        return 0;
    }

    const int32_t existing = FindTilesetPath(path);
    if (existing >= 0) {
        return static_cast<uint8_t>(existing);
    }

    if (m_tilesetPaths.size() >= 255) {
        // Index is packed into 8 bits, so clamp at 255 entries.
        return static_cast<uint8_t>(m_tilesetPaths.size() - 1);
    }

    m_tilesetPaths.push_back(path);
    return static_cast<uint8_t>(m_tilesetPaths.size() - 1);
}

int32_t TileMap::FindTilesetPath(const std::string& path) const
{
    for (size_t i = 0; i < m_tilesetPaths.size(); ++i) {
        if (m_tilesetPaths[i] == path) {
            return static_cast<int32_t>(i);
        }
    }

    return -1;
}

const std::vector<std::string>& TileMap::GetTilesetPaths() const
{
    return m_tilesetPaths;
}

void TileMap::SetTilesetPaths(const std::vector<std::string>& paths)
{
    m_tilesetPaths = paths;
}

bool TileMap::IsValidLayer(uint32_t layerIndex) const
{
    return layerIndex < m_layers.size();
}

// If the layer is resized to be smaller, we can just discard the out-of-bounds collision masks
// If larger, we need to create new collision masks for the new area
void TileMap::ResizeCollisionGrid(uint32_t newWidth, uint32_t newHeight, int32_t offsetX, int32_t offsetY)
{
	// If there are no collision masks yet, initialize to the new size with empty masks
    if (newWidth == 0 || newHeight == 0) return;

	// Create a new collision mask grid with the new dimensions, initialized to 0 (no collision)
    std::vector<uint8_t> resized(static_cast<size_t>(newWidth) * static_cast<size_t>(newHeight), 0);
    const uint32_t copyWidth = std::min(m_collisionWidth, newWidth);
    const uint32_t copyHeight = std::min(m_collisionHeight, newHeight);

	// Copy existing collision masks to the new grid with the appropriate offset, if they fall within the new bounds
    for (uint32_t y = 0; y < copyHeight; y++) {
        for (uint32_t x = 0; x < copyWidth; x++) {
			// Calculate the new coordinates with the offset applied
            const int32_t newX = static_cast<int32_t>(x) + offsetX;
            const int32_t newY = static_cast<int32_t>(y) + offsetY;

			// Skip if the new coordinates are out of bounds of the resized grid
            if (newX < 0 || newY < 0) continue;
            if (newX >= static_cast<int32_t>(newWidth) || newY >= static_cast<int32_t>(newHeight)) continue;

			// Copy the collision mask from the old grid to the new grid at the new coordinates
            const size_t oldIndex = static_cast<size_t>(y) * m_collisionWidth + static_cast<size_t>(x);
            const size_t newIndex = static_cast<size_t>(newY) * newWidth + static_cast<size_t>(newX);

			// If the old index is out of bounds of the existing collision masks, treat it as 0 
            resized[newIndex] = m_collisionMasks.empty() ? 0 : m_collisionMasks[oldIndex];
        }
    }

	// Replace the old collision masks with the resized version and update dimensions
    m_collisionMasks = std::move(resized);
    m_collisionWidth = newWidth;
    m_collisionHeight = newHeight;
}

bool TileMap::SaveMap(const std::string& filepath) const
{
    std::ofstream out(filepath, std::ios::binary);
    if (!out) return false;

    // Header: Magic, Version, Tile Size, Layer Count
    static constexpr uint32_t kTileMapMagic = 0x50414D54; // 'TMAP'
    static constexpr uint32_t kTileMapVersion = 2; // Version with tileset list + origin.

    out.write(reinterpret_cast<const char*>(&kTileMapMagic), sizeof(uint32_t));
    out.write(reinterpret_cast<const char*>(&kTileMapVersion), sizeof(uint32_t));

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

    // Append origin after layers.
    out.write(reinterpret_cast<const char*>(&m_originX), sizeof(int32_t));
    out.write(reinterpret_cast<const char*>(&m_originY), sizeof(int32_t));

    // Append tileset list (count + length-prefixed strings).
    const uint32_t tilesetCount = static_cast<uint32_t>(m_tilesetPaths.size());
    out.write(reinterpret_cast<const char*>(&tilesetCount), sizeof(uint32_t));
    for (const auto& path : m_tilesetPaths) {
        const uint32_t len = static_cast<uint32_t>(path.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
        if (len > 0) {
            out.write(path.data(), len);
        }
    }
    
    const bool ok = out.good();
    if (!ok) {
        std::cerr << "[TileMap] Failed to write tilemap: " << filepath << "\n";
    }
    return ok;
}

bool TileMap::LoadMap(const std::string& filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in) {
        std::cerr << "[TileMap] Failed to open tilemap: " << filepath << "\n";
        return false;
    }

    static constexpr uint32_t kTileMapMagic = 0x50414D54; // 'TMAP'
    static constexpr uint32_t kTileMapVersionWithTilesets = 2;

    uint32_t header = 0;
    in.read(reinterpret_cast<char*>(&header), sizeof(uint32_t));
    if (!in) {
        std::cerr << "[TileMap] Failed to read tilemap header: " << filepath << "\n";
        return false;
    }

    uint32_t version = 0;
    float fileTileSize = 0.0f;

    const bool isNewFormat = (header == kTileMapMagic);
    if (isNewFormat) {
        in.read(reinterpret_cast<char*>(&version), sizeof(uint32_t));
        in.read(reinterpret_cast<char*>(&fileTileSize), sizeof(float));
    } else {
        // Old format: first 4 bytes are the tile size float.
        std::memcpy(&fileTileSize, &header, sizeof(float));
    }
    
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
                if (isNewFormat) {
                    TileID id;
                    in.read(reinterpret_cast<char*>(&id), sizeof(TileID));
                    layer.Set(x, y, id);
                } else {
                    uint16_t id16 = 0;
                    in.read(reinterpret_cast<char*>(&id16), sizeof(uint16_t));
                    layer.Set(x, y, static_cast<TileID>(id16));
                }
            }
        }
    }

    // Read optional origin data if present (older files won't have it).
    m_originX = 0;
    m_originY = 0;
    if (in.peek() != EOF) {
        int32_t ox = 0;
        int32_t oy = 0;
        in.read(reinterpret_cast<char*>(&ox), sizeof(int32_t));
        in.read(reinterpret_cast<char*>(&oy), sizeof(int32_t));
        if (in.good()) {
            m_originX = ox;
            m_originY = oy;
        }
    }

    // Read optional tileset list if present.
    m_tilesetPaths.clear();
    const bool canReadTilesets = (isNewFormat && version >= kTileMapVersionWithTilesets) || !isNewFormat;
    if (canReadTilesets && in.peek() != EOF) {
        uint32_t tilesetCount = 0;
        in.read(reinterpret_cast<char*>(&tilesetCount), sizeof(uint32_t));
        if (in.good()) {
            m_tilesetPaths.reserve(tilesetCount);
            for (uint32_t i = 0; i < tilesetCount; ++i) {
                uint32_t len = 0;
                in.read(reinterpret_cast<char*>(&len), sizeof(uint32_t));
                std::string path;
                if (len > 0) {
                    path.resize(len);
                    in.read(path.data(), len);
                }
                if (in.good()) {
                    m_tilesetPaths.push_back(path);
                }
            }
        }
    }

    const bool ok = in.good();
    if (!ok) {
        std::cerr << "[TileMap] Failed to read tilemap data: " << filepath << "\n";
        return false;
    }

    std::cerr << "[TileMap] Loaded " << filepath
              << " layers=" << m_layers.size()
              << " tilesets=" << m_tilesetPaths.size()
              << " origin=(" << m_originX << "," << m_originY << ")"
              << "\n";
    return true;
}
