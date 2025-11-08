/* Start Header *****************************************************************/
/*!
\file   DebugUI.h
\author Foo Rui Qin (70%)
        Muhammad Nur Fadzly Bin Zulkifli (30%)
\par    ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   26th October 2025
\brief
Defines the DebugUI class which provides a comprehensive ImGui-based debug
interface for the game engine.

Features:
- Real-time engine status monitoring and performance metrics
- Interactive game object editor with entity management
- Input debugging and event tracking
- Audio system monitoring and control
- Configurable window layouts and UI scaling
- Memory and profiling information display
- Toggle-able interface with F1 key
*/
/* End Header *******************************************************************/

#ifndef DEBUGUI_H
#define DEBUGUI_H
#include <vector>
#include <string>
#include <unordered_map>
#include "audio/FmodAudioDevice.h"
#include "ecs/Entity.h"

// Forward declarations (avoid unnecessary includes)
struct GLFWwindow;
namespace ECS { class World; }
namespace Audio { class IAudioDevice; }
namespace Scenes { class Scene; }


// Configuration structure for DebugUI appearance and layout
struct DebugUIConfig {
    float FontScale = 1.35f;                               // Global font scaling factor
    static constexpr size_t MAX_OBJECT_NAME_LENGTH = 128;  // Max length for game object names
};

// Debug user interface system for engine development
class DebugUI {
public:
    // Constructor: initialize debug UI with world reference and config
    explicit DebugUI(ECS::World* world, const DebugUIConfig& config = {});

    // Destructor: cleanup ImGui resources if initialized
    ~DebugUI();

    // Initialize ImGui context and backends
    void Initialize(GLFWwindow* window);

    // Start new ImGui frame and handle input
    void NewFrame();

    // Render all debug windows
    void Render();

    // Shutdown ImGui and cleanup resources
    void Shutdown();

    // Enable or disable the entire debug UI
    void SetEnabled(bool enabled) { m_enabled = enabled; }

    // Check if debug UI is currently enabled
    bool IsEnabled() const { return m_enabled; }

    void AttachScene(Scenes::Scene* scene) { m_scenePtr = scene; }
    void DetachScene() { m_scenePtr = nullptr; }

    bool HasValidScene(const Scenes::Scene* scene = nullptr) const {
        // Check if member scene pointer is valid first
        if (!m_scenePtr)
            return false;

        // If a specific scene is provided, check for equality
        if (scene)
            return m_scenePtr == scene;

        // Otherwise, just confirm member pointer is valid
        return m_scenePtr != nullptr;
    }

    // Set world reference for entity management
    void SetWorld(ECS::World* world) { m_world = world; }

    // Check if world pointer is valid
    bool HasValidWorld() const;

    // Attach audio system for monitoring
    void AttachAudio(Audio::FmodAudioDevice* device) { m_audioPtr = device; }

    // Detach audio system
    void DetachAudio() { m_audioPtr = nullptr; }
    bool HasValidAudio() const { return m_audioPtr != nullptr; }

private:
    DebugUIConfig m_config;     // Configuration settings
    ECS::World* m_world;             // Pointer to World for entity management
    Scenes::Scene* m_scenePtr = nullptr;
    Audio::FmodAudioDevice* m_audioPtr = nullptr;
    bool m_enabled = false;     // Flag for UI enabled state
    bool m_initialized = false; // Flag for ImGui initialization state

    // Show ImGui demo window
    bool m_showDemo = false;

    // Input debugging counters
    int m_spacePressed = 0;   // Space key press event counter
    int m_spaceReleased = 0;  // Space key release event counter

    // Display engine status and demo window toggle
    void _showEngineDebugWindow();

    // Display FPS, frame times and profiler scope data
    void _showPerformanceWindow() const;

    // Display mouse, keyboard and window input state
    void _showInputDebugWindow();

    // Display audio system controls and library window
    void _showAudioWindow(Audio::FmodAudioDevice* audio);
};

#endif