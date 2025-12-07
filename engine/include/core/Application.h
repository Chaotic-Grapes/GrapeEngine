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
        
        // Get SystemManager for system access
        ECS::SystemManager& GetSystemManager() { return m_systemManager; }

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
         * @param deltaTime Time since last frame
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
         * app->UpdateSystemsByMode(editModeMask, world, deltaTime);
         * @endcode
         * 
         * Example (play mode):
         * @code
         * uint32_t playModeMask = (1 << SystemRunMode::Always) | (1 << SystemRunMode::PlayOnly);
         * app->UpdateSystemsByMode(playModeMask, world, deltaTime);
         * @endcode
         */
        void UpdateSystemsByMode(uint32_t modes, ECS::World& world, float deltaTime);

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
        
        // Helper methods for cleaner game loop logic
        void _updatePhysics(ECS::World& world);
    };

    extern GRAPEENGINE_API Application* CORE;
}

#endif