/* Start Header *****************************************************************/
/*!
\file   OverlayService.h
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

#ifndef OVERLAY_SERVICE_H
#define OVERLAY_SERVICE_H
#include <memory>
#include <string>
#include "core/IService.h"
#include "audio/FmodAudioDevice.h"
#include "services/DebugUI.h"
#include "services/UICommon.h"
#include "services/WindowManager.h"
#include "services/Input.h"
#include "scene/SceneManager.h"
#include "ecs/World.h"
#include "../editor/LevelEditor.h"
#include "math/Vector2D.h"

namespace Services {

    // Service that manages debug UI, level editor, and ImGui integration
    class OverlayService final : public Engine::IService {
    public:
        // We need static inline because PhysicsSystem needs to find the existing OverlayService
        // but doesn't have an OverlayService object to call Get() on
        // Static methods can be called without an instance, allowing access to the singleton instance
        static inline OverlayService* m_overlayInstance = nullptr;

        // Set instance in constructor
        explicit OverlayService(Scenes::SceneManager& sceneManager) : IService("Overlay Service"), 
            m_sceneManager(sceneManager) { m_overlayInstance = this; }

        // Clear instance in destructor
        ~OverlayService() { m_overlayInstance = nullptr; }

        // Standard functions
        void Initialize() override;
        void Update() override;
        void Render() override;
        void Terminate() override;

        // Audio passthrough for debug monitoring
        void SetAudio(Audio::FmodAudioDevice* device) { m_audioDevice = device; }

        // Scene/world configuration
        void SetWorld(ECS::World* world) { m_world = world; }

        // Game playback helpers exposed to other systems
        bool IsGamePlaying() const { return m_levelEditor && m_levelEditor->IsPlaying(); }
        bool IsStepRequested() const { return m_levelEditor && m_levelEditor->IsStepRequested(); }
        void ClearStepRequest() const { if (m_levelEditor) m_levelEditor->ClearStepRequest(); }

        // This method belongs to the CLASS, not instances
        // Can be called as OverlayService::Get() without needing an object first
        static inline OverlayService* Get() { return m_overlayInstance; }

        // Level Editor visibility toggle (F2)
        void ToggleLevelEditor() { m_showLevelEditor = !m_showLevelEditor; }
        bool IsLevelEditorEnabled() const { return m_showLevelEditor; }
        void SetLevelEditorEnabled(bool enabled) { m_showLevelEditor = enabled; }

    private:
        // Listen for resize events
        void _onWindowResize(int width, int height);

        // References
        Scenes::SceneManager& m_sceneManager;
        Audio::FmodAudioDevice* m_audioDevice = nullptr;
        ECS::World* m_world = nullptr;

    #ifdef USE_IMGUI
        // ImGui-driven UI
        std::unique_ptr<DebugUI> m_debugUI;
        std::unique_ptr<LevelEditor> m_levelEditor;
        bool m_initialized = false;
        bool m_dockLayoutBuilt = false;
        bool m_showLevelEditor = false; // Default false per request
        Vector2D m_lastWindowSize{ 0, 0 };
    #endif
    };

}

#endif
