#include "core/Application.h"
#include "core/CrashDumping.h"
#include "core/Profiler.h"
#include "ecs/systems/PhysicsSystem.h"
#include "scene/Scene.h"
#include "services/Input.h"
#include "services/Time.h"
#include "services/WindowManager.h"
#include "services/OverlayService.h"
#include <thread>

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
        Serialization::ConfigurationSerializer::LoadConfig("config.json", m_config);

#if !_DEBUG
        if (consoleFlag)
            _enableConsole();
        else
            _disableConsole();
#else
        (void)consoleFlag;
#endif
		// Initialize services
		_initializeServices();

        // Call OnStart() function of game then attempt to create a main window
        game.OnStart(m_sceneManager);

        // Ensure overlay has a valid world from the active scene
        if (auto* initialScene = m_sceneManager.GetActive()) {
            m_overlay->SetWorld(&initialScene->GetWorld());
        }

        m_lastFrameTime = glfwGetTime();
        while (!m_shouldStop) {
            const double frameStart = glfwGetTime();
            const double rawDelta = frameStart - m_lastFrameTime;
            m_lastFrameTime = frameStart;
            
            Time::_update(rawDelta, frameStart);
            Profiler::UpdateTime();

            // --- Input & Game Update ---
            Input::_processInput();

            // --- Update Services ---
            m_audio->Update();

            // --- Scene Update ---
            m_sceneManager.Update();
            auto* currentScene = m_sceneManager.GetActive();
            if (currentScene) {
                // Use unscaled delta time for the accumulator
                m_accumulator += Time::UnscaledDeltaTime();
                
                // Prevent accumulator from growing too large
                // Use FixedDeltaTime * 5 as threshold
                const float maxAccumulator = Time::UnscaledFixedDeltaTime() * 5.0f;
                if (m_accumulator > maxAccumulator)
                    m_accumulator = maxAccumulator;
                // Keep overlay world in sync with active scene
                m_overlay->SetWorld(&currentScene->GetWorld());

                while (m_accumulator >= Time::UnscaledFixedDeltaTime()) {
                    currentScene->OnFixedUpdate();
                    m_accumulator -= Time::UnscaledFixedDeltaTime();
                }

                currentScene->OnUpdate();
                game.OnUpdate(m_sceneManager);
                currentScene->OnLateUpdate();
            }

            // --- Update Overlay Service ---
            // This here because it depends on current scene
			m_overlay->Update();
            m_overlay->Render();

            // --- Rendering ---
            for (const auto* win : WindowManager::GetWindows()) {
                if (win->ShouldClose()) {
                    m_shouldStop = true;
                    break;
                }
                win->SwapBuffers();
            }

            // --- FPS Controller ---
            if (Time::FpsCap() > 0) {
                const double frameDuration = glfwGetTime() - frameStart;
                const double targetFrameTime = 1.0 / Time::FpsCap();
                if (frameDuration < targetFrameTime) {
                    std::this_thread::sleep_for(
                        std::chrono::duration<double>(targetFrameTime - frameDuration)
                    );
                }
            }
        }

        game.OnShutdown(m_sceneManager);

        // Clean up services
        delete m_audio;
        delete m_overlay;

        WindowManager::DestroyAll();
    }

    void Application::Close() {
        m_shouldStop = true;
    }

    void Application::_initializeServices() {
        m_audio = new Services::AudioService();
        m_audio->Initialize();

		m_overlay = new Services::OverlayService(m_sceneManager);
        m_overlay->SetAudio(m_audio->Device());
		m_overlay->Initialize();
    }


    void Application::_enableConsole() {
#ifdef _WIN32
#include <windows.h>
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