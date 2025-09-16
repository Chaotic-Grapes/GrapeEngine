#include "Engine.h"
#include <windows.h>
#include "Time.h"
#include "WindowManager.h"

namespace Engine {
    // Global pointer to the core engine
    Engine* CORE;

    Engine::Engine() {
        CORE = this;
    }

    void Engine::Initialize() const {
        for (const auto& system : m_systems)
            system->Initialize();
    }

    void Engine::Run(const bool consoleFlag) const {
        if (consoleFlag)
            _enableConsole();
        else
            _disableConsole();

        // WindowManager must be attached
        if (WindowManager::GetWindows().empty()) {
            return;
        }
        
        while (true) {
            Update();
            
            bool stop = true;
            for (const auto* win : WindowManager::GetWindows())
                if (!win->ShouldClose()) {
                    stop = false;
                    break;
                }
            
            if (stop) break;
		}
    }

    void Engine::AttachSystem(ISystem* system) {
        m_systems.push_back(system);
    }

    void Engine::Update() const {
        for (const auto& system : m_systems)
            system->Update();
    }

    void Engine::DestroySystems() const {
        for (const auto& system : m_systems)
            delete system;
    }

    void Engine::_enableConsole() {
#ifdef _WIN32
        AllocConsole();

        FILE* dummy;
        static_cast<void>(freopen_s(&dummy, "CONOUT$", "w", stderr));
        static_cast<void>(freopen_s(&dummy, "CONOUT$", "w", stdout));
#endif
    }

    void Engine::_disableConsole() {
#ifdef _WIN32
	    if (const HWND console = GetConsoleWindow())
            ShowWindow(console, SW_HIDE);
#endif
    }
}
