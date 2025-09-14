#include "Engine.h"
#include "../include/graphics/renderer.hpp"
#include "../include/graphics/shader.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "Time.h"
#include <iostream>
#include "test.h"
#include "Physics.h" 

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
        if (!m_mainWindow.Create("GrapeEngine", 1600, 900))
            return;

        // Load GL function pointers AFTER context creation
        if (!gladLoadGL()) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return;
        }

        m_renderer = std::make_unique<Renderer>(1000); // ctor runs after context creation

        Shader shader("assets/shaders/batch.vert", "assets/shaders/batch.frag");

        // create projection matrix once (window size known)
        glm::mat4 proj = glm::ortho(0.0f, (float)m_mainWindow.Width(),
            0.0f, (float)m_mainWindow.Height(),
            -1.0f, 1.0f);

        //test code
        PhysicsSystem* physics = new PhysicsSystem();
        AttachSystem(physics);
        TestScene test;
        test.init(physics, (float)m_mainWindow.Width(), (float)m_mainWindow.Height());

        while (!m_mainWindow.ShouldClose()) {
            m_mainWindow.PollEvents();
            Update();

            //Batching pipeline:
            glClearColor(0.1f, 0.1f, 0.1f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);

            shader.use();
            shader.setMat4("uProjection", proj);

            test.update(Time::DeltaTime());

            m_renderer->beginFrame();

            // Submit primitives here

            test.render(*m_renderer);

            m_renderer->endFrame();

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
