#include "Entity.h"
#include "World.h"
#include "Components.h"

Entity::Entity(const EntityId id, World* world) : m_id(id), m_world(world) { AddComponent<Component::Transform>(); }
Entity::Entity(const Entity& other)
    : m_id(other.m_world->GetEntityManager().CreateEntity().GetId()), m_world(other.m_world) {
    m_world->GetEntityManager().CloneComponents(other.GetId(), m_id);
}

Entity& Entity::operator=(const Entity& other) {
    if (this != &other) {
        RemoveAllComponents();
        m_id = m_world->GetEntityManager().CreateEntity().GetId();
        m_world->GetEntityManager().CloneComponents(other.GetId(), m_id);
    }
    return *this;
}

Entity::Entity(Entity&& entity) noexcept : m_id(entity.m_id), m_world(entity.m_world) {
    entity.m_id = 0;
    entity.m_world = nullptr;
}

Entity& Entity::operator=(Entity&& entity) noexcept {
    if (this != &entity) {
        m_id = entity.m_id;
        m_world = entity.m_world;
        entity.m_id = 0;
        entity.m_world = nullptr;
    }
    return *this;
}

Entity::~Entity() {
   // RemoveAllComponents();
}

Entity Entity::Clone() const {
    Entity copy = m_world->GetEntityManager().CreateEntity();
    m_world->GetEntityManager().CloneComponents(m_id, copy.GetId());
    return copy;
}

Component::Transform& Entity::Transform() {
    return *GetComponent<Component::Transform>();
}

void Entity::RemoveAllComponents() const {
    m_world->GetEntityManager().RemoveAllComponents(m_id);
}

EntityId Entity::GetId() const { return m_id; }
