#include "ecs/EntityManager.h"
#include <iostream>
#include "ecs/Entity.h"

Entity EntityManager::CreateEntity(const std::string& name) {
    const EntityId id = ++m_nextId;
    m_entities.insert(id);

#if _DEBUG
    std::cout << "Entity created: "
	          << '[' << id << ']'
	          << ' ' << name << '\n';
#endif

    return {id, m_world, name};
}

bool EntityManager::IsAlive(const Entity& entity) const {
	return m_entities.find(entity.GetId()) != m_entities.end();
}

void EntityManager::DestroyEntity(const Entity& entity) {
    RemoveAllComponents(entity.GetId());
    const auto& erased = m_entities.erase(entity.GetId());

#if _DEBUG
    if (erased == 0)
		std::cerr << "Warning: Attempted to destroy non-existent entity "
				  << '[' << entity.GetId() << ']'
				  << ' ' << entity.GetName() << '\n';
    else
		std::cout << "Entity destroyed: "
		          << '[' << entity.GetId() << ']'
		          << ' ' << entity.GetName() << '\n';
#endif
}

void EntityManager::DestroyAllEntities() {
    for (const EntityId id : m_entities)
        RemoveAllComponents(id);

    m_entities.clear();
    m_nextId = 0;
}