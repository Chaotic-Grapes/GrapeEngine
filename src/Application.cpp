#include "Application.h"

#include <iostream>
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
        return *m_worlds.back();
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

        // Create default world
        World& gameWorld = CreateWorld();

        // Attach core systems to the default world
        gameWorld.AddSystem<Time>();
        gameWorld.AddSystem<WindowManager>();

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
            world->Initialize();
    }

    void Application::Update() const {
        for (auto& world : m_worlds)
            world->Update();
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
