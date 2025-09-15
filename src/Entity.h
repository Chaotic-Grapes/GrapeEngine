#ifndef ENTITY_H
#define ENTITY_H

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cstdint>
#include "IComponent.h"

using EntityId = uint32_t;

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

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *comp;
        m_components[std::type_index(typeid(T))] = std::move(comp);
        return ref;
    }

    template<typename T>
    T* GetComponent() {
        const auto it = m_components.find(std::type_index(typeid(T)));
        if (it != m_components.end())
            return static_cast<T*>(it->second.get());
        return nullptr;
    }

    template<typename T>
    void RemoveComponent() {
        m_components.erase(std::type_index(typeid(T)));
    }

    template<typename T>
    bool HasComponent() const {
        return m_components.find(std::type_index(typeid(T))) != m_components.end();
    }

	// Similar to copy constructor but does not take in an entity reference
    Entity Clone() const;

private:
    EntityId m_id;
    static inline EntityId m_nextId = 0;

    using ComponentMap = std::unordered_map<std::type_index, std::unique_ptr<Component::IComponent>>;
    ComponentMap m_components;
};

#endif // ENTITY_H
