#ifndef LEVELEDITOR_H
#define LEVELEDITOR_H

#include "nlohmann/json.hpp"

// Forward declarations
struct GLFWwindow;
class World;
struct ImFont;

// Structure copied from DebugUI
struct LevelEditorConfig {
    float FontScale = 1.0f;  // Global font scaling factor for the entire UI
    float FontSize = 24.0f;
    static constexpr size_t MAX_OBJECT_NAME_LENGTH = 128;  // Maximum length for game object names
};

class LevelEditor {
public:
    enum class GameState {
        Stopped,   // Editor mode
        Playing,   // Game running
        Paused     // Freeze
    }; 

    explicit LevelEditor(World* world, const LevelEditorConfig& config = {});
    ~LevelEditor();

    void Initialize(GLFWwindow* pWin);
    void ProcessInput();
    void Render();

    // Expose game state for physics
    GameState GetGameState() const { return m_gameState; }
    bool IsPlaying() const;
    bool IsStepRequested() const;
    void ClearStepRequest();

private:
    World* m_world;
    LevelEditorConfig m_config;

    GameState m_gameState = GameState::Stopped;
    bool m_stepRequested = false;
    nlohmann::json m_savedWorldState;

    // Font for symbols
    ImFont* m_symbolsFont = nullptr;

    void _showPlaybackControls();
    void _saveWorldState();
    void _restoreWorldState();

    bool HasValidWorld() const { return m_world != nullptr; }
};

#endif