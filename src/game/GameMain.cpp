/* Start Header *****************************************************************/
/*!
\file   GameMain.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th November 2025
\brief
Main entry point for standalone game builds (without level editor).
Launches the game directly from the startup scene specified in ProjectSettings.json.
*/
/* End Header *******************************************************************/

#include <crtdbg.h>
#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "core/Logger.h"
#include "Game.h"
#include "services/WindowManager.h"
#include "physics/Physics.h"

/**
 * @brief Standalone game class that launches directly into gameplay
 */
class StandaloneGame : public Game {
public:
    void OnStart(Scenes::SceneManager& sceneManager) override {
        LOG_INFO("Starting Standalone Game Build...");
        
        // TODO: Make project name configurable via build system
        // For now, hardcoded to EchoesBelow
        const std::string projectName = "EchoesBelow";
        
        // Initialize project paths
        Engine::ProjectPaths::Initialize(projectName);
        
        // Load project settings
        if (!Engine::CORE->LoadProjectSettings(projectName)) {
            LOG_ERROR("Failed to load ProjectSettings.json for: " << projectName);
            LOG_ERROR("Cannot start game without project configuration");
            Engine::CORE->Close();
            return;
        }
        
        const auto& projectSettings = Engine::CORE->GetProjectSettings();
        
        // Apply project settings
        int width = projectSettings.WindowSettings.Width;
        int height = projectSettings.WindowSettings.Height;
        bool vsync = projectSettings.WindowSettings.VSync;
        bool fullscreen = projectSettings.WindowSettings.Fullscreen;
        
        // Apply physics settings
        Engine::Physics::SetGravity(Vector2D(0.0f, projectSettings.Physics.Gravity));
        LOG_INFO("Applied physics gravity: " << projectSettings.Physics.Gravity);
        
        // Create game window
        LOG_INFO("Creating game window: " << projectSettings.Title);
        WindowMode::Flags windowMode = fullscreen ? WindowMode::Fullscreen : WindowMode::Windowed;
        auto* window = CREATE_WINDOW_EX(
            projectSettings.Title.c_str(),
            width,
            height,
            vsync,
            windowMode
        );
        
        if (!window) {
            LOG_ERROR("Failed to create game window");
            Engine::CORE->Close();
            return;
        }
        
        // Load startup scene
        const std::string& startupScene = projectSettings.StartupScene;
        if (startupScene.empty()) {
            LOG_ERROR("No startup scene specified in ProjectSettings.json");
            Engine::CORE->Close();
            return;
        }
        
        LOG_INFO("Loading startup scene: " << startupScene);
        
        // Create a new scene and load the scene file into it
        auto* scene = new Scenes::Scene();
        size_t sceneIndex = sceneManager.AddScene(scene);
        
        if (!sceneManager.LoadScene(sceneIndex, startupScene)) {
            LOG_ERROR("Failed to load startup scene: " << startupScene);
            Engine::CORE->Close();
            return;
        }
        
        // Activate the startup scene
        sceneManager.SetActive(sceneIndex);
        LOG_INFO("Game initialized successfully");
    }

    void OnUpdate(Scenes::SceneManager& sceneManager) override {
        (void)sceneManager;
        // Game logic is handled by the engine's systems
    }

    void OnShutdown(Scenes::SceneManager& sceneManager) override {
        (void)sceneManager;
        DESTROY_ALL_WINDOWS();
        LOG_INFO("Game shutdown complete");
    }
};

/**
 * @brief Main entry point for standalone game builds
 */
int main() {
    // Enable memory leak detection in debug builds
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // Create and run the game
    Engine::Application engine;
    StandaloneGame game;
    engine.Run(game, false); // false = disable console in release builds

    return 0;
}

#ifdef _WIN32
#include <windows.h>
// WinMain shim: when linked as a GUI/Windows subsystem the linker expects WinMain.
// Provide a small wrapper that forwards to the regular `main()` function so the
// same code works for both console and GUI subsystems.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return main();
}
#endif
