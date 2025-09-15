#include "Entity.h"

Entity::Entity() : m_id(++m_nextId) { }
Entity::Entity(const Entity& entity) {
    m_id = ++m_nextId;
    m_components.clear();
    for (const auto& [type, comp] : entity.m_components) {
        m_components[type] = comp->Clone();
    }
}

Entity Entity::Clone() const {
    Entity copy;
    for (auto& [type, comp] : m_components) {
        copy.m_components[type] = comp->Clone();
    }
    return copy;
}

Entity::~Entity() {
    for (auto& [type, comp] : m_components) {
        delete comp;
    }
    m_components.clear();
}

Entity& Entity::operator=(const Entity& entity) {
    if (this != &entity) {
        for (auto& [type, comp] : m_components) {
            delete comp;
        }
        m_components.clear();
        for (const auto& [type, comp] : entity.m_components) {
            m_components[type] = comp->Clone();
        }
    }
	return *this;
}

Entity::Entity(Entity&& entity) noexcept {
    m_id = entity.m_id;
    m_components = std::move(entity.m_components);
	entity.m_components.clear();
}

Entity& Entity::operator=(Entity&& entity) noexcept {
    if (this != &entity) {
        for (auto& [type, comp] : m_components) {
            delete comp;
        }
        m_components = std::move(entity.m_components);
        entity.m_components.clear();
        m_id = entity.m_id;
    }
	return *this;
}

EntityId Entity::GetId() const { return m_id; }
