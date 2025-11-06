/**
 * @file OverlayService.h
 * @author Foo Rui Qin
 * @date 2024
 * @brief Overlay system for managing debug UI and ImGui integration
 * 
 * This file defines the OverlayService class which serves as a system-level wrapper
 * for the debug UI functionality. The OverlayService system manages:
 * - ImGui initialization and integration with the engine
 * - Debug UI lifecycle management (creation, updates, cleanup)
 * - Audio system integration for debug monitoring
 * - Conditional compilation support for ImGui features
 * - Window management integration for UI rendering
 * - World reference management for entity debugging
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

// Forward declarations
namespace Scenes { class SceneManager; }

#ifdef USE_IMGUI
#include "services/DebugUI.h"
class DebugUI;
#endif

namespace Services {
    /**
     * @brief Overlay system for managing debug UI and ImGui integration
     *
     * The OverlayService class serves as a system-level wrapper that manages the debug UI
     * functionality within the engine's ECS architecture. It handles the lifecycle
     * of ImGui integration, debug UI creation and updates, and provides a bridge
     * between the engine systems and the debug interface.
     *
     * Key responsibilities:
     * - Initialize and manage ImGui context and backends
     * - Create and manage DebugUI instance lifecycle
     * - Handle audio system integration for debug monitoring
     * - Manage window references for UI rendering
     * - Provide conditional compilation support for ImGui features
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
    OverlayService(Scenes::SceneManager& sceneManager) : IService("Overlay Service"), m_sceneManager(sceneManager) { SetEnabled(false); }

#ifdef USE_IMGUI
        /**
         * @brief Destructor for Overlay system
         *
         * Properly shuts down the debug UI and detaches the audio system
         * to prevent memory leaks and ensure clean resource cleanup.
         * Only defined when ImGui is available to avoid linking issues.
         */
        ~OverlayService() override { Terminate(); }
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
         * @brief Set the audio system for debug monitoring
         * @param device Pointer to the audio system to monitor
         *
         * Attaches an audio system to the overlay for debug monitoring.
         * The audio system will be accessible through the debug UI for
         * real-time monitoring and control.
         */
        void SetAudio(Audio::FmodAudioDevice* device) { m_audioDevice = device; }

    private:
        Audio::FmodAudioDevice* m_audioDevice = nullptr;  ///< Pointer to audio system for debug monitoring
		Scenes::SceneManager& m_sceneManager; 		      ///< Reference to the scene manager for world access

#ifdef USE_IMGUI
        std::unique_ptr<DebugUI> m_debugUI;  ///< Unique pointer to DebugUI instance for memory management
        bool m_initialized = false;          ///< Flag indicating if ImGui has been initialized
#endif
    };
}

#endif
