/* Start Header *****************************************************************/
/*!
\file    TilePalettePanel.hpp
\author Samantha Leong 
\par    s.leong@digipen.edu
\date   3rd February 2026
\brief
Defines the TilePalettePanel editor component responsible for tile
selection, painting, viewport interaction, and physics synchronization.

This panel acts as the bridge between editor UI input and tilemap
data manipulation. It coordinates tile painting, undo integration,
asset switching, and collision syncing without owning rendering logic.

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

#pragma once

#include <memory>
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <glm/glm.hpp>

#include "core/World/TileMap.hpp"
#include "core/World/Tileset.hpp"
#include "ecs/Entity.h"

struct ImFont;
namespace ECS { class World; }
namespace Editor { class UndoSystem; }

/*------------------------------------------------------------------*/
/*!
\class TilePalettePanel
\brief Editor panel for tile palette management and painting.

Provides UI-driven tile selection, viewport painting logic,
collision syncing, and editor callbacks. This class does not
own rendering systems but coordinates editor interactions.
*/
class TilePalettePanel {
public:
    /*------------------------------------------------------------------*/
    /*!
    \struct TileMapListEntry
    \brief Entry describing a selectable tilemap in the editor UI.
    */    
    struct TileMapListEntry {
        EntityId Id;        //!< Tilemap entity identifier
        std::string Name;   //!< Display name in dropdown
    };

    TilePalettePanel() = default;
    ~TilePalettePanel() = default;

    /*------------------------------------------------------------------*/
    /*!
    \brief Initializes palette state and ECS linkage.

    \param tileMap Initial tilemap reference
    \param tileset Initial tileset reference
    \param world ECS world pointer for physics syncing
    */
    void Initialize(const std::shared_ptr<TileMap>& tileMap, const std::shared_ptr<Tileset>& tileset,
        ECS::World* world, ImFont* symbolsFont);
     /*------------------------------------------------------------------*/
    /*!
    \brief Assigns ECS world for physics entity management.
    */
    void SetWorld(ECS::World* world) { m_world = world; }

	/*------------------------------------------------------------------*/
    /*!
    \brief Assigns undo system for tile edit tracking.
    */
    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }

    /*------------------------------------------------------------------*/
    /*!
    \brief Updates active tilemap, tilesets, and editor metadata.

    Resets painting state while preserving UI linkage.

    */
    // Update the active tilemap/tileset and map path used for save operations.
    void SetEditingContext(const std::shared_ptr<TileMap>& tileMap,
        const std::vector<std::shared_ptr<Tileset>>& tilesets,
        const std::vector<std::string>& tilesetPaths,
        uint8_t activeTilesetIndex,
        const std::string& tileMapPath,
        const glm::vec2& worldOrigin);

     /*------------------------------------------------------------------*/
    /*!
    \brief Renders the ImGui tile palette window.
    */
    void Render(); // ImGui Palette Window

    // Interaction hooks
     /*------------------------------------------------------------------*/
    /*!
    \brief Draws tile hover preview inside the viewport.

    \param worldPos Cursor position in world space
    */
    void OnViewportHover(const glm::vec2& worldPos);
    /*------------------------------------------------------------------*/
    /*!
    \brief Applies tile paint or erase action.

    \param worldPos Cursor position
    \param isRightClick True when erasing
    \return True if tile changed
    */
    bool OnViewportClick(const glm::vec2& worldPos, bool isRightClick);

    /*------------------------------------------------------------------*/
    /*!
    \brief Ends drag-paint tracking state.
    */
    void EndViewportPaint() { m_hasLastPaint = false; }

    /*------------------------------------------------------------------*/
    /*!
    \brief Enables or disables palette interaction.
    */
    void SetActive(bool active) { m_active = active; }
    
    /*------------------------------------------------------------------*/
    /*!
    \brief Returns whether palette is active.
    */
    bool IsActive() const { return m_active; }

     /*------------------------------------------------------------------*/
    /*!
    \brief Toggles viewport paint capture.
    */
    // Toggle whether the palette captures viewport input for painting.
    void SetPaintMode(bool enabled) { m_paintMode = enabled; }

    /*------------------------------------------------------------------*/
    /*!
    \brief Checks if viewport hover can be processed.
    */
    // Gate viewport hover handling to valid tile-editing state.
    bool CanHandleViewportHover() const { return m_active && m_tileMap && m_tileset; }

    /*------------------------------------------------------------------*/
    /*!
    \brief Checks if viewport painting is allowed.
    */
    // Gate viewport paint handling to valid tile-editing state.
    bool CanHandleViewportPaint() const { return m_active && m_paintMode && m_tileMap && m_tileset; }

    /*------------------------------------------------------------------*/
    /*!
    \brief Accessors for active tilemap and tileset.
    */
    // Access current tilemap data for viewport overlays.
    const std::shared_ptr<TileMap>& GetTileMap() const { return m_tileMap; }
    const std::shared_ptr<Tileset>& GetTileset() const { return m_tileset; }

    /*------------------------------------------------------------------*/
    /*!
    \brief Returns world origin of tilemap.
    */
    const glm::vec2& GetTileMapOrigin() const { return m_worldOrigin; }

    /*------------------------------------------------------------------*/
    /*!
    \brief Registers asset drop callback handler.
    */
    // Allow external systems (LevelEditor, SceneViewport) to forward dropped assets.
    void SetAssetDropCallback(std::function<void(const std::string&)> callback) { m_assetDropCallback = std::move(callback); }

    /*------------------------------------------------------------------*/
    /*!
    \brief Handles forwarded asset drop.
    */
    void HandleAssetDrop(const std::string& assetPath);

    /*------------------------------------------------------------------*/
    /*!
    \brief Updates tilemap dropdown list.
    */
    // Update the list of tilemaps for the active tilemap dropdown.
    void SetTileMapList(const std::vector<TileMapListEntry>& entries, EntityId activeId);

    /*------------------------------------------------------------------*/
    /*!
    \brief Registers active tilemap change callback.
    */
    // Notify LevelEditor when the active tilemap is changed via the dropdown.
    void SetActiveTileMapCallback(std::function<void(EntityId)> callback) { m_activeTileMapCallback = std::move(callback); }

    /*------------------------------------------------------------------*/
    /*!
    \brief Registers tileset change callback.
    */
    // Notify LevelEditor when the active tileset changes.
    void SetActiveTilesetCallback(std::function<void(uint8_t)> callback) { m_activeTilesetCallback = std::move(callback); }

    /*------------------------------------------------------------------*/
    /*!
    \brief Updates world origin without resetting palette state.
    */
    // Update the world-space origin without resetting selection state.
    void SetTileMapOrigin(const glm::vec2& origin) { m_worldOrigin = origin; }

private:
    bool m_active = true;           //!< Palette enabled state
    bool m_paintMode = true;        //!< Viewport painting enabled
    
        //! Active tilemap and tilesets
    std::shared_ptr<TileMap> m_tileMap;
    std::shared_ptr<Tileset> m_tileset;
    std::vector<std::shared_ptr<Tileset>> m_tilesets;

    //! Tileset metadata
    std::vector<std::string> m_tilesetPaths;
    std::string m_tileMapPath;

    //! Tilemap world origin
    glm::vec2 m_worldOrigin{0.0f, 0.0f}; // Tilemap origin in world space (entity transform).
    
    //! Editor callbacks
    std::function<void(const std::string&)> m_assetDropCallback;
    std::function<void(EntityId)> m_activeTileMapCallback;
    std::function<void(uint8_t)> m_activeTilesetCallback;

    //! External systems
    ECS::World* m_world = nullptr;
    Editor::UndoSystem* m_undoSystem = nullptr;
    ImFont* m_symbolsFont = nullptr;

    //! Tilemap UI state
    std::vector<TileMapListEntry> m_tileMapList;
    EntityId m_activeTileMapId = ECS::Entity::NPOS32;
    uint8_t m_activeTilesetIndex = 0;

    //! Painting state
    TileID m_selectedTileID = 0;   // Base ID selected in palette
    uint8_t m_currentRotation = 0; // 0..3
	bool m_isEraser = false;       // Eraser mode toggle
	bool m_hasLastPaint = false;   // Track last painted tile to avoid redundant paints
	bool m_lastPaintErase = false; // Whether the last paint was an erase
	int64_t m_lastPaintKey = 0;    // Packed coordinate of last painted tile

     /*!
    Physics collider entity cache.
    Key = packed tile coordinate.
    */
    // Physics Sync State
    // Key: (x << 16) | y. Value: Entity handle.
    std::map<int64_t, ECS::Entity> m_physicsEntities;

    /*------------------------------------------------------------------*/
    /*!
    \brief Synchronizes physics collider state with tile edits.
    */
    void SyncPhysics(int32_t x, int32_t y, TileID id, bool isEraser);

    /*------------------------------------------------------------------*/
    /*!
    \brief Packs signed tile coordinates into a unique key.
    */
    int64_t PackCoord(int32_t x, int32_t y) const {
        return (static_cast<int64_t>(x) << 32) | (static_cast<uint32_t>(y));
    }
};
