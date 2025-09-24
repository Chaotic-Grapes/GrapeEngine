#ifndef WORLD_INL
#define WORLD_INL

#include <memory>
#include "Entity.h"

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
    T* raw = behaviour.get();

    // Track state
    raw->Awake();              // Called immediately
    raw->_enabled = true;      // Internal flag for OnEnable/Disable
    raw->_started = false;     // Start will run on first Update

    m_behaviours[entity.GetId()].push_back(std::move(behaviour));
    return *raw;
}

#endif
