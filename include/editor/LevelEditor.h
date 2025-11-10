/* Start Header *****************************************************************/
/*!
\file   LevelEditor.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Declares the LevelEditor class which orchestrates all editor panels including
playback controls and asset browser.

Features:
- Manages playback controls for game state (play/pause/stop)
- Manages asset browser for file management
- Font loading and configuration for editor UI
- Exposes game state for physics system integration
*/
/* End Header *******************************************************************/

#ifndef LEVELEDITOR_H
#define LEVELEDITOR_H

#include "../editor/PlaybackControls.h"
#include "../editor/AssetBrowser.h"
#include "../editor/EditorCore.h"
#include "../editor/HierarchyWindow.h"
#include "../editor/InspectorWindow.h"
#include <unordered_set>
#include <imgui.h>

// Forward declarations
struct GLFWwindow;
struct ImFont;

// Configuration structure for level editor UI settings
struct LevelEditorConfig {
    float FontSize = 16.0f;                                // Base font size in pixels
    static constexpr size_t MAX_OBJECT_NAME_LENGTH = 128;  // Maximum length for game object names
};

// Level editor orchestrates all editor panels and manages editor state
class LevelEditor {
public:
    // This creates the level editor with a world
    // It stores config and sets up panels
    explicit LevelEditor(ECS::World* world, const LevelEditorConfig& config = {});
    // This tears down editor resources
    // It releases owned pointers cleanly
    ~LevelEditor();

    // This initializes ImGui fonts and panels
    // It sets up docking and window state
    void Initialize(GLFWwindow* pWin);

    // This handles input and per frame updates
    // It drives state for all panels
    void Update();

    // This draws all editor panels
    // It builds the layout and windows
    void Render();

    // This updates the world reference for all panels
    // It propagates the change to keep everything synced
    void SetWorld(ECS::World* world);

    // This handles window resize
    // It rebuilds docking layout if needed
    void OnWindowResized(int width, int height);

    // Expose game state for physics system
    // Get current game state for systems
    Playback::GameState GetGameState() const { return m_playback.GetGameState(); }
    // Check if the game is playing
    bool IsPlaying() const { return m_playback.IsPlaying(); }
    // Check if a single step is requested
    bool IsStepRequested() const { return m_playback.IsStepRequested(); }
    // Clear single step request
    void ClearStepRequest() { m_playback.ClearStepRequest(); }

private:
    // This builds the docking layout once
    // It positions panels in a friendly arrangement
    void _buildDockLayout();
    // This renders the dock host and space
    // It manages central docking area
    void _renderDockSpace();
    bool m_dockLayoutBuilt = false;
    ImGuiID m_dockspaceId = 0;
    ImVec2 m_lastViewportSize; // Tracks viewport size to trigger layout rebuild

    ECS::World* m_world;               // Reference to game world
    LevelEditorConfig m_config;        // Editor configuration settings
    Playback m_playback;               // Playback controls panel
    ImFont* m_symbolsFont = nullptr;   // Material Symbols icon font
    ImFont* m_mainFont = nullptr;      // Regular Inter font for text
    ImFont* m_boldFont = nullptr;      // Bold Inter font
    AssetBrowser m_assetBrowser;       // Asset browser panel
    EditorCore m_editorCore;           // Entity editor panel
    HierarchyWindow m_hierarchyWindow; // Hierarchy panel
    InspectorWindow m_inspector;       // Unified inspector absorbing PrefabEditor

    // Track last playback state to drive camera toggles
    Playback::GameState m_lastGameState = Playback::GameState::Stopped;
    // Track entities already converted from pixels to world units
    std::unordered_set<EntityId> m_convertedCircles;
    std::unordered_set<EntityId> m_convertedBoxes;
    std::unordered_set<EntityId> m_convertedPositions;
};

#endif