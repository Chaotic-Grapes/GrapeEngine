#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <memory>
#include "Physics2D.h"
#include "Renderer2D.h"
#include "Scene.h"
#include "systems/Time.h"

// TODO: Implement scene transitions (fade in/out, etc.)
// and loading progress
class SceneManager final {
public:
    static void LoadScene(std::unique_ptr<Scene> newScene) {
        if (m_currentScene) {
            m_currentScene->OnUnload(*m_world);
            m_world.reset();
        }

        m_world = std::make_unique<World>();
        m_world->AddSystem<Time>();

        m_world->AddSystem<Engine::Physics2D>(m_world.get());
        m_world->AddSystem<Engine::Renderer2D>(m_world.get());

        m_currentScene = std::move(newScene);
        m_currentScene->OnLoad(*m_world);
    }

    void Update() const {
        if (m_world)
            m_world->_update();
    }

    World* GetWorld() const { return m_world.get(); }

private:
    static std::unique_ptr<Scene> m_currentScene;
    static std::unique_ptr<World> m_world;
};

#endif
