/**
 * @file OverlayService.h
 * @author Foo Rui Qin
 * @date 2024
 * @brief Overlay system bridging DebugUI and the ImGui-based LevelEditor
 * 
 * This file defines the OverlayService class which serves as a system-level wrapper
 * for editor and debug UI functionality. OverlayService manages:
 * - DebugUI lifecycle and ImGui backend integration
 * - LevelEditor creation/update/render gating per active scene
 * - Audio system integration for debug monitoring
 * - Conditional compilation support for ImGui features
 * - Window management integration for UI rendering
 * - World reference propagation to keep editor/debug views in sync
 * 
 * The OverlayService system inherits from Engine::ISystem and follows the ECS pattern,
 * providing a clean interface between the engine's system architecture and
 * the ImGui-based debug interface.
 */

#ifndef OVERLAYSERVICE_H
#define OVERLAYSERVICE_H

#include "core/IService.h"
#include <memory>
#include "audio/FmodAudioDevice.h"
#include <memory>

// Forward declarations
namespace Scenes { class SceneManager; class Scene; }

#include "services/DebugUI.h"
#include "../editor/LevelEditor.h"

namespace Services {
    /**
     * @brief Overlay system bridging DebugUI and the LevelEditor overlay
     *
     * The OverlayService class serves as a system-level wrapper that manages the debug UI
     * functionality within the engine's ECS architecture. It handles the lifecycle
     * of ImGui integration, debug UI creation and updates, and provides a bridge
     * between the engine systems and the debug interface.
     *
     * Key responsibilities:
     * - Initialize and manage ImGui context and backends via DebugUI
     * - Create and manage LevelEditor and DebugUI lifecycles
     * - Handle audio system integration for debug monitoring
     * - Manage window references for UI rendering
     * - Provide conditional compilation support for ImGui features
     * - Expose playback state helpers to systems (playing/step)
     * - Ensure proper cleanup and resource management
     *
     * The system follows the Engine::ISystem interface pattern and integrates
     * seamlessly with the engine's update loop and system management.
     *
     * Usage example:
     * @code
     * auto overlay = std::make_unique<Overlay>(&world);
     * overlay->SetAudio(&audioSystem);
     * systemManager.AddSystem(std::move(overlay));
     * @endcode
     */
    class OverlayService final : public Engine::IService {
    public:
        /**
         * @brief Constructor for Overlay system
         */
    explicit OverlayService(Scenes::SceneManager& sceneManager) : IService("Overlay Service"), m_sceneManager(sceneManager) { 
        m_overlayInstance = this;
        SetEnabled(false); 
    }

#ifdef USE_IMGUI
        /**
         * @brief Destructor for Overlay system
         */
        ~OverlayService() override;
#endif

        /**
		 * @brief Initialize the overlay system
         */
        void Initialize() override;

        /**
         * @brief Update the overlay system each frame
         *
         * Called every frame to update the overlay system. Handles:
         * - DebugUI instance creation if not already created
         * - ImGui initialization when window becomes available
         * - Audio system attachment for debug monitoring
         * - Frame-by-frame UI rendering and updates
         *
         * This method manages the complete UI update cycle including
         * ImGui frame setup, rendering, and finalization.
         */
        void Update() override;

        /**
		 * @brief Render the overlay UI
		 */
        void Render() override;

        /**
         * @brief Cleanup and terminate the overlay system
         */
        void Terminate() override;

        /**
         * @brief Enable the LevelEditor UI targeting a specific scene or globally
         * @param scene Scene to attach the LevelEditor to; nullptr for scene-less mode
         */
        void EnableLevelEditorForScene(Scenes::Scene* scene);

        /**
         * @brief Disable the LevelEditor UI and release its resources
         */
        void DisableLevelEditor();

        /**
         * @brief Set the audio system for debug monitoring
         * @param device Pointer to the audio system to monitor
         *
         * Attaches an audio system to the overlay for debug monitoring.
         * The audio system will be accessible through the debug UI for
         * real-time monitoring and control.
         */
        void SetAudio(Audio::FmodAudioDevice* device) { m_audioDevice = device; }
        void SetWorld(ECS::World* world);

        bool IsGamePlaying() const;
        bool IsStepRequested() const;
        void ClearStepRequest() const;

        static inline OverlayService* Get() { return m_overlayInstance; }

    private:
        Audio::FmodAudioDevice* m_audioDevice = nullptr;  ///< Pointer to audio system for debug monitoring
		Scenes::SceneManager& m_sceneManager; 		      ///< Reference to the scene manager for world access
        ECS::World* m_world = nullptr;

        // Global access helper for test harness and sandbox
        static inline OverlayService* m_overlayInstance = nullptr;

#ifdef USE_IMGUI
        std::unique_ptr<DebugUI> m_debugUI;
        std::unique_ptr<LevelEditor> m_levelEditor;
        bool m_initialized = false;
        bool m_showLevelEditor = false;
        Scenes::Scene* m_levelEditorForScene = nullptr; // nullptr means scene-less mode
        bool m_pendingLevelEditorRebuild = false;
#endif
    };
}

#endif
