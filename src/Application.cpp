#include "Application.h"
#include <windows.h>
#include "Time.h"
#include "WindowManager.h"
#include "Entity.h"

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
        world.AddSystem<Time>();
        world.AddSystem<WindowManager>();

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

    void Application::Run(const bool consoleFlag) {
#if !_DEBUG
        if (consoleFlag)
            _enableConsole();
        else
            _disableConsole();
#else
        (void)consoleFlag;
#endif

		Initialize();

        // WindowManager must be attached
        if (WindowManager::GetWindows().empty()) {
            return;
        }
        
        while (!m_shouldStop) {
            Update();
            
            for (const auto* win : WindowManager::GetWindows())
                if (win->ShouldClose()) {
                    m_shouldStop = true;
                    break;
                }
		}

        m_worlds.clear();
    }

    void Application::Initialize() const {
        for (auto& world : m_worlds)
            world->_initialize();
    }

    void Application::Update() const {
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
