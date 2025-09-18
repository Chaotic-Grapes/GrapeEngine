#ifndef ENTITY_H
#define ENTITY_H

#include <cstdint>
#include "ecs/Components.h"

using EntityId = uint32_t;

class World;
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
    T* GetComponent();

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args);

    template<typename T>
    void RemoveComponent();

    template<typename T>
    bool HasComponent() const;

    void RemoveAllComponents() const;

private:
    EntityId m_id;
    World* m_world = nullptr;
};

#include "Entity.inl"

#endif
