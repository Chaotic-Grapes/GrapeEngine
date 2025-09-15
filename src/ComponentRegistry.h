#ifndef COMPONENTREGISTRY_H
#define COMPONENTREGISTRY_H

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cstdint>
#include <functional>

using EntityId = uint32_t;

// Base wrapper interface (type-erased)
struct IComponentWrapper {
    virtual ~IComponentWrapper() = default;
    virtual std::unique_ptr<IComponentWrapper> Clone() const = 0;
    virtual void* RawPtr() = 0;
};

// Concrete wrapper for T
template<typename T>
struct ComponentWrapper : IComponentWrapper {
    T m_component;

    explicit ComponentWrapper(const T& comp) : m_component(comp) {}
    explicit ComponentWrapper(T&& comp) : m_component(std::move(comp)) {}

    std::unique_ptr<IComponentWrapper> Clone() const override {
        return std::make_unique<ComponentWrapper>(m_component); // copy ctor
    }

    void* RawPtr() override {
        return &m_component;
    }
};

class ComponentRegistry {
public:
    static ComponentRegistry& Get() {
        static ComponentRegistry instance;
        return instance;
    }

    template<typename T, typename... Args>
    T& AddComponent(const EntityId id, Args&&... args) {
        auto& compMap = m_components[typeid(T)];
        auto wrapper = std::make_unique<ComponentWrapper<T>>(T(std::forward<Args>(args)...));

        T& ref = static_cast<ComponentWrapper<T>*>(wrapper.get())->m_component;
        compMap[id] = std::move(wrapper);

        return ref;
    }

    template<typename T>
    T* GetComponent(const EntityId id) {
		// Check if the component type exists
        const auto itMap = m_components.find(typeid(T));
        if (itMap == m_components.end())
            return nullptr;

        auto& compMap = itMap->second;
        const auto it = compMap.find(id);
        if (it != compMap.end())
            return &static_cast<ComponentWrapper<T>*>(it->second.get())->m_component;

        return nullptr;
    }

    template<typename T>
    bool HasComponent(const EntityId id) {
        const auto itMap = m_components.find(typeid(T));
        return itMap != m_components.end() && itMap->second.find(id) != itMap->second.end();
    }

    template<typename T>
    void RemoveComponent(const EntityId id) {
        const auto itMap = m_components.find(typeid(T));
        if (itMap != m_components.end())
            itMap->second.erase(id);
    }

    void RemoveAllComponents(const EntityId id) {
        for (auto& [type, compMap] : m_components) {
            compMap.erase(id);
        }
    }

    void CloneComponents(const EntityId src, const EntityId dst) {
        for (auto& [type, compMap] : m_components) {
            const auto it = compMap.find(src);
            if (it != compMap.end()) {
                compMap[dst] = it->second->Clone();
            }
        }
    }

private:
    ComponentRegistry() = default;

	// Map: type_index -> (Map: EntityId -> ComponentWrapper)
    using ComponentMap = std::unordered_map<EntityId, std::unique_ptr<IComponentWrapper>>;
    std::unordered_map<std::type_index, ComponentMap> m_components;
};

#endif