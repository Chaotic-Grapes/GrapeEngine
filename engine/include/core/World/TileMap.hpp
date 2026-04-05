/* Start Header *****************************************************************/
/*!
\file   TileMap.hpp
\author Choi Meng Yew (90%)
        Samantha Leong (10%)
\par    choi.m@digipen.edu
        s.leong@digipen.edu
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
    /**
     * @brief Construct a tilemap with fixed world-units-per-tile scale.
     * @param tileSize World-space size of one tile.
     */
    explicit TileMap(float tileSize);

    /**
     * @brief Get tile size in world units.
     * @return Tile size scalar.
     */
    float TileSize() const;

    /**
     * @brief Get number of layers currently owned by this map.
     * @return Layer count.
     */
    uint32_t LayerCount() const;

    /**
     * @brief Add a new layer and return its index.
     * @param width Layer width in tiles.
     * @param height Layer height in tiles.
     * @return Index of the newly added layer.
     */
    uint32_t AddLayer(uint32_t width, uint32_t height);

    /**
     * @brief Get a read-only layer reference.
     * @param layerIndex Target layer index.
     * @return Const reference to requested layer.
     */
    const TileLayer& GetLayer(uint32_t layerIndex) const;

    /**
     * @brief Read one tile value from a layer.
     * @param layerIndex Target layer index.
     * @param x Tile x index in layer space.
     * @param y Tile y index in layer space.
     * @return Tile id, or EMPTY_TILE on invalid input.
     */
    TileID GetTile(uint32_t layerIndex, uint32_t x, uint32_t y) const;

    /**
     * @brief Write one tile value into a layer.
     * @param layerIndex Target layer index.
     * @param x Tile x index in layer space.
     * @param y Tile y index in layer space.
     * @param id Tile id to store.
     */
    void SetTile(uint32_t layerIndex, uint32_t x, uint32_t y, TileID id);

    /**
     * @brief Resize a layer while preserving overlap region data.
     * @param layerIndex Target layer index.
     * @param newWidth New layer width in tiles.
     * @param newHeight New layer height in tiles.
     */
    void ResizeLayer(uint32_t layerIndex, uint32_t newWidth, uint32_t newHeight);

    /**
     * @brief Get signed tile-space X origin.
     * @return Tile-space origin X.
     */
    int32_t OriginX() const;

    /**
     * @brief Get signed tile-space Y origin.
     * @return Tile-space origin Y.
     */
    int32_t OriginY() const;

    /**
     * @brief Set signed tile-space origin.
     * @param originX New tile-space origin X.
     * @param originY New tile-space origin Y.
     */
    void SetOrigin(int32_t originX, int32_t originY);

    /**
     * @brief Expand a layer to include requested signed coordinate.
     * @param layerIndex Target layer index.
     * @param tileX Signed tile x coordinate.
     * @param tileY Signed tile y coordinate.
     * @param margin Extra tiles to pad around requested coordinate.
     * @param step Expansion granularity in tiles.
     */
    void ExpandLayerToFit(uint32_t layerIndex, int32_t tileX, int32_t tileY, uint32_t margin, uint32_t step);

    /**
     * @brief Convert world scalar to unsigned tile coordinate.
     * @param worldCoord World coordinate.
     * @return Unsigned tile coordinate.
     */
    uint32_t WorldToTile(float worldCoord) const;

    /**
     * @brief Convert world scalar to signed tile coordinate.
     * @param worldCoord World coordinate.
     * @return Signed tile coordinate.
     */
    int32_t  WorldToTileSigned(float worldCoord) const;

    /**
     * @brief Convert unsigned tile coordinate to world scalar.
     * @param tileCoord Unsigned tile coordinate.
     * @return World coordinate.
     */
    float    TileToWorld(uint32_t tileCoord) const;

    /**
     * @brief Convert signed tile coordinate to world scalar.
     * @param tileCoord Signed tile coordinate.
     * @return World coordinate.
     */
    float    TileToWorldSigned(int32_t tileCoord) const;

    /**
     * @brief Check whether signed tile coordinate is in bounds.
     * @param tileX Signed tile x coordinate.
     * @param tileY Signed tile y coordinate.
     * @return True when coordinate maps to a valid layer-0 tile.
     */
    bool IsTileInBounds(int32_t tileX, int32_t tileY) const;

    /**
     * @brief Read one tile using signed coordinates with origin offset.
     * @param layerIndex Target layer index.
     * @param tileX Signed tile x coordinate.
     * @param tileY Signed tile y coordinate.
     * @return Tile id, or EMPTY_TILE when out of bounds.
     */
    TileID GetTileSigned(uint32_t layerIndex, int32_t tileX, int32_t tileY) const;

    /**
     * @brief Write one tile using signed coordinates with origin offset.
     * @param layerIndex Target layer index.
     * @param tileX Signed tile x coordinate.
     * @param tileY Signed tile y coordinate.
     * @param id Tile id to write.
     */
    void SetTileSigned(uint32_t layerIndex, int32_t tileX, int32_t tileY, TileID id);

    /**
     * @brief Read collision mask at signed tile coordinate.
     * @param tileX Signed tile x coordinate.
     * @param tileY Signed tile y coordinate.
     * @return 4-bit collision mask value.
     */
    uint8_t GetCollisionMaskSigned(int32_t tileX, int32_t tileY) const;

    /**
     * @brief Write collision mask at signed tile coordinate.
     * @param tileX Signed tile x coordinate.
     * @param tileY Signed tile y coordinate.
     * @param mask 4-bit collision mask value.
     */
    void SetCollisionMaskSigned(int32_t tileX, int32_t tileY, uint8_t mask);

    /**
     * @brief Access packed collision mask grid for upload.
     * @return Const mask buffer reference.
     */
    const std::vector<uint8_t>& GetCollisionMasks() const { return m_collisionMasks; }

    /**
     * @brief Get collision grid width in tiles.
     * @return Collision width.
     */
    uint32_t CollisionWidth()  const { return m_collisionWidth; }

    /**
     * @brief Get collision grid height in tiles.
     * @return Collision height.
     */
    uint32_t CollisionHeight() const { return m_collisionHeight; }

    /**
     * @brief Append a tileset path and return its index.
     * @param path Tileset asset path.
     * @return Index of inserted path.
     */
    uint8_t AddTilesetPath(const std::string& path);

    /**
     * @brief Find tileset path index.
     * @param path Tileset asset path.
     * @return Index when found, otherwise -1.
     */
    int32_t FindTilesetPath(const std::string& path) const;

    /**
     * @brief Get list of tileset paths stored by this map.
     * @return Const vector of tileset paths.
     */
    const std::vector<std::string>& GetTilesetPaths() const;

    /**
     * @brief Replace tileset path list.
     * @param paths New tileset path list.
     */
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
    std::vector<uint8_t> m_collisionMasks; // 4-bit mask per tile (layer 0 only).
    uint32_t m_collisionWidth = 0;
    uint32_t m_collisionHeight = 0;

    /**
     * @brief Validate a layer index.
     * @param layerIndex Candidate layer index.
     * @return True when layer index is valid.
     */
    bool IsValidLayer(uint32_t layerIndex) const;

    /**
     * @brief Resize collision grid while preserving overlapping cells.
     * @param newWidth New collision width.
     * @param newHeight New collision height.
     * @param offsetX Signed x offset applied to old content.
     * @param offsetY Signed y offset applied to old content.
     */
    void ResizeCollisionGrid(uint32_t newWidth, uint32_t newHeight, int32_t offsetX, int32_t offsetY);
};
