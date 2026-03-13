/* Start Header *****************************************************************/
/*!
\file   LevelEditor.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th March 2026
\brief
Header for LevelEditor class - main orchestrator for the game editor interface.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef LEVEL_EDITOR_H
#define LEVEL_EDITOR_H

#include "ecs/World.h"
#include "scene/Scene.h"
#include "PlaybackControls.h"
#include "EditorFileMenu.h"
#include "AssetBrowserPanel.h"
#include "SceneViewport.h"
#include "GameViewport.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "LayersPanel.h"
#include "EditorEntityActions.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "ConsolePanel.h"
#include "PerformancePanel.h"
#include "SpriteImportPanel.h"
#include "SystemsPanel.h"
#include "TilePalettePanel.h"

// Configuration for font sizes and toolbar layout
struct LevelEditorConfig {
    float TextFontSize = 20.0f;     // Main body font size in pixels
    float IconFontSize = 22.0f;     // Icon/symbol font size in pixels
    float ToolbarHeight = 46.0f;    // Height of the playback toolbar in pixels
};

// Registration entry for a panel in the editor panel system
struct PanelRegistration {
    const char* Name = nullptr;                           // Display name used for docking and window titles
    std::function<void()> InitializeCallback;             // Called once during editor initialization
    std::function<void()> RenderCallback;                 // Called every frame to draw the panel
    std::function<void(ECS::World*)> SetWorldCallback;    // Called when the active world changes
};

struct EditorSettings;

class LevelEditor {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    LevelEditor(ECS::World* world, const LevelEditorConfig& config, Scenes::Scene* scene);
    ~LevelEditor();

    // Initialize ImGui, load fonts, register panels and build the dock layout
    void Initialize(const GLFWwindow* pWin);

    // Begin a new ImGui frame
    void BeginFrame();

    // Process per-frame editor logic including playback and shortcut handling
    void Update();

    // Render all registered panels and the dockspace
    void Render();

    // Submit the ImGui draw data and swap buffers
    void EndFrame();

    // Update the active ECS world reference across all panels
    void SetWorld(ECS::World* world);

    // Set the active scene pointer so EntityActions can operate immediately
    void SetScene(Scenes::Scene* scene);

    // Notify the editor that the window has been resized
    void OnWindowResized(int width, int height);

    // Return true if the editor is currently in Play or Step mode
    bool IsPlaying() const;

    // Return true if a single-step advance has been requested
    bool IsStepRequested() const;

    // Clear the pending step request after it has been consumed
    void ClearStepRequest();

    // Return the current editor state (Edit, Play, Pause, Step)
    EditorState GetEditorState() const;

    // Return true if a valid ECS world is assigned
    bool HasValidWorld() const { return m_world != nullptr; }

    // Set callback to invoke when the project browser should be opened
    void SetProjectBrowserRequestCallback(std::function<void()> callback) { m_projectBrowserRequest = std::move(callback); }

    // Apply editor settings to all panels that use them
    void SetEditorSettings(EditorSettings* settings);

private:
    // -------------------------------------------------------------------------
    // Panel System
    // -------------------------------------------------------------------------

    // Register a panel with its init, render and world-update callbacks
    void _registerPanel(const char* panelName,
        const std::function<void()>& initFn,
        const std::function<void()>& renderFn,
        const std::function<void(ECS::World*)>& setWorldFn
    );

    // Initialize all registered panels in registration order
    void _initializePanels();

    // Render all registered panels in registration order
    void _renderPanels();

    // Propagate a new world reference to all registered panels
    void _updatePanelWorlds(ECS::World* world);

    // -------------------------------------------------------------------------
    // Docking
    // -------------------------------------------------------------------------

    // Build the default dock layout on first frame
    void _buildDockLayout();

    // Render the fullscreen dockspace that panels attach to
    void _renderDockSpace();

    // -------------------------------------------------------------------------
    // Font Loading
    // -------------------------------------------------------------------------

    // Load and configure main, bold and symbol fonts at the current UI scale
    void _loadFonts();

    // -------------------------------------------------------------------------
    // Event Handlers
    // -------------------------------------------------------------------------

    // Handle playback state transitions (e.g. Edit -> Play, Play -> Edit)
    void _onPlaybackStateChanged(EditorState oldState, EditorState newState);

    // Handle entity selection changes originating from the viewport
    void _onViewportSelectionChanged(EntityId id);

    // Handle asset selection from the asset browser (e.g. tileset drag-drop)
    void _onAssetSelected(const std::string& assetPath);

    // Sync tile palette and editor state from a selected entity's tilemap component
    void _syncTilePaletteToSelection(EntityId id);

    // Save all cached tilemap assets to disk before scene serialization
    void _saveActiveTileMapAsset(const std::string& scenePath);

    // Refresh cached tilemaps and push all visible tilemaps to the renderer
    void _refreshTileMapCache();

    // Apply a tileset asset path to an existing tilemap entity
    void _applyTilesetToTilemap(ECS::Entity entity, const std::string& assetPath);

    // Activate a tilemap entity for palette editing without changing hierarchy selection
    void _setActiveTileMap(EntityId id);

    // -------------------------------------------------------------------------
    // Core State
    // -------------------------------------------------------------------------

    ECS::World* m_world = nullptr;      // Active ECS world containing all entities and components
    LevelEditorConfig m_config;         // Font sizes and toolbar layout configuration

    // -------------------------------------------------------------------------
    // Panels
    // -------------------------------------------------------------------------

    Playback m_playback;                        // Playback toolbar and state machine
    EditorFileMenu m_fileMenu;                  // File/Edit/View menu bar
    AssetBrowserPanel m_assetBrowser;           // Asset browser for file navigation
    SceneViewport m_sceneViewport;              // Scene editor viewport
    GameViewport m_gameViewport;                // Game preview viewport
    HierarchyPanel m_hierarchyWindow;           // Entity hierarchy tree
    InspectorPanel m_inspector;                 // Component inspector
    LayersPanel m_layersPanel;                  // Layer visibility and configuration
    EntityActions m_entityActions;              // Entity create/clone/delete operations
    ConsolePanel m_console;                     // Log output console
    PerformancePanel m_performancePanel;        // CPU/GPU/memory performance stats
    SpriteImportPanel m_spriteImportPanel;      // Sprite sheet import and slicing tool
    SystemsPanel m_systemsPanel;                // ECS system enable/disable controls
    TilePalettePanel m_tilePalette;             // Tile palette for tilemap editing

    // -------------------------------------------------------------------------
    // Tilemap Cache
    // -------------------------------------------------------------------------

    // Per-entity cached tilemap data kept alive for the renderer and palette
    struct TileMapCacheEntry {
        std::shared_ptr<TileMap> Map;               // Loaded tilemap asset
        std::vector<std::shared_ptr<Tileset>> Tilesets; // Associated tileset assets
        std::string MapPath;                        // File path of the tilemap asset
        std::vector<std::string> TilesetPaths;      // File paths of the tileset assets
        float TileWorldSize = 1.0f;                 // World-space size of one tile
        uint32_t TilePixelSize = 32;                // Pixel size of one tile in the tileset
        uint32_t DefaultWidth = 64;                 // Default map width in tiles
        uint32_t DefaultHeight = 64;                // Default map height in tiles
        glm::vec2 Origin{ 0.0f, 0.0f };             // World-space origin of the tilemap
        bool Visible = true;                        // Whether the tilemap is rendered
        std::string DisplayName;                    // Human-readable name for UI display
        uint8_t ActiveTilesetIndex = 0;             // Index of the currently active tileset
        uint32_t Generation = 0;                    // Entity generation for detecting scene reloads
    };

    std::shared_ptr<TileMap> m_activeTileMap;           // Currently active tilemap for palette editing
    std::shared_ptr<Tileset> m_activeTileset;           // Currently active tileset for tile painting
    std::string m_activeTileMapPath;                    // File path of the active tilemap asset
    std::string m_activeTilesetPath;                    // File path of the active tileset asset
    EntityId m_activeTileMapEntityId = ECS::Entity::NPOS32;           // Entity owning the active tilemap
    std::unordered_map<EntityId, TileMapCacheEntry> m_tileMapCache;   // Cache of all live tilemap entities
    std::unordered_map<EntityId, TileMapCacheEntry> m_detachedTileMapCache; // Tilemaps for undo-removed entities, restorable on redo
    std::vector<TilePalettePanel::TileMapListEntry> m_tileMapList;    // Flat list of tilemaps shown in the palette dropdown

    // -------------------------------------------------------------------------
    // Panel Registry
    // -------------------------------------------------------------------------

    std::vector<PanelRegistration> m_panelRegistry; // Ordered list of all registered panels

    // -------------------------------------------------------------------------
    // UI Resources
    // -------------------------------------------------------------------------

    ImFont* m_symbolsFont = nullptr;        // Symbols/icon font for icon-only buttons
    ImFont* m_mainFont = nullptr;           // Main body font
    ImFont* m_boldFont = nullptr;           // Bold font for headers and labels
    float m_uiScale = 1.0f;                 // Global UI scale factor
    EditorSettings* m_editorSettings = nullptr; // Editor settings for view mode and config

    // -------------------------------------------------------------------------
    // Docking State
    // -------------------------------------------------------------------------

    ImGuiID m_dockspaceId = 0;              // ID of the root dockspace
    bool m_dockLayoutBuilt = false;         // Whether the default dock layout has been built
    ImVec2 m_lastViewportSize = ImVec2(0, 0); // Last known viewport size for layout rebuilds
    int m_viewportLayoutPreset = 1;         // Active scene/game viewport layout preset (1/2/4 views)

    // -------------------------------------------------------------------------
    // Editor State
    // -------------------------------------------------------------------------

    EditorState m_lastEditorState = EditorState::Edit;  // Previous editor state for transition detection
    bool m_focusTilePaletteNextFrame = false;           // Deferred focus request for tile palette panel
    bool m_suppressViewportSelectionSync = false;       // Suppress viewport callbacks when hierarchy drives selection
    Editor::UndoSystem m_undoSystem;                    // Undo/redo system for all editor actions
    std::function<void()> m_projectBrowserRequest;      // Callback to open the project browser

    // -------------------------------------------------------------------------
    // Message Subscriptions
    // -------------------------------------------------------------------------

    Messaging::SubscriptionHandle m_entityCreatedSubscription;         // Listens for entity creation events
    Messaging::SubscriptionHandle m_entityDestroyedSubscription;       // Listens for entity destruction events
    Messaging::SubscriptionHandle m_sceneModifiedSubscription;         // Listens for scene modification events
    Messaging::SubscriptionHandle m_tileMapCollisionEditSubscription;  // Listens for tilemap collision edit requests
    Messaging::SubscriptionHandle m_viewportLayoutSubscription;        // Listens for viewport layout change requests
};

#endif // LEVEL_EDITOR_H