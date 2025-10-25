#include "core/Application.h"
#include <thread>
#include "core/CrashDumping.h"
#include "services/Input.h"
#include "ecs/systems/PhysicsSystem.h"
#include "core/Profiler.h"
#include "services/WindowManager.h"
#include "Scene.h"
#include "services/Time.h"

namespace Engine {
    // Global pointer to the core engine
    Application* CORE = nullptr;
    bool Application::m_shouldStop = false;

    void Application::Run(Game& game, const bool consoleFlag) {
        Time::_initialize();

        // Set global pointer to this application instance
        CORE = this;

        // Initialize crash dumping system
        Grape_Engine::CrashDumping::Initialize();
        Grape_Engine::CrashDumping::SetProgramName("GrapeEngine");
        Grape_Engine::CrashDumping::SetDumpCreateState(true);

        // Load configuration first
        Serialization::ConfigLoader::LoadConfig("../config.json", m_config);

#if !_DEBUG
        if (consoleFlag)
            _enableConsole();
        else
            _disableConsole();
#else
        (void)consoleFlag;
#endif
        m_audio = new Services::AudioService();
        m_audio->Initialize();

        // Call OnStart() function of game then attempt to create a main window
        game.OnStart(m_sceneManager);
        Scene* currentScene = m_sceneManager.GetActiveScene();

        m_lastFrameTime = glfwGetTime();
        while (!m_shouldStop) {
            const double frameStart = glfwGetTime();
            Time::_update(frameStart - m_lastFrameTime, frameStart);
            Profiler::UpdateTime();
            m_lastFrameTime = glfwGetTime();

            // --- Input & Game Update ---
            Input::_processInput();
            auto* newScene = m_sceneManager.GetActiveScene();
            const bool isNewScene = newScene != currentScene;
            if (isNewScene) {
                if (currentScene)
                    currentScene->Unload();

                newScene->Load();

                // This *might* cause a memory access violation
                delete currentScene;
                currentScene = newScene;
            }

            // --- Update Services ---
            m_audio->Update();

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
                win->SwapBuffers();
            }

            const double frameDuration = m_lastFrameTime - frameStart;

            // --- FPS Controller ---
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

        // Clean up services
        delete m_audio;

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