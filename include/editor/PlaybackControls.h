/* Start Header *****************************************************************/
/*!
\file   PlaybackControls.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Declares the PlaybackControls class for managing game playback state in the
level editor (play, pause, stop, step).

Features:
- Game state management (stopped, playing, paused)
- Keyboard shortcuts for playback control (Ctrl+P, Ctrl+Shift+P, Alt+P)
- World state serialization for play/stop functionality
- Step-by-step physics frame execution
- ImGui UI with Material Symbols icons
*/
/* End Header *******************************************************************/

#ifndef PLAYBACKCONTROLS_H
#define PLAYBACKCONTROLS_H

#include "nlohmann/json.hpp"

// Forward declarations
class World;
struct ImFont;

class Playback {
public:
    enum class GameState {
        Stopped,   // Editor mode
        Playing,   // Game running
        Paused     // Freeze
    };

    explicit Playback(World* world);  
    ~Playback();                  

    // Initialize with symbols font for icons
    void Initialize(ImFont* symbolsFont);

    // Handle keyboard shortcuts
    void ProcessInput();

    // Render playback controls UI
    void Render();

    // Expose game state
    GameState GetGameState() const { return m_gameState; }
    bool IsPlaying() const;
    bool IsStepRequested() const;
    void ClearStepRequest();

private:
    World* m_world;
    ImFont* m_symbolsFont = nullptr;
    GameState m_gameState = GameState::Stopped;
    bool m_stepRequested = false;
    nlohmann::json m_savedWorldState;

    // Save current world state before playing
    void _saveWorldState();

    // Restore world state when stopping
    void _restoreWorldState();

    bool HasValidWorld() const { return m_world != nullptr; }
};

#endif