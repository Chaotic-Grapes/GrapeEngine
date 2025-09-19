#ifndef COMPONENTMANAGER_H
#define COMPONENTMANAGER_H

#include <unordered_map>

using EntityId = uint32_t;

struct IComponentManager {
    virtual ~IComponentManager() = default;
    virtual void Remove(EntityId id) = 0;
    virtual void Clone(EntityId from, EntityId to) = 0;
};

template<typename T>
class ComponentManager : public IComponentManager {
public:
    std::unordered_map<EntityId, T> m_components;

    void Add(EntityId id, const T& component) {
        m_components[id] = component;
    }

    T* Get(EntityId id) {
        auto it = m_components.find(id);
        return it != m_components.end() ? &it->second : nullptr;
    }

    void Remove(EntityId id) override {
        m_components.erase(id);
    }

    void Clone(EntityId from, EntityId to) override {
        auto it = m_components.find(from);
        if (it != m_components.end())
            m_components[to] = it->second;
    }
};

#endif
