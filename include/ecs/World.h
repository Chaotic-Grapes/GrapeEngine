#ifndef WORLD_H
#define WORLD_H

#include <memory>
#include <vector>
#include "ecs/EntityManager.h"
#include "ecs/ISystem.h"
#include "systems/Behaviour.h"
#include "systems/AudioEngine.h"

class Entity;
class Scene;
namespace Engine { class Application; } // Forward declaration for friend class

class World {
public:
    World() { m_entityManager.SetWorld(this); }
    ~World() = default;

    EntityManager& GetEntityManager();

    Entity CreateEntity(const std::string& name = "GameObject");

    template<typename T, typename... Args>
    T* AddSystem(Args&&... args);

    template<typename T>
    T* GetSystem();

    // In World because World consists of logic
    // Whereas EntityManager consists of only data
    template<typename T, typename... Args>
    T& AddBehaviour(Entity& entity, Args&&... args);

    template<typename T>
    void ForEachEntity(T func);

    void RemoveAllBehaviours(const Entity& entity);

    const std::unordered_map<EntityId, std::vector<std::unique_ptr<Behaviour>>>& GetBehaviours() const;

private:
	friend class Scene;
    friend class Engine::Application;

	EntityManager m_entityManager;
	std::vector<std::unique_ptr<Engine::ISystem>> m_systems;
    std::unordered_map<EntityId, std::vector<std::unique_ptr<Behaviour>>> m_behaviours;

    void _initialize() const;
    void _update() const;
    void _shutdown();
    void _fixedUpdate() const;
    void _lateUpdate() const;
};

#include "World.inl"

#endif