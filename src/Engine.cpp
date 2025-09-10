#include "Engine.h"
#include "Time.h"

namespace Engine {
    /// Global pointer to the core engine
    Engine* CORE;

    Engine::Engine() {
        Time::Initialize();
        CORE = this;
    }

    void Engine::Initialize() const {
        for (const auto& system : Systems)
            system->Initialize();
    }

    void Engine::Run() {
        if (!MainWindow.Create("GrapeEngine", 1280, 720))
            return;

        while (!MainWindow.ShouldClose()) {
            MainWindow.PollEvents();
            Update();
            MainWindow.SwapBuffers();
		}

        MainWindow.Destroy();
    }

    void Engine::AttachSystem(ISystem* system) {
        Systems.push_back(system);
    }

    void Engine::Update() const {
        Time::Update();

        for (const auto& system : Systems)
            system->Update();
    }

    void Engine::DestroySystems() const {
        for (const auto& system : Systems)
            delete system;
    }
}
