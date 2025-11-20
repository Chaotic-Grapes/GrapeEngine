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

// Forward declaration
namespace Services { class OverlayService; }

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
        const EditorSettings& GetConfig() const { return m_config; }

        /**
         * @brief Get the project settings
         */
        const ProjectSettings& GetProjectSettings() const { return m_projectSettings; }

        /**
         * @brief Check if project settings have been loaded
         */
        bool HasProjectSettings() const { return m_hasProjectSettings; }

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

        Services::OverlayService* GetOverlayService() { return m_overlay; }
        const Services::OverlayService* GetOverlayService() const { return m_overlay; }

    private:
        // Flag to indicate if application should stop
        static bool m_shouldStop;

        // Scene manager
        Scenes::SceneManager m_sceneManager;

        // Editor configuration
        EditorSettings m_config;

        // Project settings
        ProjectSettings m_projectSettings;
        bool m_hasProjectSettings = false;

		// Functions to enable/disable console output
        static void _enableConsole();
        static void _disableConsole();

        void _initializeServices();

        // Services
        Services::AudioService* m_audio = nullptr;
		Services::OverlayService* m_overlay = nullptr;

        double m_lastFrameTime{0};
        float m_accumulator = 0.0f;

        void _onGameStart(Scenes::Scene* scene);
        void _onGameStop(Scenes::Scene* scene);
    };

    extern Application* CORE;
}

#endif