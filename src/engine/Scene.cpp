#include "Scene.h"
#include "services/DebugUI.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "services/OverlayService.h"
#include "services/Time.h"

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
    m_world->AddSystem<Engine::PhysicsSystem>(m_world.get());
    m_world->AddSystem<Engine::RendererSystem>(m_world.get());

	// This must be added last so it renders on top of everything else
#ifdef USE_IMGUI
    const auto overlay = m_world->AddSystem<Overlay>(m_world.get());

    if (Engine::CORE && Engine::CORE->GetAudioService() && Engine::CORE->GetAudioService()->Device()) {
        overlay->SetAudio(Engine::CORE->GetAudioService()->Device());
    }
#endif

    OnLoad();
    m_world->_initialize();
}

void Scene::Unload() {
	if (!m_world) return;
    m_world->_shutdown();
    OnUnload();
}