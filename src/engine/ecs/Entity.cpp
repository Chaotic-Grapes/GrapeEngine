#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/Components.h"

Entity::Entity(const EntityId id, World* world, const std::string& name) : m_id(id), m_world(world) {
	// m_name is initialized here because std::move on a const std::string is not allowed
	m_name = name;
	AddComponent<Component::Transform>();
}

Entity Entity::Clone() const {
    const Entity copy = m_world->GetEntityManager().CreateEntity(m_name);
    m_world->GetEntityManager().CloneComponents(m_id, copy.GetId());
    return copy;
}

Component::Transform& Entity::Transform() {
    return *GetComponent<Component::Transform>();
}

void Entity::RemoveAllComponents() const {
    if (m_world) m_world->GetEntityManager().RemoveAllComponents(m_id);
}

void Entity::SetName(const std::string& newName) {
    m_name = newName;
    m_world->GetEntityManager().SetName(m_id, newName);
}

//void Entity::SetActive(const bool active) {
//	m_isActive = active;
//}

EntityId Entity::GetId()        const { return m_id; }
std::string Entity::GetName()   const { return m_name; }
//bool Entity::IsActive()         const { return m_isActive; }
