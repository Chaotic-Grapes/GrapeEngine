/* Start Header *****************************************************************/
/*!
\file   TileMap.hpp
\author Choi Meng Yew
\date   31st January 2026
\author Samantha Leong
\par    s.leong@digipen.edu
\date   3rd February 2026
\brief
Defines the TileMap class, which owns and coordinates multiple tile layers
and provides world-to-tile mapping utilities.

Details:
The TileMap class manages a collection of TileLayer objects and defines a
fixed world-units-per-tile scale. It serves as a thin data coordinator,
routing tile access and mutation to individual layers while handling
signed tile origins, resizing, and bounds expansion.

This class contains no rendering, ECS, collision, or gameplay logic.
It is intended to represent tile-based world data and support editor
and runtime systems through safe access, serialization, and coordinate
conversion utilities.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <string>

#include "Export.h"
#include "TileLayer.hpp"
#include "TileTypes.hpp" // TileID, EMPTY_TILE

// Owns multiple TileLayer(s) and defines the world-units-per-tile scale.
// Thin coordinator: no rendering, ECS, collision, or metadata.
class GRAPEENGINE_API TileMap
{
public:
    explicit TileMap(float tileSize);

    float TileSize() const;

    uint32_t LayerCount() const;

    // Adds a layer owned by the TileMap and returns its index
    uint32_t AddLayer(uint32_t width, uint32_t height);

    const TileLayer& GetLayer(uint32_t layerIndex) const;

    // Routes tile access; invalid layer or coordinates return EMPTY_TILE
    TileID GetTile(uint32_t layerIndex, uint32_t x, uint32_t y) const;

    // Routes tile mutation; invalid layer or coordinates are ignored
    void SetTile(uint32_t layerIndex, uint32_t x, uint32_t y, TileID id);

    // Resize an existing layer, preserving existing tiles in the overlap region.
    void ResizeLayer(uint32_t layerIndex, uint32_t newWidth, uint32_t newHeight);

    // Signed tile-space origin (tile coordinate that maps to layer index 0,0).
    int32_t OriginX() const;
    int32_t OriginY() const;
    void SetOrigin(int32_t originX, int32_t originY);

    // Expand the layer to include the requested signed tile coordinate.
    void ExpandLayerToFit(uint32_t layerIndex, int32_t tileX, int32_t tileY, uint32_t margin, uint32_t step);

    // World to tile helpers
    uint32_t WorldToTile(float worldCoord) const;
    int32_t  WorldToTileSigned(float worldCoord) const;
    float    TileToWorld(uint32_t tileCoord) const;
    float    TileToWorldSigned(int32_t tileCoord) const;

    // Signed tile helpers that account for the origin offset.
    bool IsTileInBounds(int32_t tileX, int32_t tileY) const;
    TileID GetTileSigned(uint32_t layerIndex, int32_t tileX, int32_t tileY) const;
    void SetTileSigned(uint32_t layerIndex, int32_t tileX, int32_t tileY, TileID id);

    // Tileset list stored in the tilemap asset.
    uint8_t AddTilesetPath(const std::string& path);
    int32_t FindTilesetPath(const std::string& path) const;
    const std::vector<std::string>& GetTilesetPaths() const;
    void SetTilesetPaths(const std::vector<std::string>& paths);

    /**
     * @brief Serializes the TileMap data to a binary file.
     * @param filepath The destination path for the map file.
     * @return True if the save was successful, false otherwise.
     */
    bool SaveMap(const std::string& filepath) const;
    
    /**
     * @brief Deserializes TileMap data from a binary file.
     * @note This clears existing layers before loading.
     * @param filepath The path to the binary map file.
     * @return True if the load was successful, false otherwise.
     */
    bool LoadMap(const std::string& filepath);

private:
    const float m_tileSize;          // immutable after creation
    std::vector<TileLayer> m_layers; // TileMap owns its layers
    int32_t m_originX = 0;           // Signed tile coordinate of layer index 0.
    int32_t m_originY = 0;           // Signed tile coordinate of layer index 0.
    std::vector<std::string> m_tilesetPaths;

    bool IsValidLayer(uint32_t layerIndex) const;
};
