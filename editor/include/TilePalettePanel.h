/* Start Header *****************************************************************/
/*!
\file    TilePalettePanel.hpp
\author  Samantha Leong (60%)
         Foo Rui Qin (40%)
\par     s.leong@digipen.edu
         ruiqin.foo@digipen.edu
\date    11th March 2026
\brief
Defines the TilePalettePanel editor component responsible for tile
selection, painting, viewport interaction and physics synchronization.

This panel acts as the bridge between editor UI input and tilemap
data manipulation. It coordinates tile painting, undo integration,
asset switching and collision syncing without owning rendering logic.

Key responsibilities:
- Tile palette UI interaction
- Tile painting and erasing
- Undo system integration
- Physics collider synchronization
- Tilemap/tileset switching
- Viewport hover and paint handling

Dependencies:
- TileMap for tile storage
- Tileset for tile definitions
- ECS world for physics entities
- Undo system for reversible edits
*/
/* End Header *******************************************************************/

#ifndef TILE_PALETTE_PANEL_H
#define TILE_PALETTE_PANEL_H

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <unordered_map>
#include <glm/glm.hpp>

#include "core/World/TileMap.hpp"
#include "core/World/Tileset.hpp"
#include "ecs/Entity.h"

struct ImFont;
namespace ECS { class World; }
namespace Editor { class UndoSystem; }

// Editor panel for tile palette management, viewport painting and collision syncing
// Does not own rendering systems; coordinates editor interactions only
class TilePalettePanel {
public:
    // -------------------------------------------------------------------------
    // Types
    // -------------------------------------------------------------------------

    // Entry describing a selectable tilemap in the editor dropdown
    struct TileMapListEntry {
        EntityId Id;        // Tilemap entity identifier
        std::string Name;   // Display name shown in the dropdown
    };

    TilePalettePanel() = default;
    ~TilePalettePanel() = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    // Initialize palette state, ECS linkage and font references
    void Initialize(const std::shared_ptr<TileMap>& tileMap, const std::shared_ptr<Tileset>& tileset,
        ECS::World* world, ImFont* symbolsFont, ImFont* boldFont);

    // -------------------------------------------------------------------------
    // System Wiring
    // -------------------------------------------------------------------------

    // Set the ECS world for physics entity management
    void SetWorld(ECS::World* world) { m_world = world; }

    // Set the undo system for recording tile edits
    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }

    // Update the active tilemap, tilesets and map path used for save operations
    // Resets painting state while preserving UI linkage
    void SetEditingContext(const std::shared_ptr<TileMap>& tileMap,
        const std::vector<std::shared_ptr<Tileset>>& tilesets,
        const std::vector<std::string>& tilesetPaths,
        uint8_t activeTilesetIndex,
        const std::string& tileMapPath,
        const glm::vec2& worldOrigin);

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    // Render the ImGui tile palette window
    void Render();

    // -------------------------------------------------------------------------
    // Viewport Interaction
    // -------------------------------------------------------------------------

    // Draw tile hover preview at the given world-space cursor position
    void OnViewportHover(const glm::vec2& worldPos);

    // Draw collision brush hover preview at the given world-space cursor position
    void OnViewportCollisionHover(const glm::vec2& worldPos);

    // Apply a tile paint or erase action; returns true if any tile changed
    bool OnViewportClick(const glm::vec2& worldPos, bool isRightClick);

    // End drag-paint tracking state
    void EndViewportPaint() { m_hasLastPaint = false; }

    // Apply a collision mask paint action; returns true if any mask changed
    bool OnViewportCollisionClick(const glm::vec2& worldPos);

    // End collision paint drag tracking state
    void EndViewportCollisionPaint() { m_collisionHasLastPaint = false; }

    // -------------------------------------------------------------------------
    // State Control
    // -------------------------------------------------------------------------

    // Enable or disable collision edit mode
    void SetCollisionEditActive(bool active);

    // Return true if collision edit mode is active
    bool IsCollisionEditActive() const { return m_collisionEditActive; }

    // Enable or disable palette interaction
    void SetActive(bool active) { m_active = active; }

    // Return true if the palette is active
    bool IsActive() const { return m_active; }

    // Enable or disable viewport paint capture
    void SetPaintMode(bool enabled) { m_paintMode = enabled; }

    // Return true if paint mode is enabled
    bool IsPaintModeEnabled() const { return m_paintMode; }

    // Return the entity ID of the currently active tilemap
    EntityId GetActiveTileMapId() const { return m_activeTileMapId; }

    // -------------------------------------------------------------------------
    // Capability Checks
    // -------------------------------------------------------------------------

    // Return true if viewport hover can be processed for active painting tools
    bool CanHandleViewportHover() const { return m_active && (m_paintMode || m_collisionEditActive) && m_tileMap && m_tileset; }

    // Return true if viewport tile painting is allowed in the current state
    bool CanHandleViewportPaint() const { return m_active && m_paintMode && m_tileMap && m_tileset && !m_collisionEditActive; }

    // Return true if viewport collision painting is allowed in the current state
    bool CanHandleViewportCollisionPaint() const { return m_active && m_collisionEditActive && m_tileMap && m_tileset; }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    // Return the active tilemap for viewport overlays
    const std::shared_ptr<TileMap>& GetTileMap() const { return m_tileMap; }

    // Return the active tileset for tile data
    const std::shared_ptr<Tileset>& GetTileset() const { return m_tileset; }

    // Return the world-space origin of the active tilemap
    const glm::vec2& GetTileMapOrigin() const { return m_worldOrigin; }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    // Register a callback for asset drops forwarded from external systems
    void SetAssetDropCallback(std::function<void(const std::string&)> callback) { m_assetDropCallback = std::move(callback); }

    // Register a callback invoked when the active tilemap changes via the dropdown
    void SetActiveTileMapCallback(std::function<void(EntityId)> callback) { m_activeTileMapCallback = std::move(callback); }

    // Register a callback invoked when the active tileset changes
    void SetActiveTilesetCallback(std::function<void(uint8_t)> callback) { m_activeTilesetCallback = std::move(callback); }

    // Register a callback invoked when paint mode is toggled
    void SetPaintModeChangedCallback(std::function<void(bool)> callback) { m_onPaintModeChanged = std::move(callback); }

    // -------------------------------------------------------------------------
    // Asset Handling
    // -------------------------------------------------------------------------

    // Handle a forwarded asset drop (e.g. tileset dragged from asset browser)
    void HandleAssetDrop(const std::string& assetPath);

    // Clear cached per-tilemap tile preview sizes and reset to default
    void ClearTilePreviewSizeCache();

    // Update the tilemap dropdown list and set the active tilemap ID
    void SetTileMapList(const std::vector<TileMapListEntry>& entries, EntityId activeId);

    // Update the world-space origin without resetting palette selection state
    void SetTileMapOrigin(const glm::vec2& origin) { m_worldOrigin = origin; }

    // Update the tilemap asset path without resetting palette selection state
    void SetTileMapPath(const std::string& path) { m_tileMapPath = path; }

private:
    // -------------------------------------------------------------------------
    // Internal Helpers
    // -------------------------------------------------------------------------

    // Build a cache key from the active tileset ID and tilemap path
    // Used to persist user-adjusted tile preview sizes across tilemap switches
    std::string GetActiveTilesetPreviewKey() const;

    // Restore the cached tile preview size for the currently active tileset
    void RefreshTilePreviewSizeForActiveTileset();

    // Pack signed tile coordinates into a unique 64-bit key
    int64_t PackCoord(int32_t x, int32_t y) const {
        return (static_cast<int64_t>(x) << 32) | (static_cast<uint32_t>(y));
    }

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    bool m_active = true;                   // Whether palette interaction is enabled
    bool m_paintMode = true;                // Whether viewport paint capture is enabled
    bool m_collisionEditActive = false;     // Whether collision edit mode is active
    bool m_collisionPrevPaintMode = true;   // Cached paint mode state before entering collision edit
    uint8_t m_collisionBrushMask = 0x0F;    // 4-bit brush mask for 2x2 subcell collision painting
    bool m_collisionEraser = false;         // Whether collision eraser mode is active
    bool m_collisionHasLastPaint = false;   // Whether a collision paint was made in the current drag
    uint8_t m_collisionLastPaintMask = 0;   // Mask applied at the last collision paint position
    int64_t m_collisionLastPaintKey = 0;    // Packed coordinate of the last collision paint position

    // Active tilemap and tilesets
    std::shared_ptr<TileMap> m_tileMap;
    std::shared_ptr<Tileset> m_tileset;
    std::vector<std::shared_ptr<Tileset>> m_tilesets;

    // Tileset metadata
    std::vector<std::string> m_tilesetPaths;   // File paths of the loaded tileset assets
    std::string m_tileMapPath;                 // File path of the active tilemap asset

    glm::vec2 m_worldOrigin{ 0.0f, 0.0f };     // World-space origin of the active tilemap

    // Editor callbacks
    std::function<void(const std::string&)> m_assetDropCallback;    // Forwarded asset drop handler
    std::function<void(EntityId)> m_activeTileMapCallback;          // Active tilemap change handler
    std::function<void(uint8_t)> m_activeTilesetCallback;           // Active tileset change handler
    std::function<void(bool)> m_onPaintModeChanged;                 // Paint mode toggle handler

    // External systems
    ECS::World* m_world = nullptr;              // ECS world for physics entity management
    Editor::UndoSystem* m_undoSystem = nullptr; // Undo system for recording tile edits
    ImFont* m_symbolsFont = nullptr;            // Symbols/icon font for toolbar buttons
    ImFont* m_boldFont = nullptr;               // Bold font for section headers

    // Tilemap UI state
    std::vector<TileMapListEntry> m_tileMapList;                          // List of tilemaps shown in the dropdown
    EntityId m_activeTileMapId = ECS::Entity::NPOS32;                     // Active tilemap entity ID for dropdown selection
    uint8_t m_activeTilesetIndex = 0;                                     // Index of the active tileset in m_tilesets
    float m_tilePreviewSize = 64.0f;                                      // Current tile preview size in pixels
    static constexpr float kTilePreviewMin = 24.0f;                       // Minimum allowed tile preview size
    static constexpr float kTilePreviewMax = 96.0f;                       // Maximum allowed tile preview size
    std::unordered_map<std::string, float> m_tilePreviewSizeByTileset;    // Per-tileset preview sizes keyed by tileset + tilemap context

    // Painting state
    TileID m_selectedTileID = 0;       // Tile ID currently selected in the palette
    uint8_t m_currentRotation = 0;     // Current tile rotation: 0 = 0°, 1 = 90°, 2 = 180°, 3 = 270°
    bool m_isEraser = false;           // Whether eraser mode is active
    bool m_hasLastPaint = false;       // Whether a tile was painted in the current drag
    bool m_lastPaintErase = false;     // Whether the last paint action was an erase
    int64_t m_lastPaintKey = 0;        // Packed coordinate of the last painted tile
};

#endif  // TILE_PALETTE_PANEL_H