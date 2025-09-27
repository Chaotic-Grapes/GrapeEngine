#ifndef WORLD_H
#define WORLD_H

#include <memory>
#include <vector>
#include "EntityManager.h"
#include "ISystem.h"

class Entity;
namespace Engine { class Application; } // Forward declaration for friend class
class World {
public:
    World() { m_entityManager.SetWorld(this); }
	~World() { _shutdown(); }

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

    template<typename T>
    void ForEachEntity(T func) {
        auto entityIds = m_entityManager.GetAllEntities();
        for (EntityId id : entityIds) {
            Entity entity = m_entityManager.GetEntity(id);
            func(entity);
        }
    }

private:
	friend class Engine::Application;

	EntityManager m_entityManager;
	std::vector<std::unique_ptr<Engine::ISystem>> m_systems;

    void _initialize() const;
    void _update() const;
    void _shutdown();
};

#endif