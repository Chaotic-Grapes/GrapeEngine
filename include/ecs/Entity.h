#ifndef ENTITY_H
#define ENTITY_H

#include <cstdint>
#include "ecs/Components.h"

using EntityId = uint32_t;

class World;
class Entity {
public:
    Entity(EntityId id, World* world, const std::string& name = "GameObject");
    Entity(const Entity& other) = default;   // just copy the handle
    ~Entity() = default;
    Entity& operator=(const Entity& other) = default;
    Entity(Entity&& other) noexcept = default;
    Entity& operator=(Entity&& other) noexcept = default;

    EntityId GetId() const;
    std::string GetName() const;
    Entity Clone() const;

    Component::Transform& Transform();

    template<typename T>
    T* GetComponent();

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args);

    template<typename T>
    void RemoveComponent() const;

    template<typename T>
    bool HasComponent() const;

    template<typename T, typename... Args>
    T& AddBehaviour(Args&&... args);

    void RemoveAllComponents() const;

private:
    EntityId m_id;
    std::string m_name;
    World* m_world = nullptr;
};

#include "Entity.inl"

#endif
