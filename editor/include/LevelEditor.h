/* Start Header *****************************************************************/
/*!
\file   LevelEditor.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
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
#include "GameConfigPanel.h"

struct LevelEditorConfig {
    float TextFontSize = 20.0f;
    float IconFontSize = 22.0f;
    float ToolbarHeight = 46.0f;
};

struct PanelRegistration {
    const char* Name = nullptr;
    std::function<void()> InitializeCallback;
    std::function<void()> RenderCallback;
    std::function<void(ECS::World*)> SetWorldCallback;
};

class LevelEditor {
public:
    LevelEditor(ECS::World* world, const LevelEditorConfig& config, Scenes::Scene* scene);
    ~LevelEditor();

    void Initialize(const GLFWwindow* pWin);
    void BeginFrame();
    void Update();
    void Render();
    void EndFrame();
    void SetWorld(ECS::World* world);
    // Set active scene pointer so EntityActions can operate immediately
    void SetScene(Scenes::Scene* scene);

    void OnWindowResized(int width, int height);

    bool IsPlaying() const;
    bool IsStepRequested() const;
    void ClearStepRequest();
    EditorState GetEditorState() const;
    bool HasValidWorld() const { return m_world != nullptr; }
    void SetProjectBrowserRequestCallback(std::function<void()> callback) { m_projectBrowserRequest = std::move(callback); }

private:
    // Panel registration system
    void _registerPanel(const char* panelName,
        const std::function<void()>& initFn,
        const std::function<void()>& renderFn,
        const std::function<void(ECS::World*)>& setWorldFn
    );
    void _initializePanels();
    void _renderPanels();
    void _updatePanelWorlds(ECS::World* world);

    // Docking
    void _buildDockLayout();
    void _renderDockSpace();

    // Font loading
    void _loadFonts();

    // Event handlers
    void _onPlaybackStateChanged(EditorState oldState, EditorState newState);
    void _onViewportSelectionChanged(EntityId id);
    // Asset selection handler for tileset -> tile palette wiring.
    void _onAssetSelected(const std::string& assetPath);
    // Sync tile palette/editor state from a selected entity (if it has a tilemap component).
    void _syncTilePaletteToSelection(EntityId id);
    // Save all cached tilemap assets to disk before scene serialization.
    void _saveActiveTileMapAsset(const std::string& scenePath);
    // Refresh cached tilemaps and push all visible tilemaps to the renderer.
    void _refreshTileMapCache();
    // Apply a tileset asset path to an existing tilemap entity.
    void _applyTilesetToTilemap(ECS::Entity entity, const std::string& assetPath);
    // Activate the tilemap for palette editing without changing hierarchy selection.
    void _setActiveTileMap(EntityId id);

    // Core state
    ECS::World* m_world = nullptr;
    LevelEditorConfig m_config;

    // Panels
    Playback m_playback;
    EditorFileMenu m_fileMenu;
    AssetBrowserPanel m_assetBrowser;
    SceneViewport m_sceneViewport;
    GameViewport m_gameViewport;
    HierarchyPanel m_hierarchyWindow;
    InspectorPanel m_inspector;
    LayersPanel m_layersPanel;
    EntityActions m_entityActions;
    ConsolePanel m_console;
    PerformancePanel m_performancePanel;
    SpriteImportPanel m_spriteImportPanel;
    SystemsPanel m_systemsPanel;
    TilePalettePanel m_tilePalette;
    GameConfigPanel m_gameConfigPanel;

    struct TileMapCacheEntry {
        std::shared_ptr<TileMap> Map;
        std::vector<std::shared_ptr<Tileset>> Tilesets;
        std::string MapPath;
        std::vector<std::string> TilesetPaths;
        float TileWorldSize = 1.0f;
        uint32_t TilePixelSize = 32;
        uint32_t DefaultWidth = 64;
        uint32_t DefaultHeight = 64;
        glm::vec2 Origin{0.0f, 0.0f};
        bool Visible = true;
        std::string DisplayName;
        uint8_t ActiveTilesetIndex = 0;
        uint32_t Generation = 0; // Track entity generation to detect scene reloads.
    };

    // Active tilemap editing context (kept alive for renderer + palette).
    std::shared_ptr<TileMap> m_activeTileMap;
    std::shared_ptr<Tileset> m_activeTileset;
    std::string m_activeTileMapPath;
    std::string m_activeTilesetPath;
    EntityId m_activeTileMapEntityId = ECS::Entity::NPOS32;
    std::unordered_map<EntityId, TileMapCacheEntry> m_tileMapCache;
    std::vector<TilePalettePanel::TileMapListEntry> m_tileMapList;

    // Panel registry
    std::vector<PanelRegistration> m_panelRegistry;

    // UI resources
    ImFont* m_symbolsFont = nullptr;
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    float m_uiScale = 1.0f;

    // Docking state
    ImGuiID m_dockspaceId = 0;
    bool m_dockLayoutBuilt = false;
    ImVec2 m_lastViewportSize = ImVec2(0, 0);
    // Tracks the active scene/game viewport layout preset (1/2/4).
    int m_viewportLayoutPreset = 1;

    // Playback state tracking
    EditorState m_lastEditorState = EditorState::Edit;

    // Undo System
    Editor::UndoSystem m_undoSystem;

    std::function<void()> m_projectBrowserRequest;

    // Message system subscriptions for engine events
    Messaging::SubscriptionHandle m_entityCreatedSubscription;
    Messaging::SubscriptionHandle m_entityDestroyedSubscription;
    Messaging::SubscriptionHandle m_sceneModifiedSubscription;
    // Listens for viewport layout changes requested by the header controls.
    Messaging::SubscriptionHandle m_viewportLayoutSubscription;
};

#endif // LEVEL_EDITOR_H
