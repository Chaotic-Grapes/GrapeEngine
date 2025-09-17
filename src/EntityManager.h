#ifndef ENTITYMANAGER_H
#define ENTITYMANAGER_H

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include "ComponentManager.h"

class Entity;
class World;
class EntityManager {
public:
    EntityManager() = default;

    Entity CreateEntity();

    void SetWorld(World* world) { m_world = world; }

    void DestroyEntity(const Entity& entity);
    void DestroyAllEntities();

    bool IsAlive(const Entity& entity) const;

    template<typename T, typename... Args>
    T& AddComponent(EntityId id, Args&&... args) {
        auto& mgr = _getOrCreateManager<T>();
        if (!mgr.Get(id))
            mgr.Add(id, T(std::forward<Args>(args)...));

        return *_getOrCreateManager<T>().Get(id);
    }

    template<typename T>
    T* GetComponent(EntityId id) {
        return _getOrCreateManager<T>().Get(id);
    }

    template<typename T>
    bool HasComponent(EntityId id) {
        const auto it = m_managers.find(typeid(T));
        if (it == m_managers.end())
            return false;

        auto* mgr = static_cast<ComponentManager<T>*>(it->second.get());
        return mgr->Get(id) != nullptr;
    }

    template<typename T>
    void RemoveComponent(const EntityId id) {
    	const auto it = m_managers.find(typeid(T));
        if (it != m_managers.end())
            it->second.get()->Remove(id);
    }

    void RemoveAllComponents(const EntityId id) {
        for (auto& [type, mgr] : m_managers) {            
            mgr->Remove(id);
        }
    }

    void CloneComponents(const EntityId from, const EntityId to) {
        for (auto& [type, mgr] : m_managers)
            mgr->Clone(from, to);
    }

private:
    EntityId m_nextId{ 0 };
    World* m_world = nullptr;

    std::unordered_set<EntityId> m_entities;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentManager>> m_managers;

    template<typename T>
    ComponentManager<T>& _getOrCreateManager() {
        const auto it = m_managers.find(typeid(T));
        if (it == m_managers.end()) {
            auto mgr = std::make_unique<ComponentManager<T>>();
            auto* ptr = mgr.get();
            m_managers[typeid(T)] = std::move(mgr);
            return *ptr;
        }
        return *static_cast<ComponentManager<T>*>(m_managers[typeid(T)].get());
    }
};

#endif
