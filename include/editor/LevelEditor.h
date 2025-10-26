#ifndef LEVELEDITOR_H
#define LEVELEDITOR_H

#include "../editor/PlaybackControls.h"

// Forward declarations
struct GLFWwindow;
class World;

// Structure for LevelEditor config
struct LevelEditorConfig {
    float FontScale = 1.0f;  // Global font scaling factor for the entire UI
    float FontSize = 24.0f;
    static constexpr size_t MAX_OBJECT_NAME_LENGTH = 128;  // Maximum length for game object names
};

class LevelEditor {
public:
    explicit LevelEditor(World* world, const LevelEditorConfig& config = {});
    ~LevelEditor();

    void Initialize(GLFWwindow* pWin);
    void Update();
    void Render();

    // Expose game state for physics
    Playback::GameState GetGameState() const { return m_playback.GetGameState(); }
    bool IsPlaying() const { return m_playback.IsPlaying(); }
    bool IsStepRequested() const { return m_playback.IsStepRequested(); }
    void ClearStepRequest() { m_playback.ClearStepRequest(); }

private:
    World* m_world;
    LevelEditorConfig m_config;
    Playback m_playback;
    ImFont* m_symbolsFont = nullptr;
};

#endif