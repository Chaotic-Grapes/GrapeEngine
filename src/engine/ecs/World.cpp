#include "ecs/World.h"
#include "core/Profiler.h"
#include "ecs/Entity.h"

void World::_initialize() const {
    for (auto& sys : m_systems)
        sys->OnCreate();
}

void World::_update() const {
    for (auto& sys : m_systems) {
#ifdef _DEBUG
		ProfileScope scope(sys->Name());
#endif
        sys->OnUpdate();
    }

    // End timing for overall game loop
}

void World::_fixedUpdate() const {
    for (auto& sys : m_systems)
        sys->OnFixedUpdate();
}

void World::_lateUpdate() const {
	for (auto& sys : m_systems)
		sys->OnLateUpdate();
}

void World::_shutdown() {
    // No need to destroy systems individually since they are managed by unique_ptr

    m_entityManager.DestroyAllEntities();
    m_systems.clear();
}

void World::RemoveAllBehaviours(const Entity& entity) {
    const auto it = m_behaviours.find(entity.GetId());
    if (it != m_behaviours.end()) {
        for (const auto& behaviour : it->second) {
            behaviour->OnDestroy();
        }
        m_behaviours.erase(it);
    }
}

EntityManager& World::GetEntityManager()               { return m_entityManager; }
Entity World::CreateEntity(const std::string& name)    { return m_entityManager.CreateEntity(name); }
const std::unordered_map<EntityId,
	std::vector<
		std::unique_ptr<
			Behaviour>>>& World::GetBehaviours() const { return m_behaviours; }