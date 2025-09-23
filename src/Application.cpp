#include "Application.h"
#include <windows.h>

#include "Input.h"
#include "Physics2D.h"
#include "Renderer2D.h"
#include "systems/Time.h"
#include "systems/WindowManager.h"
#include "systems/Overlay.h"

namespace Engine {
    // Global pointer to the core engine
    Application* CORE = nullptr;
    bool Application::m_shouldStop = false;

    Application::Application() {
        CORE = this;
    }

    World& Application::CreateWorld() {
        m_worlds.push_back(std::make_unique<World>());
        auto& world = *m_worlds.back();

        // Attach core systems to the world
		world.AddSystem<Time>();            // Time system must be attached first (FPS)

		// Anything else can be attached after
        world.AddSystem<Physics2D>(&world);
        world.AddSystem<Renderer2D>(&world);

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

        World& world = CreateWorld();
        Initialize();

        game.OnStart(world);

        if (WindowManager::GetWindows().empty()) {
            return;
        }
        
        while (!m_shouldStop) {
			Input::_processInput();
            Update();
			game.OnUpdate(world);
            
            for (const auto* win : WindowManager::GetWindows()) {
                if (win->ShouldClose()) {
                    m_shouldStop = true;
                    break;
                }
                glfwSwapBuffers(win->Handle());
            }
		}

        game.OnShutdown(world);
        m_worlds.clear();
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

    void Application::LateUpdate() const {
       for (auto& world : m_worlds)
		   world->_lateUpdate();
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
