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
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <functional>

class Playback {
public:
    enum class GameState {
        Stopped,
        Playing,
        Paused
    };

    Playback(ECS::World* world);
    ~Playback();

    void Initialize(ImFont* mainFont, ImFont* symbolsFont, float toolbarHeight = 60.0f);
    void ProcessInput();
    void Render();

    // State change event registration
    void OnStateChanged(std::function<void(GameState, GameState)> callback);

    // Query methods
    bool IsPlaying() const;
    bool IsStepRequested() const;
    void ClearStepRequest();
    GameState GetGameState() const;

    // World management
    void SetWorld(ECS::World* world);
    bool HasValidWorld() const { return m_world != nullptr; }

private:
    void _saveWorldState();
    void _restoreWorldState();
    void _changeState(GameState newState);

    ECS::World* m_world = nullptr;
    GameState m_gameState = GameState::Stopped;
    bool m_stepRequested = false;
    nlohmann::json m_savedWorldState;

    // UI fonts
    ImFont* m_mainFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    float m_toolbarHeight = 26.0f;

    // Event callback
    std::function<void(GameState, GameState)> m_onStateChanged;
};

#endif