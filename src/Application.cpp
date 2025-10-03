#include "Application.h"
#include <thread>
#include "CrashDumping.h"
#include "Input.h"
#include "Physics2D.h"
#include "Profiler.h"
#include "systems/WindowManager.h"
#include "ecs/Scene.h"
#include "systems/Time.h"

namespace Engine {
    // Global pointer to the core engine
    Application* CORE = nullptr;
    bool Application::m_shouldStop = false;

    void Application::Run(Game& game, const bool consoleFlag) {
        // Set global pointer to this application instance
        CORE = this;

        // Initialize crash dumping system
        Grape_Engine::CrashDumping::Initialize();
        Grape_Engine::CrashDumping::SetProgramName("GrapeEngine");
        Grape_Engine::CrashDumping::SetDumpCreateState(true);

#if !_DEBUG
        if (consoleFlag)
            _enableConsole();
        else
            _disableConsole();
#else
        (void)consoleFlag;
#endif

        // Call OnStart() function of game then attempt to create a main window
        game.OnStart(m_sceneManager);
        Scene* currentScene = m_sceneManager.GetActiveScene();

        while (!m_shouldStop) {
            const double frameStart = glfwGetTime();
            Profiler::UpdateTime();
            // --- Input & Game Update ---
            Input::_processInput();
            auto* newScene = m_sceneManager.GetActiveScene();
            const bool isNewScene = newScene == currentScene;
            if (!isNewScene) {
                if (currentScene)
                    currentScene->Unload();

                newScene->Load();

                // This *might* cause a memory access violation
                delete currentScene;
                currentScene = newScene;
            }

            if (currentScene) {
                currentScene->Update();
                game.OnUpdate(m_sceneManager);
                currentScene->LateUpdate();
            }

            // --- Rendering ---
            for (const auto* win : WindowManager::GetWindows()) {
                if (win->ShouldClose()) {
                    m_shouldStop = true;
                    break;
                }
                glfwSwapBuffers(win->Handle());
            }

            // --- FPS Controller ---
            const double frameEnd = glfwGetTime();
            double frameDuration = frameEnd - frameStart;

            // Apply FPS cap (if set)
            if (Time::FpsCap() > 0) {
                const double targetFrameTime = 1.0 / Time::FpsCap();
                if (frameDuration < targetFrameTime) {
                    std::this_thread::sleep_for(
                        std::chrono::duration<double>(targetFrameTime - frameDuration)
                    );
                }
            }
        }

        if (currentScene) {
            game.OnShutdown(m_sceneManager);
            currentScene->Unload();
        }

        WindowManager::DestroyAll();
    }

    void Application::Close() {
        m_shouldStop = true;
    }

    void Application::_enableConsole() {
#ifdef _WIN32
        AllocConsole();

        FILE* dummy;
        static_cast<void>(freopen_s(&dummy, "CONOUT$", "w", stderr));
        static_cast<void>(freopen_s(&dummy, "CONOUT$", "w", stdout));
#endif
    }

    void Application::_disableConsole() {
#ifdef _WIN32
        if (const HWND console = GetConsoleWindow())
            ShowWindow(console, SW_HIDE);
#endif
    }
}