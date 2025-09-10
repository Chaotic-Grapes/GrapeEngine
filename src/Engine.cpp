#include "Engine.h"
#include "Time.h"

namespace Engine {
    /// Global pointer to the core engine
    Engine* CORE;

    Engine::Engine() {        
        LastTime = 0;
        IsRunning = true;
        CORE = this;
    }

    void Engine::Initialize() const {
        for (const auto& system : Systems)
            system->Initialize();
    }

    void Engine::AttachSystem(ISystem* system) {
        Systems.push_back(system);
    }

    void Engine::Update() const {
        for (const auto& system : Systems)
            system->Update();
    }

    void Engine::DestroySystems() const {
        for (const auto& system : Systems)
            delete system;
    }
}
