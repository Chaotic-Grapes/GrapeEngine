/* Start Header *****************************************************************/
/*!
\file   LevelEditor.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025

\brief
Declares the LevelEditor class - main orchestrator for the game editor interface.
Manages docking layout, panel coordination, entity selection, and playback controls.

New Features:
- Centralized panel registration system for easier integration
- Simplified initialization and rendering through callbacks
- Reduced boilerplate for adding new panels
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/World.h"
#include "../editor/PlaybackControls.h"
#include "../editor/AssetBrowserPanel.h"
#include "../editor/HierarchyPanel.h"
#include "../editor/InspectorPanel.h"
#include "../editor/Viewport.h"
#include "../editor/EditorEntityActions.h"
#include <imgui.h>
#include <unordered_set>
#include <functional>
#include <vector>

struct GLFWwindow;

// Configuration for the level editor appearance and behavior
struct LevelEditorConfig {
    float TextFontSize = 23.0f;
    float IconFontSize = 26.0f;
};

// Main editor coordinator class that manages all editor panels and their interactions.
// Uses a centralized panel registration system to reduce boilerplate when adding panels.
class LevelEditor {
public:
    LevelEditor(ECS::World* world, const LevelEditorConfig& config, Scenes::Scene* scene);
    ~LevelEditor();

    // Core lifecycle methods
    void Initialize(GLFWwindow* pWin);
    void Update();
    void Render();
    
    // Window event handlers
    void OnWindowResized(int width, int height);

    // World management
    void SetWorld(ECS::World* world);
    inline ECS::World* GetWorld() const { return m_world; }
    inline bool HasValidWorld() const { return m_world != nullptr; }

    // Playback state queries
    inline bool IsPlaying() const { return m_playback.IsPlaying(); }
    inline bool IsStepRequested() const { return m_playback.IsStepRequested(); }
    inline void ClearStepRequest() { m_playback.ClearStepRequest(); }

private:
    // -------------------------------------------------------------------------
    // Panel Registration System
    // -------------------------------------------------------------------------
    // Represents a registered editor panel with its lifecycle callbacks.
    // Centralizes panel management and eliminates repetitive initialization code.
    struct PanelRegistration {
        const char* Name;                                 // Display name for debugging
        std::function<void()> InitializeCallback;         // Called once during editor startup
        std::function<void()> RenderCallback;             // Called every frame
        std::function<void(ECS::World*)> SetWorldCallback; // Called when world changes
    };

    // Register a panel with the editor system.
    // Panels are initialized in registration order and rendered each frame.
    void _registerPanel(const char* panelName, 
                        std::function<void()> initFn, 
                        std::function<void()> renderFn,
                        std::function<void(ECS::World*)> setWorldFn);

    // Initialize all registered panels in order.
    void _initializePanels();
    
    // Render all registered panels in order.
    void _renderPanels();

    // Propagate world reference to all registered panels.
    void _updatePanelWorlds(ECS::World* world);

    void _loadFonts();

    // -------------------------------------------------------------------------
    // Docking
    // -------------------------------------------------------------------------
    void _buildDockLayout();
    void _renderDockSpace();

    // -------------------------------------------------------------------------
    // Panel Instances
    // -------------------------------------------------------------------------
    Playback m_playback;              // Game controls (play/pause/stop/step)
    AssetBrowserPanel m_assetBrowser;      // File browser and asset management
    Viewport m_viewport;          // Viewport and core editor features
    HierarchyPanel m_hierarchyWindow; // Entity tree view
    InspectorPanel m_inspector;      // Component property editor
    EntityActions m_entityActions;

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    ECS::World* m_world;              // Active scene world being edited
    LevelEditorConfig m_config;        // Editor configuration

    // Panel registration storage
    std::vector<PanelRegistration> m_panelRegistry;

    // Fonts
    ImFont* m_mainFont;
    ImFont* m_boldFont;
    ImFont* m_symbolsFont;

    // Docking state
    ImGuiID m_dockspaceId = 0;
    bool m_dockLayoutBuilt = false;
    ImVec2 m_lastViewportSize{ 0, 0 };

    // Playback state tracking
    Playback::GameState m_lastGameState = Playback::GameState::Stopped;

    // Unit conversion tracking (pixel to world space)
    std::unordered_set<EntityId> m_convertedPositions;
    std::unordered_set<EntityId> m_convertedCircles;
    std::unordered_set<EntityId> m_convertedBoxes;
};
