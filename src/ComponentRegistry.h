#ifndef COMPONENTREGISTRY_H
#define COMPONENTREGISTRY_H

#include <functional>
#include <unordered_map>
#include <typeindex>

#include "IComponent.h"

class ComponentRegistry {
public:
    template<typename T>
    void Register() {
        m_factories[std::type_index(typeid(T))] = []() -> std::unique_ptr<Component::IComponent> {
            return std::make_unique<T>();
        };
    }

    template<typename T>
    std::unique_ptr<Component::IComponent> Create() {
        const auto it = m_factories.find(std::type_index(typeid(T)));
        if (it != m_factories.end())
            return it->second();
        return nullptr;
    }

private:
    std::unordered_map<std::type_index, std::function<std::unique_ptr<Component::IComponent>()>> m_factories;
};

#endif // COMPONENTREGISTRY_H
