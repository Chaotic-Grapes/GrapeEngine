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
#include "services/TimeSystem.h"
#include "platform/IPlatformContext.h"
#include "physics/Physics.h"
#include "scene/SceneManager.h"

/**
 * @brief Main entry point for standalone game builds
 */
int main() {
    // Enable memory leak detection in debug builds
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    LOG_INFO("Starting Standalone Game Build...");

    // Initialize engine in Game mode
    Engine::Application engine;
#ifdef _DEBUG
    engine.Initialize(Engine::EngineMode::Game, true);
#else
    engine.Initialize(Engine::EngineMode::Game, false);
#endif

    // TODO: Make project name configurable via build system
    // For now, hardcoded to EchoesBelow
    const std::string projectName = "EchoesBelow";

    // Initialize project paths
    Engine::ProjectPaths::Initialize(projectName);

    // Load project settings
    if (!engine.LoadProjectSettings(projectName)) {
        LOG_ERROR("Failed to load ProjectSettings.json for: " << projectName);
        LOG_ERROR("Cannot start game without project configuration");
        return 1;
    }

    const auto& projectSettings = engine.GetProjectSettings();

    // Apply project settings
    int width = projectSettings.WindowSettings.Width;
    int height = projectSettings.WindowSettings.Height;
    bool vsync = projectSettings.WindowSettings.VSync;
    bool fullscreen = projectSettings.WindowSettings.Fullscreen;

    // Apply physics settings
    Engine::Physics::SetGravity(Vector2D(0.0f, projectSettings.Physics.Gravity));
    LOG_INFO("Applied physics gravity: " << projectSettings.Physics.Gravity);

    // Create game window via platform context
    LOG_INFO("Creating game window: " << projectSettings.Title);
    auto* platformContext = engine.GetPlatformContext();
    if (!platformContext) {
        LOG_ERROR("Platform context not available");
        engine.Shutdown();
        return 1;
    }

    Platform::WindowCreateInfo windowInfo;
    windowInfo.Title = projectSettings.Title;
    windowInfo.Width = width;
    windowInfo.Height = height;
    windowInfo.VSync = vsync;
    windowInfo.Mode = fullscreen ? Platform::WindowMode::Fullscreen : Platform::WindowMode::Windowed;

    auto* window = platformContext->CreatePlatformWindow(windowInfo);
    if (!window) {
        LOG_ERROR("Failed to create game window");
        engine.Shutdown();
        return 1;
    }

    // Initialize systems after window is created
    ECS::World emptyWorld;
    engine.GetSystemManager().CreateAll(emptyWorld);

    // Load startup scene
    const std::string& startupScene = projectSettings.StartupScene;
    if (startupScene.empty()) {
        LOG_ERROR("No startup scene specified in ProjectSettings.json");
        engine.Shutdown();
        return 1;
    }

    LOG_INFO("Loading startup scene: " << startupScene);

    auto& sceneManager = engine.GetSceneManager();
    auto* scene = new Scenes::Scene();
    size_t sceneIndex = sceneManager.AddScene(scene);

    if (!sceneManager.LoadScene(sceneIndex, startupScene)) {
        LOG_ERROR("Failed to load startup scene: " << startupScene);
        engine.Shutdown();
        return 1;
    }

    sceneManager.SetActive(sceneIndex);

    // Game main loop
    while (engine.IsRunning()) {
        // Update engine (all game systems run)
        engine.Update();

        // Swap buffers
        for (auto* win : platformContext->GetAllWindows()) {
            win->SwapBuffers();
        }
    }

    // Shutdown
    engine.Shutdown();
    LOG_INFO("Game shutdown complete");

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
