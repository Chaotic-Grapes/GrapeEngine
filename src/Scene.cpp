#include "ecs/Scene.h"
#include "Physics2D.h"
#include "Renderer2D.h"
#include "systems/Time.h"

void Scene::Update() {
    m_accumulator += Time::DeltaTime();
    const auto& fixedTime = Time::FixedDeltaTime();

    // Fixed timestep physics updates
    while (m_accumulator >= fixedTime) {
        m_world->_fixedUpdate();
        OnFixedUpdate();
        m_accumulator -= fixedTime;
    }

    m_world->_update();
    OnUpdate();
}

void Scene::LateUpdate() {
    m_world->_lateUpdate();
    OnLateUpdate();
}

void Scene::Load() {
    m_world->AddSystem<Time>();
    m_world->AddSystem<Engine::Physics2D>(m_world.get());
    m_world->AddSystem<Engine::Renderer2D>(m_world.get());

    OnLoad();
    m_world->_initialize();
}

void Scene::Unload() {
	if (!m_world) return;
    m_world->_shutdown();
    OnUnload();
}