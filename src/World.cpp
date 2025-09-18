#include "World.h"
#include "Entity.h"

void World::_initialize() const {
    for (auto& sys : m_systems)
        sys->OnCreate();
}

void World::_update() const {
    for (auto& sys : m_systems)
        sys->OnUpdate();
}

void World::_shutdown() {
    // No need to destroy systems individually since they are managed by unique_ptr

    m_entityManager.DestroyAllEntities();
    m_systems.clear();
}

EntityManager& World::GetEntityManager() { return m_entityManager; }
Entity World::CreateEntity()             { return m_entityManager.CreateEntity(); }