#include "Physics.h"
#include <iostream>

namespace Engine {

    // initialize gravity
    glm::vec3 PhysicsSystem::m_gravity = glm::vec3(0.0f, -9.81f, 0.0f);

    void PhysicsSystem::Initialize() {
        std::cout << "Physics System Initialized" << std::endl; // placeholder for now
    }

    void PhysicsSystem::Update() {

        m_accumulator += Time::DeltaTime(); // accumulate time from last frame

        // fixed time physics update
        while (m_accumulator >= Time::FixedDeltaTime()) {
            FixedUpdate();
            m_accumulator -= Time::FixedDeltaTime();
        }
    }

    void PhysicsSystem::FixedUpdate() {
        const float fixedDt = Time::FixedDeltaTime();

        for (auto& component : m_entities) {
            if (!component) continue;

            // this resets acceleration each frame!!! (Only applied forces)
            component->acceleration = glm::vec3(0.0f, 0.0f, 0.0f);

            // 1. GRAVITY
            // apply gravity if enabled
            if (component->useGravity) {
                component->acceleration += m_gravity;
            }

            // 2. BUOYANCY
            // apply buoyancy if enabled
            if (component->useBuoyancy) {
                // F_b = -gravity * buoyancyFactor * mass
                glm::vec3 buoyancyForce = -m_gravity * component->buoyancy * component->mass;   // initialized here to save memory (temporary calc.)
                component->acceleration += buoyancyForce / component->mass;
            }

            // 3. DRAG
            // F_d = -velocity * dragCoefficient
            glm::vec3 dragForce = -component->velocity * component->dragCoefficient;
            component->acceleration += dragForce / component->mass;

            // 4. DAMPING
            // applies damping to velocity like basic resistance
            // when damping = 1.0, no slowdown. when damping = 0.0, instantly stops
            component->velocity *= component->damping;

            // 5. ANGULAR DRAG
            // (rotational resistance)
            component->angularVelocity *= component->angularDrag;

            // integrates acceleration into velocity (a = F/m)
            component->velocity += component->acceleration * fixedDt;
/*
            // Maximum Velocity
            const float maxSpeed = 100.0f;
            if (glm::length (component->velocity) > maxSpeed){
                component->velocity = glm::normalize(component->velocity) * maxSpeed;
            }
*/
            // integrates velocity into position
            component->position += component->velocity * fixedDt;
        }
    }

    void PhysicsSystem::ApplyForce(PhysicsComponent* component, const glm::vec3& force) {
        if (component && component->mass > 0.0f) { // checks for division by zero
            component->acceleration += force / component->mass;
        }
    }

    void PhysicsSystem::ApplyImpulse(PhysicsComponent* component, const glm::vec3& impulse) {
        if (component) {
            component->velocity += impulse / component->mass;
        }
    }

    void PhysicsSystem::SetGravity(const glm::vec3& gravity) {
        m_gravity = gravity;
    }

    // get current global gravity
    glm::vec3 PhysicsSystem::GetGravity() {
        return m_gravity;
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