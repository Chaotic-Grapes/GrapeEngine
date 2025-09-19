#include "Physics.h"
#include "Math/Vector3D.h"  
#include <iostream>
#include <vector>
#include "ecs/Components.h"

namespace Engine {

    // initialize gravity
    Vector3D PhysicsSystem::m_gravity = Vector3D(0.0f, -9.81f, 0.0f);

    void PhysicsSystem::OnCreate() {
        std::cout << "Physics System Initialized" << std::endl; // placeholder for now
    }

    void PhysicsSystem::OnUpdate() {

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
            component->acceleration = Vector3D(0.0f, 0.0f, 0.0f);

            // 1. GRAVITY
            // apply gravity if enabled
            if (component->useGravity) {
                component->acceleration += m_gravity;
            }

            // 2. BUOYANCY
            // apply buoyancy if enabled
            if (component->useBuoyancy) {
                // F_b = -gravity * buoyancyFactor * mass
                Vector3D buoyancyForce = -m_gravity * component->buoyancy * component->mass;   // initialized here to save memory (temporary calc.)
                component->acceleration += buoyancyForce / component->mass;
            }

            // 3. DRAG
            // F_d = -velocity * dragCoefficient
            Vector3D dragForce = -component->velocity * component->dragCoefficient;
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
            if (vec.length(component->velocity) > maxSpeed){
                component->velocity = vec.Normalize(component->velocity) * maxSpeed;
            }
*/
            // integrates velocity into position
            component->position += component->velocity * fixedDt;
        }
    }

    void PhysicsSystem::ApplyForce(PhysicsComponent* component, const Vector3D& force) {
        if (component && component->mass > 0.0f) { // checks for division by zero
            component->acceleration += force / component->mass;
        }
    }

    void PhysicsSystem::ApplyImpulse(PhysicsComponent* component, const Vector3D& impulse) {
        if (component) {
            component->velocity += impulse / component->mass;
        }
    }

    void PhysicsSystem::SetGravity(const Vector3D& gravity) {
        m_gravity = gravity;
    }

    // get current global gravity
    Vector3D PhysicsSystem::GetGravity() {
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