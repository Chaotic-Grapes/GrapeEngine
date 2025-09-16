#include "World.h"
#include "Entity.h"

void World::Initialize() const {
    for (auto& sys : m_systems)
        sys->OnCreate();
}

void World::Update() const {
    for (auto& sys : m_systems)
        sys->OnUpdate();
}

EntityManager& World::GetEntityManager() { return m_entityManager; }
Entity World::CreateEntity()             { return m_entityManager.CreateEntity(); }