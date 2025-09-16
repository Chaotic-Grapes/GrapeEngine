#ifndef PHYSICS_H
#define PHYSICS_H

#include "ISystem.h"
#include "Time.h"
#include <vector>
#include <glm/glm.hpp>

namespace Engine {

    struct PhysicsComponent {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 acceleration;
        glm::vec3 angularVelocity;
        float damping;
        float mass;
        bool useGravity;

        // underwater stuff
        bool useBuoyancy;
        float buoyancy;                      
        float dragCoefficient;               
        float angularDrag;                          // rotational resistance

/*      
        // FUTURE ROTATIONAL PHYSICS
        glm::vec3 rotation;                         // current orientation in radians (or quaternion ??)
        glm::vec3 torque;                           // rotational force
        float inertiaMoment;                        // rotational mass
*/

        // TEMPORARY INITIALISATION 
        PhysicsComponent()
            : position(0.0f, 0.0f, 0.0f)
            , velocity(0.0f, 0.0f, 0.0f)
            , acceleration (0.0f, 0.0f, 0.0f)
            , damping(0.98f)                        // damping range from 0-1, higher means more resistance
            , mass(1.0f)
            , useGravity(true)                      // gravity is on by default

            , angularVelocity(0.0f, 0.0f, 0.0f)
            , useBuoyancy(false)                    // buoyancy is off by default for backward compatibility (CHANGE WHEN NEEDED!)
            , buoyancy(0.8f)                        // 0 = sinks, 1 = neutral, >1 = floats        
            , dragCoefficient(2.0f)                 // water has higher drag than air
            , angularDrag(0.95f)
        {}
    };

    class PhysicsSystem : public ISystem {
    public:
        void Initialize() override;
        void Update() override;
        std::string Name() const override { return "Physics"; }

        // Entities
        void AddEntity(PhysicsComponent* component);
        void RemoveEntity(PhysicsComponent* component);

        static void ApplyForce(PhysicsComponent* component, const glm::vec3& force);
        static void ApplyImpulse(PhysicsComponent* component, const glm::vec3& impulse);

        static void SetGravity(const glm::vec3& gravity);
        static glm::vec3 GetGravity();

    private:
        void FixedUpdate();

        std::vector<PhysicsComponent*> m_entities;

        static glm::vec3 m_gravity;

        float m_accumulator = 0.0f;
    };

} // namespace Engine

#endif // PHYSICS_H