/* Start Header *****************************************************************/
/*!
\file   Application.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   14th September 2025
\brief
Main application class for the engine. Manages the game loop, input, and
windowing.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef APPLICATION_H
#define APPLICATION_H

#include "Export.h"
#include "scene/SceneManager.h"
#include "serialization/ConfigurationSerializer.h"
#include "services/AudioService.h"
#include "services/DeviceManager.h"
#include "ecs/SystemManager.h"
#include <functional>

// Forward declarations
namespace ECS { 
    class ScriptManager;
    class AudioSystem;
}

namespace Platform {
    class IPlatformContext;
}

namespace Engine {
    /**
     * @brief Engine execution mode
     */
    enum class GRAPEENGINE_API EngineMode {
        Game,   // Standalone game runtime
        Editor  // Editor mode with ImGui and tools
    };

    class GRAPEENGINE_API Application {
    public:
        /**
         * @brief Access the SceneManager for creating/loading/unloading scenes
         */
        Scenes::SceneManager& GetSceneManager() { return m_sceneManager; }

        /**
         * @brief Get the project settings (const)
         */
        const ProjectSettings& GetProjectSettings() const { return m_projectSettings; }

        /**
         * @brief Get the project settings (mutable)
         */
        ProjectSettings& GetProjectSettings() { return m_projectSettings; }

        /**
         * @brief Check if project settings have been loaded
         */
        bool HasProjectSettings() const { return m_hasProjectSettings; }

        /**
         * @brief Save project settings to ProjectSettings.json
         * @param projectRoot Path to the project root directory
         * @return true if project settings were saved successfully
         */
        bool SaveProjectSettings(const std::string& projectRoot);

        /**
         * @brief Initialize the engine in specified mode
         * @param mode Engine execution mode (Game or Editor)
         * @param enableConsole If true, enables console output
         */
        void Initialize(EngineMode mode, bool enableConsole = true);

        /**
         * @brief Update engine systems for one frame
         */
        void Update();

        /**
         * @brief Check if engine is still running
         * @return true if engine should continue running
         */
        bool IsRunning() const { return !m_shouldStop; }

        /**
         * @brief Shutdown the engine and release resources
         */
        void Shutdown();

        /**
         * @brief Get current engine mode
         */
        EngineMode GetMode() const { return m_mode; }

        /**
         * @brief Load project-specific settings from ProjectSettings.json
         * @param projectRoot Path to the project root directory
         * @return true if project settings were loaded successfully
         */
        bool LoadProjectSettings(const std::string& projectRoot);

        /**
         * @brief Close worlds and release resources.
         */
        void Close();

        // Getters for services
        Services::AudioService* GetAudioService() { return m_audio; }
        const Services::AudioService* GetAudioService() const { return m_audio; }
        
        /**
         * @brief Get access to the SystemManager for registering/updating systems
         */
        ECS::SystemManager& GetSystemManager() { return m_systemManager; }

        /**
         * @brief Get access to the ScriptManager service (may be null if CLR failed to initialize)
         */
        ECS::ScriptManager* GetScriptManager() { return m_scriptManager; }

        /**
         * @brief Get the platform context (window, rendering, input services)
         * @return Pointer to platform context interface
         * 
         * This provides the editor and other modules access to platform services
         * through abstract interfaces, without coupling to GLFW or platform specifics.
         */
        Platform::IPlatformContext* GetPlatformContext() const { return m_platformContext; }

        /**
         * @brief Update systems for specific run modes (editor use)
         * @param modes Bitmask of SystemRunMode values to execute
         * @param world Active scene's World
         * 
         * **EDITOR MODE ONLY**: Editor uses this to control which systems execute
         * based on editor state (play/pause/edit/step).
         * 
         * In Game mode, Application::Update() automatically runs Always + PlayOnly systems.
         * In Editor mode, editor completely controls system execution via this method.
         * 
         * Example (edit mode):
         * @code
         * uint32_t editModeMask = (1 << SystemRunMode::Always) | (1 << SystemRunMode::EditOnly);
         * app->UpdateSystemsByMode(editModeMask, world);
         * @endcode
         * 
         * Example (play mode):
         * @code
         * uint32_t playModeMask = (1 << SystemRunMode::Always) | (1 << SystemRunMode::PlayOnly);
         * app->UpdateSystemsByMode(playModeMask, world);
         * @endcode
         */
        void UpdateSystemsByMode(uint32_t modes, ECS::World& world);

        // ==================== Device Management (Phase 3) ====================
        
        /**
         * @brief Set window resolution and refresh rate
         * @param width Resolution width in pixels
         * @param height Resolution height in pixels
         * @param refreshRate Target refresh rate in Hz (0 = use current monitor's rate)
         * @return true if resolution was successfully changed
         * 
         * Validates that the requested resolution is supported by the current monitor
         * before attempting to apply it. Returns false if resolution is not supported
         * or platform context is unavailable.
         * 
         * @code
         * // Change to 1920x1080 at 144Hz
         * if (app->SetResolution(1920, 1080, 144)) {
         *     LOG_INFO("Resolution changed successfully");
         * } else {
         *     LOG_ERROR("Resolution not supported");
         * }
         * @endcode
         */
        bool SetResolution(int width, int height, int refreshRate = 0);
        
        /**
         * @brief Set fullscreen mode
         * @param fullscreen true for fullscreen, false for windowed
         * @param monitorIndex Which monitor to use (0 = primary)
         * @return true if mode was successfully changed
         * 
         * Transitions between fullscreen and windowed modes. The monitorIndex parameter
         * is used to determine which monitor the window appears on (multi-monitor support).
         */
        bool SetFullscreenMode(bool fullscreen, int monitorIndex = 0);
        
        /**
         * @brief Switch to a different audio device
         * @param deviceID Device identifier from AudioDeviceInfo::DeviceID
         * @return true if audio device was successfully switched
         * 
         * Validates that the requested device exists and is connected before switching.
         * Returns false if device is not found, not connected, or audio service is unavailable.
         * 
         * Get available devices from DeviceManager::EnumerateAudioOutputDevices()
         */
        bool SetAudioDevice(const std::string& deviceID);
        
        /**
         * @brief Get information about the current monitor
         * @return MonitorInfo for the current display, or empty struct if unavailable
         * 
         * Returns detailed information about the monitor the main window is currently on,
         * including supported display modes, DPI, aspect ratio, and physical position.
         */
        MonitorInfo GetCurrentMonitorInfo() const;
        
        /**
         * @brief Get the current audio device being used
         * @return Current AudioDeviceInfo
         * 
         * Returns information about the currently active audio output device.
         * If no device is explicitly set, returns the system default.
         */
        AudioDeviceInfo GetCurrentAudioDevice() const;

    private:
        // Engine state
        bool m_shouldStop = false;
        bool m_initialized = false;
        EngineMode m_mode = EngineMode::Game;

        // Scene manager
        Scenes::SceneManager m_sceneManager;
        
        // System manager (global, persistent)
        ECS::SystemManager m_systemManager;

        // Project settings (used by both editor and game runtime)
        ProjectSettings m_projectSettings;
        bool m_hasProjectSettings = false;

        // Platform abstraction (window, rendering, input)
        Platform::IPlatformContext* m_platformContext = nullptr;

		// Functions to enable/disable console output
        static void _enableConsole();
        static void _disableConsole();

        void _initializeServices();
        void _registerSystems();

        // Services
        Services::AudioService* m_audio = nullptr;
        ECS::ScriptManager* m_scriptManager = nullptr;

        double m_lastFrameTime{0};
        float m_accumulator = 0.0f;
        
        // Device management
        std::string m_currentAudioDeviceID;
        size_t m_notifiedActiveSceneIndex = static_cast<size_t>(-1);
        
        // Helper methods for cleaner game loop logic
        void _updatePhysics(ECS::World& world);
    };

    extern GRAPEENGINE_API Application* CORE;
}

#endif
