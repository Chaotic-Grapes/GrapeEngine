#ifndef ENTITY_H
#define ENTITY_H

#include "Components.h"
#include "World.h"

using EntityId = uint32_t;

class Entity {
public:
    Entity(EntityId id, World* world);
    Entity(const Entity& other);
    ~Entity();
    Entity& operator=(const Entity& other);
    Entity(Entity&& entity) noexcept;
    Entity& operator=(Entity&& entity) noexcept;

    EntityId GetId() const;
    Entity Clone() const;

    Component::Transform& Transform();

    template<typename T>
    T* GetComponent() {
        return m_world->GetEntityManager().GetComponent<T>(m_id);
    }

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        return m_world->GetEntityManager().AddComponent<T>(m_id, std::forward<Args>(args)...);
    }

    template<typename T>
    void RemoveComponent() {
        m_world->GetEntityManager().RemoveComponent<T>(m_id);
    }

    template<typename T>
    bool HasComponent() const {
        return m_world->GetEntityManager().HasComponent<T>(m_id);
    }

    void RemoveAllComponents() const {
        m_world->GetEntityManager().RemoveAllComponents(m_id);
    }

private:
    EntityId m_id;
    World* m_world;
};

#endif
