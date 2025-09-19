#ifndef ENTITYMANAGER_H
#define ENTITYMANAGER_H

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>

#include "IComponent.h"
#include "ecs/ComponentManager.h"

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

    //template<typename... Components>
    //std::vector<EntityId> Query() {
    //    std::vector<EntityId> result;

    //    for (EntityId id : m_entities) {
    //        if ((HasComponent<Components>(id) && ...)) {
    //            result.push_back(id);
    //        }
    //    }
    //    return result;
    //}

    template<typename... Components>
    std::vector<std::tuple<Components&...>> Query() {
        std::vector<std::tuple<Components&...>> result;

        for (const EntityId id : m_entities) {
            if ((HasComponentOrDerived<Components>(id) && ...)) {
                result.emplace_back(*GetComponentOrDerived<Components>(id)...);
            }
        }

        return result;
    }

    template<typename T, typename... Args>
    T& AddComponent(EntityId id, Args&&... args) {
        auto& mgr = _getOrCreateManager<T>();
        if (!mgr.Get(id))
            mgr.Add(id, T(std::forward<Args>(args)...));

        return *mgr.Get(id);
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

    template<typename T>
    bool HasComponentOrDerived(EntityId id) {
        // Exact type first
        if (HasComponent<T>(id)) return true;

        // Check all managers for derived types
        for (auto& [type, mgr] : m_managers) {
            Component::IComponent* baseComp = mgr->GetBaseComponent(id);
            if (baseComp && dynamic_cast<T*>(baseComp))
                return true;
        }
        return false;
    }

    template<typename T>
    T* GetComponentOrDerived(EntityId id) {
        // Exact type first
        if (auto* comp = GetComponent<T>(id)) return comp;

        // Check all managers for derived types
        for (auto& [type, mgr] : m_managers) {
            Component::IComponent* baseComp = mgr->GetBaseComponent(id);
            if (baseComp) {
                if (auto* derived = dynamic_cast<T*>(baseComp))
                    return derived;
            }
        }

        return nullptr;
    }
};

#endif
