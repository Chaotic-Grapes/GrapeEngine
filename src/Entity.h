#ifndef ENTITY_H
#define ENTITY_H

#include <memory>
#include "ComponentRegistry.h"

class Entity {
public:
    Entity();
	// Copy constructor
    Entity(const Entity& entity);
    ~Entity();
    Entity& operator=(const Entity& entity);
    Entity(Entity&& entity) noexcept;
    Entity& operator=(Entity&& entity) noexcept;

    EntityId GetId() const;
    Entity Clone() const;

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        return ComponentRegistry::Get().AddComponent<T>(m_id, std::forward<Args>(args)...);
    }

    template<typename T>
    T* GetComponent() {
        return ComponentRegistry::Get().GetComponent<T>(m_id);
    }

    template<typename T>
    bool HasComponent() const {
        return ComponentRegistry::Get().HasComponent<T>(m_id);
    }

    template<typename T>
    void RemoveComponent() const {
        ComponentRegistry::Get().RemoveComponent<T>(m_id);
    }

private:
    EntityId m_id;
    static inline EntityId m_nextId = 0;
};

#endif // ENTITY_H
