#pragma once

#include <memory>
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <glm/glm.hpp>

#include "../../engine/include/core/World/TileMap.hpp"
#include "../../engine/include/core/World/Tileset.hpp"
#include "ecs/Entity.h"

namespace ECS { class World; }

class TilePalettePanel {
public:
    struct TileMapListEntry {
        EntityId Id;
        std::string Name;
    };

    TilePalettePanel() = default;
    ~TilePalettePanel() = default;

    void Initialize(const std::shared_ptr<TileMap>& tileMap, const std::shared_ptr<Tileset>& tileset, ECS::World* world);
    void SetWorld(ECS::World* world) { m_world = world; }
    // Update the active tilemap/tileset and map path used for save operations.
    void SetEditingContext(const std::shared_ptr<TileMap>& tileMap,
        const std::vector<std::shared_ptr<Tileset>>& tilesets,
        const std::vector<std::string>& tilesetPaths,
        uint8_t activeTilesetIndex,
        const std::string& tileMapPath,
        const glm::vec2& worldOrigin);

    void Render(); // ImGui Palette Window

    // Interaction hooks
    void OnViewportHover(const glm::vec2& worldPos);
    bool OnViewportClick(const glm::vec2& worldPos, bool isRightClick);
    void EndViewportPaint() { m_hasLastPaint = false; }

    void SetActive(bool active) { m_active = active; }
    bool IsActive() const { return m_active; }
    // Toggle whether the palette captures viewport input for painting.
    void SetPaintMode(bool enabled) { m_paintMode = enabled; }
    // Gate viewport input handling to valid tile-editing state.
    bool CanHandleViewportInput() const { return m_active && m_paintMode && m_tileMap && m_tileset; }
    // Access current tilemap data for viewport overlays.
    const std::shared_ptr<TileMap>& GetTileMap() const { return m_tileMap; }
    const std::shared_ptr<Tileset>& GetTileset() const { return m_tileset; }
    const glm::vec2& GetTileMapOrigin() const { return m_worldOrigin; }
    // Allow external systems (LevelEditor, SceneViewport) to forward dropped assets.
    void SetAssetDropCallback(std::function<void(const std::string&)> callback) { m_assetDropCallback = std::move(callback); }
    void HandleAssetDrop(const std::string& assetPath);
    // Update the list of tilemaps for the active tilemap dropdown.
    void SetTileMapList(const std::vector<TileMapListEntry>& entries, EntityId activeId);
    // Notify LevelEditor when the active tilemap is changed via the dropdown.
    void SetActiveTileMapCallback(std::function<void(EntityId)> callback) { m_activeTileMapCallback = std::move(callback); }
    // Notify LevelEditor when the active tileset changes.
    void SetActiveTilesetCallback(std::function<void(uint8_t)> callback) { m_activeTilesetCallback = std::move(callback); }
    // Update the world-space origin without resetting selection state.
    void SetTileMapOrigin(const glm::vec2& origin) { m_worldOrigin = origin; }

private:
    bool m_active = true;
    bool m_paintMode = true;
    
    std::shared_ptr<TileMap> m_tileMap;
    std::shared_ptr<Tileset> m_tileset;
    std::vector<std::shared_ptr<Tileset>> m_tilesets;
    std::vector<std::string> m_tilesetPaths;
    std::string m_tileMapPath;
    glm::vec2 m_worldOrigin{0.0f, 0.0f}; // Tilemap origin in world space (entity transform).
    std::function<void(const std::string&)> m_assetDropCallback;
    std::function<void(EntityId)> m_activeTileMapCallback;
    std::function<void(uint8_t)> m_activeTilesetCallback;
    ECS::World* m_world = nullptr;

    std::vector<TileMapListEntry> m_tileMapList;
    EntityId m_activeTileMapId = ECS::Entity::NPOS32;
    uint8_t m_activeTilesetIndex = 0;
    TileID m_selectedTileID = 0;   // Base ID selected in palette
    uint8_t m_currentRotation = 0; // 0..3
	bool m_isEraser = false;       // Eraser mode toggle
	bool m_hasLastPaint = false;   // Track last painted tile to avoid redundant paints
	bool m_lastPaintErase = false; // Whether the last paint was an erase
	int64_t m_lastPaintKey = 0;    // Packed coordinate of last painted tile

    // Physics Sync State
    // Key: (x << 16) | y. Value: Entity handle.
    std::map<int64_t, ECS::Entity> m_physicsEntities;

    void SyncPhysics(int32_t x, int32_t y, TileID id, bool isEraser);
    int64_t PackCoord(int32_t x, int32_t y) const {
        return (static_cast<int64_t>(x) << 32) | (static_cast<uint32_t>(y));
    }
};
