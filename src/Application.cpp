#include "Application.h"
#include <windows.h>
#include <algorithm>
#include <thread>
#include "Input.h"
#include "Physics2D.h"
#include "Profiler.h"
#include "systems/WindowManager.h"
#include "systems/Time.h"
#include "ecs/Scene.h"
#include "systems/Overlay.h"
#include "systems/AudioEngine.h"

namespace Engine {
    Application* CORE = nullptr;
    bool Application::m_shouldStop = false;

    Application::Application() {
        CORE = this;
    }

    World& Application::CreateWorld() {
        m_worlds.push_back(std::make_unique<World>());
        auto& world = *m_worlds.back();

        // Attach core systems to the world
        world.AddSystem<Time>();
        world.AddSystem<Overlay>(&world);
        world.AddSystem<WindowManager>();
        world.AddSystem<AudioSystem>();

        // Link Overlay -> Audio so DebugUI can use it
        auto* overlay = world.GetSystem<Overlay>();
        auto* audioSys = world.GetSystem<AudioSystem>();
        if (overlay && audioSys) {
            overlay->SetAudio(audioSys->GetAudio());
        }

        return world;
    }

    void Application::DestroyWorld(World& world) {
        const auto it = std::find_if(m_worlds.begin(), m_worlds.end(),
            [&world](const std::unique_ptr<World>& ptr) {
                return ptr.get() == &world;
            });

        if (it != m_worlds.end()) {
            m_worlds.erase(it);
        }
    }

    void Application::DestroyWorld(const size_t index) {
        if (index < m_worlds.size()) {
            m_worlds.erase(m_worlds.begin() + static_cast<long long>(index));
        }
    }

    void Application::DestroyAllWorlds() {
        m_worlds.clear();
    }

    size_t Application::GetWorldCount() const {
        return m_worlds.size();
    }

    void Application::Run(Game& game, const bool consoleFlag) {
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

    void Application::Initialize() const {
        for (auto& world : m_worlds)
            world->_initialize();
    }

    void Application::Update() const {
        // Clear screen FIRST
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (auto& world : m_worlds)
            world->_update();
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
