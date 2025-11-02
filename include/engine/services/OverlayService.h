/* Start Header *****************************************************************/
/*!
\file   Overlay.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Defines the Overlay class which serves as a system-level wrapper for managing
debug UI and level editor functionality.

Features:
- ImGui initialization and integration with the engine
- Debug UI and level editor lifecycle management
- Audio system integration for debug monitoring
- Conditional compilation support for ImGui features
- Window management integration for UI rendering
- Game playback state management (play/pause/stop/step)

References:
- Engine system architecture (ISystem interface pattern)
- ImGui integration with GLFW/OpenGL
- ECS pattern for system lifecycle management
*/
/* End Header *******************************************************************/

#ifndef OVERLAY_H
#define OVERLAY_H
#include <memory>
#include "ecs/ISystem.h"
#include "services/DebugUI.h"
#include "core/Application.h"
#include "audio/FmodAudioDevice.h"
#include "../editor/LevelEditor.h"

// Forward declarations
class World;
#ifdef USE_IMGUI
class DebugUI;
class LevelEditor;
#endif

// Overlay system for managing debug UI, level editor, and ImGui integration
class Overlay final : public Engine::ISystem {
public:
    // Constructor: initialize overlay with world reference
    explicit Overlay(World* world) : m_world(world) {}

    // Initialize overlay system and create UI instances
    void OnCreate() override;

    // Update overlay system each frame (handles UI rendering and updates)
    void OnUpdate() override;

#ifdef USE_IMGUI
    // Destructor: cleanup debug UI and detach audio system
    ~Overlay() override;
#endif

    // Get system name for debugging and logging
    std::string Name() const override { return "Overlay"; }

    // Set audio system for debug monitoring
    void SetAudio(Audio::FmodAudioDevice* device) { m_audioDevice = device; }

    // Check if game is currently playing (exposed for physics system)
    bool IsGamePlaying() const { return m_levelEditor && m_levelEditor->IsPlaying(); }

    // Check if physics step is requested
    bool IsStepRequested() const { return m_levelEditor && m_levelEditor->IsStepRequested(); }

    // Clear step request flag after processing
    void ClearStepRequest() const { if (m_levelEditor) m_levelEditor->ClearStepRequest(); }

private:
    Audio::FmodAudioDevice* m_audioDevice = nullptr;  // Audio system for debug monitoring

    // Set world reference for entity management
    void SetWorld(World* world) { m_world = world; }

    World* m_world = nullptr;  // World reference for entity management

#ifdef USE_IMGUI
    std::unique_ptr<DebugUI> m_debugUI;          // Debug UI instance
    std::unique_ptr<LevelEditor> m_levelEditor;  // Level editor instance
    bool m_initialized = false;                  // ImGui initialization flag
#endif
};

#endif