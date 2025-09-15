#ifndef COMPONENTREGISTRY_H
#define COMPONENTREGISTRY_H

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cstdint>
#include <functional>

using EntityId = uint32_t;

class ComponentRegistry {
public:
    static ComponentRegistry& Get() {
        static ComponentRegistry instance;
        return instance;
    }

    template<typename T>
    void Register() {
        const std::type_index type = typeid(T);
        if (m_cloners.find(type)
            != m_cloners.end()) return; // Already registered

        // Clone component
        m_cloners[type] = [](EntityId src, EntityId dst, auto& compMap) {
            auto it = compMap.find(src);
            if (it != compMap.end()) {
                compMap[dst] = std::make_unique<T>(*static_cast<T*>(it->second.get())); // Use the copy ctor
            }
        };

        // Remove all components for an entity
        m_removers[type] = [](EntityId id, auto& compMap) {
            compMap.erase(id);
        };
    }

    template<typename T, typename... Args>
    T& AddComponent(const EntityId id, Args&&... args) {
        const std::type_index type = typeid(T);
        auto& compMap = m_components[type];

        // Register cloner if not yet registered
        if (m_cloners.find(type) == m_cloners.end()) {
            RegisterType<T>();
        }

        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *comp;
        compMap[id] = std::move(comp);

        return ref;
    }

    template<typename T>
    T* GetComponent(const EntityId id) {
		// Check if the component type exists
        const auto itMap = m_components.find(typeid(T));
		if (itMap == m_components.end()) // No components of this type registered
            return nullptr;

        auto& compMap = m_components[typeid(T)];
        const auto it = compMap.find(id);
        if (it != compMap.end())
            return static_cast<T*>(it->second.get());

        return nullptr;
    }

    template<typename T>
    bool HasComponent(const EntityId id) {
        const auto itMap = m_components.find(typeid(T));
        if (itMap == m_components.end())
            return false;

        return itMap->second.find(id) != itMap->second.end();
    }

    template<typename T>
    void RemoveComponent(const EntityId id) {
        const auto itMap = m_components.find(typeid(T));
        if (itMap == m_components.end())
            return;

        itMap->second.erase(id);
    }

    void RemoveAllComponents(const EntityId id) {
        for (auto& [type, compMap] : m_components) {
            compMap.erase(id);
        }
    }

    void CloneComponents(const EntityId src, const EntityId dst) {
        for (auto& [type, compMap] : m_components) {
            if (m_cloners.find(type) != m_cloners.end()) {
                // Invoke the registered cloner for this type
                m_cloners[type](src, dst, compMap);
            }
        }
    }

private:
    ComponentRegistry() = default;

	// Base deleter interface for type-erased deletion
    using RawComponentMap = std::unordered_map<EntityId, std::unique_ptr<void, void(*)(void*)>>;
    std::unordered_map<std::type_index, RawComponentMap> m_components;

	// Cloner function type
    using Cloner = std::function<void(EntityId, EntityId, RawComponentMap&)>;
    std::unordered_map<std::type_index, Cloner> m_cloners;

	// Remover function type
    using Remover = std::function<void(EntityId, RawComponentMap&)>;
    std::unordered_map<std::type_index, Remover> m_removers;

    template<typename T>
    void RegisterType() {
        const std::type_index type = typeid(T);

		// Register cloner
        // Looks a little messy
        m_cloners[type] = [](const EntityId src, const EntityId dst,
            std::unordered_map<EntityId, std::unique_ptr<void, void(*)(void*)>>& compMap) {
                const auto it = compMap.find(src);
                if (it != compMap.end()) {
                    T* original = static_cast<T*>(it->second.get());
                    auto copy = std::make_unique<T>(*original); // Copy ctor
                    compMap[dst] = std::unique_ptr<void, void(*)(void*)>(
                        copy.release(),
                        [](void* p) { delete static_cast<T*>(p); }
                    );
                }
            };
    }
};

#endif