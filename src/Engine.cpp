#include "Engine.h"
#include "Time.h"

namespace Engine {
    /// Global pointer to the core engine
    Engine* CORE;

    Engine::Engine() {
        CORE = this;
    }

    void Engine::Initialize() const {
        for (const auto& system : m_systems)
            system->Initialize();
    }

    void Engine::Run() {
        if (!m_mainWindow.Create("GrapeEngine", 1280, 720))
            return;

        while (!m_mainWindow.ShouldClose()) {
            m_mainWindow.PollEvents();
            Update();
            m_mainWindow.SwapBuffers();
		}

        m_mainWindow.Destroy();
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
