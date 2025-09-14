#include "Engine.h"
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

    void Engine::Run() const {
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
}
