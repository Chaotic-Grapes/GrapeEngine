#ifndef APPLICATION_H
#define APPLICATION_H

#include <windows.h>
#include "Game.h"
#include "ecs/SceneManager.h"
#include "serialization/Serialization.h"
#include "services/AudioService.h"

namespace Engine {
    class Application {
    public:
        /**
         * @brief Access the SceneManager for creating/loading/unloading scenes
         */
        SceneManager& GetSceneManager() { return m_sceneManager; }

        /**
         * @brief Get the application configuration
         */
        const ApplicationConfig& GetConfig() const { return m_config; }

        /**
         * @brief Starts the engine
         *
         * @param game Reference to the game instance
         * @param consoleFlag If true, runs with console output enabled
         */
        void Run(Game& game, bool consoleFlag);

        /**
         * @brief Close worlds and release resources.
         */
        void Close();

        Services::AudioService* GetAudioService() { return m_audio; }
        const Services::AudioService* GetAudioService() const { return m_audio; }
    private:
        // Flag to indicate if application should stop
        static bool m_shouldStop;

        // Scene manager
        SceneManager m_sceneManager;

        // Application configuration
        ApplicationConfig m_config;

		// Functions to enable/disable console output
        static void _enableConsole();
        static void _disableConsole();

        // Services
        Services::AudioService* m_audio;
    };

    extern Application* CORE;
}

#endif