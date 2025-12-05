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
         * @brief Set callback to control whether game logic should run (editor use)
         * @param callback Function that returns true if game should run
         */
        void SetGameLogicCallback(std::function<bool()> callback) { m_gameLogicCallback = callback; }

        /**
         * @brief Set callback to check if step was requested (editor use)
         * @param callback Function that returns true if step requested (and clears the flag)
         */
        void SetStepRequestCallback(std::function<bool()> callback) { m_stepRequestCallback = callback; }

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
        
        // Callbacks for external control of game logic (used by editor)
        std::function<bool()> m_gameLogicCallback;
        std::function<bool()> m_stepRequestCallback;  // Returns true if step requested, resets after check

        void _onGameStart(Scenes::Scene* scene);
        void _onGameStop(Scenes::Scene* scene);
        
        // Helper methods for cleaner game loop logic
        bool _shouldRunGameLogic() const;
        void _updatePhysics(ECS::World& world, bool shouldRun, bool stepRequested);
    };

    extern GRAPEENGINE_API Application* CORE;
}

#endif