#ifndef ENTITY_INL
#define ENTITY_INL

#include "World.h"

template<typename T>
T* Entity::GetComponent() {
    return m_world->GetEntityManager().GetComponent<T>(m_id);
}

template<typename T, typename... Args>
T& Entity::AddComponent(Args&&... args) {
    return m_world->GetEntityManager().AddComponent<T>(m_id, std::forward<Args>(args)...);
}

template<typename T>
void Entity::RemoveComponent() const {
    m_world->GetEntityManager().RemoveComponent<T>(m_id);
}

template<typename T>
bool Entity::HasComponent() const {
    return m_world->GetEntityManager().HasComponent<T>(m_id);
}

template<typename T, typename... Args>
T& Entity::AddBehaviour(Args&&... args) {
    return m_world->AddBehaviour<T>(*this, std::forward<Args>(args)...);
}

#endif