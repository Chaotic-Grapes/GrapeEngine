#ifndef WORLD_INL
#define WORLD_INL

#include <memory>
#include "ecs/Entity.h"

template<typename T, typename... Args>
T* World::AddSystem(Args&&... args) {
    auto sys = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = sys.get();
    m_systems.push_back(std::move(sys));
    return raw;
}

template<typename T>
T* World::GetSystem() {
    for (auto& sys : m_systems) {
        if (auto casted = dynamic_cast<T*>(sys.get()))
            return casted;
    }
    return nullptr;
}

// In World because World consists of logic
// Whereas EntityManager consists of only data
template<typename T, typename... Args>
T& World::AddBehaviour(Entity& entity, Args&&... args) {
    auto behaviour = std::make_unique<T>(std::forward<Args>(args)...);
    T* ptr = behaviour.get();

    // Track state
    ptr->Awake();              // Called immediately
    ptr->_enabled = true;      // Internal flag for OnEnable/Disable
    ptr->_started = false;     // Start will run on first Update

    m_behaviours[entity.GetId()].push_back(std::move(behaviour));
    return *ptr;
}

template<typename T>
void World::ForEachEntity(T func) {
    const auto entityIds = m_entityManager.GetAllEntities();
    for (const EntityId id : entityIds) {
        Entity entity = m_entityManager.GetEntity(id);
        func(entity);
    }
}

#endif
