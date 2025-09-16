#include "EntityManager.h"

Entity EntityManager::CreateEntity() {
    const EntityId id = ++m_nextId;
    m_entities.insert(id);

    Entity e(id, m_world);

    return e;
}

bool EntityManager::IsAlive(const Entity& entity) const {
	return m_entities.find(entity.GetId()) != m_entities.end();
}

void EntityManager::DestroyEntity(const Entity& entity) {
    RemoveAllComponents(entity.GetId());
}