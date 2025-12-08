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
#include "EditorEntityActions.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <vector>
#include <unordered_set>
#include "ConsolePanel.h"
#include "PerformancePanel.h"
#include "SpriteImportPanel.h"

struct LevelEditorConfig {
    float TextFontSize = 23.0f;
    float IconFontSize = 26.0f;
    float ToolbarHeight = 26.0f;
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
    void Update();
    void Render();
    void SetWorld(ECS::World* world);
    // Set active scene pointer so EntityActions can operate immediately
    void SetScene(Scenes::Scene* scene);

    void OnWindowResized(int width, int height);

    bool IsPlaying() const;
    bool IsStepRequested() const;
    void ClearStepRequest();
    EditorState GetEditorState() const;
    bool HasValidWorld() const { return m_world != nullptr; }

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
    EntityActions m_entityActions;
    ConsolePanel m_console;
    PerformancePanel m_performancePanel;
    SpriteImportPanel m_spriteImportPanel;

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

    // Playback state tracking
    EditorState m_lastEditorState = EditorState::Edit;

    // Undo System
    Editor::UndoSystem m_undoSystem;

    // Message system subscriptions for engine events
    Messaging::SubscriptionHandle m_entityCreatedSubscription;
    Messaging::SubscriptionHandle m_entityDestroyedSubscription;
    Messaging::SubscriptionHandle m_sceneModifiedSubscription;
};

#endif // LEVEL_EDITOR_H