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

    // This creates playback controls bound to a world
    // It initializes default state and storage
    explicit Playback(ECS::World* world);
    // This cleans up any playback resources
    // It resets state if needed
    ~Playback();

    // This sets fonts for playback controls
    // It gets icons and text ready for UI
    void Initialize(ImFont* mainFont, ImFont* symbolsFont);

    // This handles keyboard shortcuts for playback
    // It toggles play pause stop and step
    void ProcessInput();

    // This draws the playback toolbar
    // It shows buttons and state feedback
    void Render();

    // This returns the current game state
    // It can be polled by other systems
    GameState GetGameState() const { return m_gameState; }
    // This returns true when the game is running
    // It is handy for gating updates
    bool IsPlaying() const;
    // This returns true if a single step is requested
    // It lets physics run one frame
    bool IsStepRequested() const;
    // This clears the single step request
    // It resets the flag for next time
    void ClearStepRequest();

    // This updates the world reference
    // It is safe to call when scenes change
    void SetWorld(ECS::World* world);

private:
    ECS::World* m_world;                        // Pointer to the game world being edited
    ImFont* m_mainFont = nullptr;               // Font for UI text
    ImFont* m_symbolsFont = nullptr;            // Font for Material Symbols icons
    GameState m_gameState = GameState::Stopped; // Current playback state
    bool m_stepRequested = false;               // Flag for single-step execution
    nlohmann::json m_savedWorldState;           // Stores world state snapshot for restore on stop

    // This saves current world state to JSON
    // It is used before starting play
    void _saveWorldState();

    // This restores world state after stopping
    // It brings the editor back to how it was
    void _restoreWorldState();

    // This returns whether the world pointer is valid
    // It helps guard operations
    bool HasValidWorld() const { return m_world != nullptr; }
};

#endif