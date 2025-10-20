#ifndef APPLICATION_H
#define APPLICATION_H

#include "Game.h"
#include "scene/SceneManager.h"
#include "serialization/ConfigurationSerializer.h"
#include "services/AudioService.h"
#include "services/OverlayService.h"

namespace Engine {
    class Application {
    public:
        /**
         * @brief Access the SceneManager for creating/loading/unloading scenes
         */
        Scenes::SceneManager& GetSceneManager() { return m_sceneManager; }

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
        Scenes::SceneManager m_sceneManager;

        // Application configuration
        ApplicationConfig m_config;

		// Functions to enable/disable console output
        static void _enableConsole();
        static void _disableConsole();

        void _initializeServices();

        // Services
        Services::AudioService* m_audio = nullptr;
		Services::OverlayService* m_overlay = nullptr;

        double m_lastFrameTime{0};
        float m_accumulator = 0.0f;
    };

    extern Application* CORE;
}

#endif