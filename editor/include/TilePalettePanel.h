#pragma once

#include <memory>
#include <map>
#include <string>
#include <functional>
#include <glm/glm.hpp>

#include "../../engine/include/core/World/TileMap.hpp"
#include "../../engine/include/core/World/Tileset.hpp"
#include "ecs/Entity.h"

namespace ECS { class World; }

class TilePalettePanel {
public:
    TilePalettePanel() = default;
    ~TilePalettePanel() = default;

    void Initialize(const std::shared_ptr<TileMap>& tileMap, const std::shared_ptr<Tileset>& tileset, ECS::World* world);
    void SetWorld(ECS::World* world) { m_world = world; }
    // Update the active tilemap/tileset and map path used for save operations.
    void SetEditingContext(const std::shared_ptr<TileMap>& tileMap, const std::shared_ptr<Tileset>& tileset, const std::string& tileMapPath);

    void Render(); // ImGui Palette Window

    // Interaction hooks
    void OnViewportHover(const glm::vec2& worldPos);
    bool OnViewportClick(const glm::vec2& worldPos, bool isRightClick);

    void SetActive(bool active) { m_active = active; }
    bool IsActive() const { return m_active; }
    // Gate viewport input handling to valid tile-editing state.
    bool CanHandleViewportInput() const { return m_active && m_tileMap && m_tileset; }
    // Allow external systems (LevelEditor, SceneViewport) to forward dropped assets.
    void SetAssetDropCallback(std::function<void(const std::string&)> callback) { m_assetDropCallback = std::move(callback); }
    void HandleAssetDrop(const std::string& assetPath);

private:
    bool m_active = true;
    
    std::shared_ptr<TileMap> m_tileMap;
    std::shared_ptr<Tileset> m_tileset;
    std::string m_tileMapPath;
    std::function<void(const std::string&)> m_assetDropCallback;
    ECS::World* m_world = nullptr;

    TileID m_selectedTileID = 0; // Base ID selected in palette
    uint8_t m_currentRotation = 0; // 0..3
    bool m_isEraser = false;

    // Physics Sync State
    // Key: (x << 16) | y. Value: Entity handle.
    std::map<uint32_t, ECS::Entity> m_physicsEntities;

    void SyncPhysics(uint32_t x, uint32_t y, TileID id, bool isEraser);
    uint32_t PackCoord(uint32_t x, uint32_t y) const { return (x << 16) | y; }
};
