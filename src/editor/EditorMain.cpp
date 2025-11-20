/* Start Header *****************************************************************/
/*!
\file   EditorMain.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
Main entry point for the Grape Engine Level Editor.
Launches the application in editor mode with the level editor interface.
*/
/* End Header *******************************************************************/

#include <crtdbg.h>
#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "Game.h"
#include "services/WindowManager.h"
#include "services/OverlayService.h"
#include "core/Logger.h"

/**
 * @brief Editor-focused game class that launches directly into the level editor
 */
class EditorGame : public Game {
public:
    void OnStart(Scenes::SceneManager& sceneManager) override {
        (void)sceneManager; // Editor starts scene-less
        
        LOG_INFO("Starting Grape Engine Level Editor...");
        
        // TODO: Remove hardcoded "EchoesBelow" when editor is separated from engine
        // Initialize project paths to point to game project folder
        Engine::ProjectPaths::Initialize("EchoesBelow");
        
        // Load project-specific settings
        Engine::CORE->LoadProjectSettings("EchoesBelow");
        
        // Get window dimensions from project settings if available, otherwise use editor config
        int width, height;
        if (Engine::CORE->HasProjectSettings()) {
            const auto& projectSettings = Engine::CORE->GetProjectSettings();
            width = projectSettings.WindowSettings.Width;
            height = projectSettings.WindowSettings.Height;
        }
        else {
            const auto& config = Engine::CORE->GetConfig();
            width = config.WindowSettings.Width;
            height = config.WindowSettings.Height;
        }
        
        CREATE_WINDOW("Grape Engine Editor", width, height);
        
        // Enable level editor without a scene (scene-less editor mode)
        if (auto* overlay = Services::OverlayService::Get()) {
            overlay->EnableLevelEditorForScene(nullptr);
            LOG_INFO("Level Editor initialized successfully");
        } 
        else {
            LOG_ERROR("Failed to initialize Level Editor: OverlayService not available");
        }
    }

    void OnUpdate(Scenes::SceneManager& sceneManager) override {
        (void)sceneManager;
        // Editor updates are handled by the overlay service
    }

    void OnShutdown(Scenes::SceneManager& sceneManager) override {
        (void)sceneManager;
        DESTROY_ALL_WINDOWS();
        LOG_INFO("Grape Engine Editor shutdown complete");
    }
};

/**
 * @brief Main entry point for the Grape Engine Level Editor
 */
int main() {
    // Enable memory leak detection in debug builds
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // Create and run the editor
    Engine::Application engine;
    EditorGame editor;
    engine.Run(editor, true);

    return 0;
}
