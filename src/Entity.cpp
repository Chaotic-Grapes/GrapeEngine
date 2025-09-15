#include "Entity.h"

#include "Components.h"

Entity::Entity() : m_id(++m_nextId) { AddComponent<Component::Transform>(); }
Entity::Entity(const Entity& entity) {
    m_id = ++m_nextId;
    ComponentRegistry::Get().CloneComponents(entity.m_id, m_id);
}

Entity& Entity::operator=(const Entity& entity) {
    if (this != &entity) {
        ComponentRegistry::Get().RemoveAllComponents(m_id);
        ComponentRegistry::Get().CloneComponents(entity.m_id, m_id);
    }
    return *this;
}

Entity::Entity(Entity&& entity) noexcept {
    m_id = entity.m_id;
    entity.m_id = 0; // moved-from
}

Entity& Entity::operator=(Entity&& entity) noexcept {
    if (this != &entity) {
        m_id = entity.m_id;
        entity.m_id = 0;
    }
    return *this;
}

Entity::~Entity() {
    ComponentRegistry::Get().RemoveAllComponents(m_id);
}

Entity Entity::Clone() const {
    Entity copy;
    ComponentRegistry::Get().CloneComponents(m_id, copy.m_id);
    return copy;
}

Component::Transform& Entity::Transform() {
    return *GetComponent<Component::Transform>();
}

EntityId Entity::GetId() const { return m_id; }
