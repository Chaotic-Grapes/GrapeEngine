#ifndef WORLD_H
#define WORLD_H

#include <memory>
#include <vector>
#include "EntityManager.h"
#include "ISystem.h"

class World {
public:
	World() = default;
	~World() = default;

    EntityManager& GetEntityManager();

    Entity CreateEntity();

    template<typename T, typename... Args>
    T* AddSystem(Args&&... args) {
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = sys.get();
        m_systems.push_back(std::move(sys));
        return raw;
    }

    template<typename T>
    T* GetSystem() {
        for (auto& sys : m_systems) {
            if (auto casted = dynamic_cast<T*>(sys.get()))
                return casted;
        }
        return nullptr;
    }

    void Initialize() const;

    void Update() const;

private:
	EntityManager m_entityManager;
	std::vector<std::unique_ptr<Engine::ISystem>> m_systems;
};

#endif