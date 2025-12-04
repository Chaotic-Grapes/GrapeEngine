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

#include "Game.h"
#include "scene/SceneManager.h"
#include "serialization/ConfigurationSerializer.h"
#include "services/AudioService.h"
#include "ecs/SystemManager.h"
#include <functional>

// Forward declarations
namespace ECS { 
    class ScriptSystem;
    class AudioSystem;
}

namespace Engine {
    class Application {
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
         * @brief Starts the engine
         *
         * @param game Reference to the game instance
         * @param consoleFlag If true, runs with console output enabled
         */
        void Run(Game& game, bool consoleFlag);

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

    private:
        // Flag to indicate if application should stop
        static bool m_shouldStop;

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
        ECS::ScriptSystem* m_scriptSystem = nullptr;

        double m_lastFrameTime{0};
        float m_accumulator = 0.0f;
        
        // Callback for external control of game logic (used by editor)
        std::function<bool()> m_gameLogicCallback;

        void _onGameStart(Scenes::Scene* scene);
        void _onGameStop(Scenes::Scene* scene);
        
        // Helper methods for cleaner game loop logic
        bool _shouldRunGameLogic() const;
        void _updatePhysics(ECS::World& world, bool shouldRun, bool stepRequested);
        void _updateScripts(ECS::World& world, bool shouldRun);
    };

    extern Application* CORE;
}

#endif