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
namespace ECS {
    class World;
}
struct ImFont;

class Playback {
public:
    // Enum representing the current playback state
    enum class GameState {
        Stopped,   // Editor mode
        Playing,   // Game running
        Paused     // Freeze
    };

    explicit Playback(ECS::World* world);
    ~Playback();

    // Initialize with symbols font for icons
    void Initialize(ImFont* mainFont, ImFont* symbolsFont);

    // Handle keyboard shortcuts
    void ProcessInput();

    // Render playback controls UI
    void Render();

    // Expose game state
    GameState GetGameState() const { return m_gameState; }
    bool IsPlaying() const;       // Returns true if game is running
    bool IsStepRequested() const; // Returns true if user requested a single physics step
    void ClearStepRequest();      // Resets step request flag

private:
    ECS::World* m_world;                        // Pointer to the game world being edited
    ImFont* m_mainFont = nullptr;               // Font for UI text
    ImFont* m_symbolsFont = nullptr;            // Font for Material Symbols icons
    GameState m_gameState = GameState::Stopped; // Current playback state
    bool m_stepRequested = false;               // Flag for single-step execution
    nlohmann::json m_savedWorldState;           // Stores world state snapshot for restore on stop

    // Save current world state before playing
    void _saveWorldState();

    // Restore world state when stopping
    void _restoreWorldState();

    // Convenience check for valid world pointer
    bool HasValidWorld() const { return m_world != nullptr; }
};

#endif