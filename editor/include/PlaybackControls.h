/* Start Header *****************************************************************/
/*!
\file   PlaybackControls.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th March 2026
\brief
Header for PlaybackControls class managing game playback state with state change events.
*/
/* End Header *******************************************************************/

#ifndef PLAYBACK_CONTROLS_H
#define PLAYBACK_CONTROLS_H

#include "ecs/World.h"
#include "EditorState.h"
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <functional>

class Playback {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    Playback(ECS::World* world);
    ~Playback();

    // Initialize fonts and toolbar layout dimensions
    void Initialize(ImFont* mainFont, ImFont* symbolsFont, float toolbarHeight = 60.0f);

    // -------------------------------------------------------------------------
    // Per-Frame Update
    // -------------------------------------------------------------------------

    // Process keyboard shortcuts for play, pause and step actions
    void ProcessInput();

    // Render the playback toolbar with play, pause and step buttons
    void Render();

    // -------------------------------------------------------------------------
    // Event Registration
    // -------------------------------------------------------------------------

    // Register a callback invoked when the editor state transitions
    void OnStateChanged(std::function<void(EditorState, EditorState)> callback);

    // -------------------------------------------------------------------------
    // External Hooks
    // -------------------------------------------------------------------------

    // Set a provider that returns true if the scene has unsaved changes
    void SetUnsavedChangesProvider(std::function<bool()> provider);

    // Set a callback to trigger a scene save before entering play mode
    void SetSaveSceneCallback(std::function<void()> callback);

    // Set a provider that returns true if the current scene has a save path
    void SetHasScenePathProvider(std::function<bool()> provider);

    // -------------------------------------------------------------------------
    // State Query
    // -------------------------------------------------------------------------

    // Return true if the editor is currently in Play or Step mode
    bool IsPlaying() const;

    // Return true if a single-step advance has been requested
    bool IsStepRequested() const;

    // Clear the pending step request after it has been consumed
    void ClearStepRequest();

    // Return the current editor state (Edit, Play, Pause, Step)
    EditorState GetEditorState() const;

    // -------------------------------------------------------------------------
    // World Management
    // -------------------------------------------------------------------------

    // Update the active world reference, optionally preserving playback snapshot
    void SetWorld(ECS::World* world, bool preserveState = false);

    // Return true if a valid ECS world is assigned
    bool HasValidWorld() const { return m_world != nullptr; }

    // Clear any saved world snapshot to avoid stale restores on scene reload
    void ClearSavedState();

private:
    // -------------------------------------------------------------------------
    // Internal Helpers
    // -------------------------------------------------------------------------

    // Serialize the current world state into the playback snapshot
    void _saveWorldState();

    // Restore the world to the state saved at play start
    void _restoreWorldState();

    // Transition to a new editor state and fire the state change callback
    void _changeState(EditorState newState);

    // Begin play from edit mode, guarding with unsaved-changes checks
    bool _startPlayFromEdit();

    // Return true if the external dirty-state provider reports unsaved changes
    bool _hasUnsavedChanges() const;

    // Return true if the external scene path provider reports a valid save path
    bool _hasScenePath() const;

    // Restore all component data on an existing entity from a JSON snapshot
    void _restoreEntityState(ECS::Entity entity, const nlohmann::json& entityJson);

    // Recreate an entity with a specific ID from a JSON snapshot
    ECS::Entity _recreateEntityWithId(uint32_t targetId, const nlohmann::json& entityJson);

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    ECS::World* m_world = nullptr;              // Active ECS world for play/restore operations
    ECS::World* m_savedWorldPtr = nullptr;      // World pointer at the time of the last snapshot
    EditorState m_editorState = EditorState::Edit; // Current editor state
    bool m_stepRequested = false;               // Whether a single-step advance is pending
    nlohmann::json m_savedWorldState;           // Full world snapshot taken at play start
    bool m_suppressRestoreWarning = false;      // Suppress restore mismatch warnings after intentional reloads

    // UI fonts
    ImFont* m_mainFont = nullptr;               
    ImFont* m_symbolsFont = nullptr;            
    float m_toolbarHeight = 26.0f;              

    float m_userTimeScale = 1.0f;               // Cached time scale to restore after Pause or Step
    bool m_showSaveScenePrompt = false;         // Whether to show the save prompt before entering play mode
    bool m_zeroTimeOnNextPlay = false;          // Zeroes delta time on the first play frame after a save dialog
    int m_restoreTimeScaleFrame = -1;           // Frame index on which to restore the time scale

    // Callbacks
    std::function<void(EditorState, EditorState)> m_onStateChanged;   // State transition event callback
    std::function<bool()> m_hasUnsavedChangesProvider;                // Returns true if scene has unsaved changes
    std::function<void()> m_saveSceneCallback;                        // Triggers a scene save before play
    std::function<bool()> m_hasScenePathProvider;                     // Returns true if scene has a save path
};

#endif