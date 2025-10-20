#include "ecs/EntityManager.h"
#include <iostream>
#include "ecs/Entity.h"
#include "core/Logger.h"
#include <sstream>

Entity EntityManager::CreateEntity(const std::string& name) {
    const EntityId id = ++m_nextId;
    m_entities.insert(id);
    m_names[id] = name;

#if _DEBUG
    LOG_DEBUG("Entity created: "
			  << '[' << id << ']'
			  << ' ' << name);
#endif

    return {id, m_world, name};
}

bool EntityManager::IsAlive(const Entity& entity) const {
	return m_entities.find(entity.GetId()) != m_entities.end();
}

void EntityManager::DestroyEntity(const Entity& entity) {
    RemoveAllComponents(entity.GetId());
    const auto& erased = m_entities.erase(entity.GetId());
    m_names.erase(entity.GetId());

#if _DEBUG
    if (erased == 0) {
        LOG_WARNING("Warning: Attempted to destroy non-existent entity "
		            << '[' << entity.GetId() << ']'
		            << ' ' << entity.GetName());
    }
    else {
        LOG_DEBUG("Entity destroyed: "
				  << '[' << entity.GetId() << ']'
		          << ' ' << entity.GetName());
    }
#endif
}

void EntityManager::DestroyAllEntities() {
    for (const EntityId id : m_entities)
        RemoveAllComponents(id);

    m_entities.clear();
    m_names.clear();
    m_nextId = 0;
}

std::vector<EntityId> EntityManager::GetAllEntities() const {
    return std::vector(m_entities.begin(), m_entities.end());
}

Entity EntityManager::GetEntity(const EntityId id) const {
    return Entity{id, m_world, GetName(id)};
}
