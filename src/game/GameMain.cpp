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
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>
#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "core/Logger.h"
#include "services/TimeSystem.h"
#include "platform/IPlatformContext.h"
#include "physics/Physics.h"
#include "scene/SceneManager.h"

namespace {
    std::string GetProjectRootFromArgs(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--project") == 0 || std::strcmp(argv[i], "-project") == 0) {
                if (i + 1 < argc) {
                    return argv[i + 1];
                }
            }
        }
        return "";
    }

    std::string FindProjectRootInCwd() {
        namespace fs = std::filesystem;
        std::vector<std::string> candidates;
        const fs::path cwd = fs::current_path();

        for (const auto& entry : fs::directory_iterator(cwd)) {
            if (!entry.is_directory()) {
                continue;
            }
            const fs::path settingsPath = entry.path() / "ProjectSettings.json";
            if (fs::exists(settingsPath)) {
                candidates.push_back(entry.path().string());
            }
        }

        if (candidates.size() == 1) {
            return candidates.front();
        }

        if (candidates.empty()) {
            return "";
        }

        LOG_ERROR("Multiple projects found in working directory. Use --project <path>.");
        for (const auto& candidate : candidates) {
            LOG_ERROR("  Project: " << candidate);
        }
        return "__ambiguous__";
    }
}

/**
 * @brief Main entry point for standalone game builds
 */
int main(int argc, char** argv) {
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

    std::string projectRoot = GetProjectRootFromArgs(argc, argv);
    if (projectRoot.empty()) {
        projectRoot = FindProjectRootInCwd();
    }
    if (projectRoot.empty()) {
        LOG_ERROR("No project found. Place a project folder next to the executable or pass --project <path>.");
        return 1;
    }
    if (projectRoot == "__ambiguous__") {
        return 1;
    }

    // Initialize project paths
    Engine::ProjectPaths::Initialize(projectRoot);

    // Load project settings
    if (!engine.LoadProjectSettings(projectRoot)) {
        LOG_ERROR("Failed to load ProjectSettings.json for: " << projectRoot);
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
    return main(__argc, __argv);
}
#endif
