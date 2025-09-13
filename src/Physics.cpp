#include "Physics.h"
#include <iostream>

namespace Engine {

    void PhysicsSystem::Initialize() {
        std::cout << "Physics System Initialized - Linear Velocity & Damping Only" << std::endl; // placeholder for now
    }

    void PhysicsSystem::Update() {

        m_accumulator += Time::DeltaTime();

        while (m_accumulator >= m_fixedTimestep) {
            FixedUpdate();
            m_accumulator -= m_fixedTimestep;
        }
    }

    void PhysicsSystem::FixedUpdate() {
        const float fixedDt = Time::FixedDeltaTime();

        for (auto& component : m_entities) {
            if (!component) continue;

            component->velocity *= component->damping;
            component->position += component->velocity * fixedDt;

        }
    }

    void PhysicsSystem::AddEntity(PhysicsComponent* component) {
        if (component) {
            m_entities.push_back(component);
        }
    }

    void PhysicsSystem::RemoveEntity(PhysicsComponent* component) {
        m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), component), m_entities.end());
    }

} // namespace Engine