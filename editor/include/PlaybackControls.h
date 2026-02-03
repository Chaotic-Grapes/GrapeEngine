/* Start Header *****************************************************************/
/*!
\file   PlaybackControls.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
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
    Playback(ECS::World* world);
    ~Playback();

    void Initialize(ImFont* mainFont, ImFont* symbolsFont, float toolbarHeight = 60.0f);
    void ProcessInput();
    void Render();

    // State change event registration
    void OnStateChanged(std::function<void(EditorState, EditorState)> callback);
    // External hooks for dirty-state checks and save actions.
    void SetUnsavedChangesProvider(std::function<bool()> provider);
    void SetSaveSceneCallback(std::function<void()> callback);

    // Query methods
    bool IsPlaying() const;
    bool IsStepRequested() const;
    void ClearStepRequest();
    EditorState GetEditorState() const;

    // World management
    void SetWorld(ECS::World* world, bool preserveState = false);
    bool HasValidWorld() const { return m_world != nullptr; }

private:
    void _saveWorldState();
    void _restoreWorldState();
    void _changeState(EditorState newState);
    // Entry point that can guard play with unsaved-changes checks.
    bool _startPlayFromEdit();
    // Consults external dirty-state provider when available.
    bool _hasUnsavedChanges() const;
    
    // Helper methods for in-place entity restoration
    void _restoreEntityState(ECS::Entity entity, const nlohmann::json& entityJson);
    ECS::Entity _recreateEntityWithId(uint32_t targetId, const nlohmann::json& entityJson);

    ECS::World* m_world = nullptr;
    ECS::World* m_savedWorldPtr = nullptr;
    EditorState m_editorState = EditorState::Edit;
    bool m_stepRequested = false;
    nlohmann::json m_savedWorldState;

    // UI fonts
    ImFont* m_mainFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    float m_toolbarHeight = 26.0f;
    // Cached playback speed to restore after Pause/Step.
    float m_userTimeScale = 1.0f;
    // Optional play-warning flow for unsaved scenes.
    bool m_warnOnUnsavedPlay = true;
    bool m_showUnsavedPlayPopup = false;

    // Event callback
    std::function<void(EditorState, EditorState)> m_onStateChanged;
    // Editor callbacks for play-time warnings/actions.
    std::function<bool()> m_hasUnsavedChangesProvider;
    std::function<void()> m_saveSceneCallback;
};

#endif
