#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/Components.h"

Entity::Entity(const EntityId id, World* world, const std::string& name) : m_id(id), m_world(world) { m_name = name; AddComponent<Component::Transform>(); }

Entity Entity::Clone() const {
    const Entity copy = m_world->GetEntityManager().CreateEntity(m_name);
    m_world->GetEntityManager().CloneComponents(m_id, copy.GetId());
    return copy;
}

Component::Transform& Entity::Transform() {
    return *GetComponent<Component::Transform>();
}

void Entity::RemoveAllComponents() const {
    m_world->GetEntityManager().RemoveAllComponents(m_id);
}

EntityId Entity::GetId()        const { return m_id; }
std::string Entity::GetName()   const { return m_name; }
