#ifndef PHYSICS_H
#define PHYSICS_H

#include "systems/Time.h"
#include "Math/Vector3D.h"
#include <vector>
#include "ecs/ISystem.h"
#include "ecs/World.h"

namespace Engine {

    struct PhysicsComponent {
        Vector3D position;
        Vector3D velocity;
        Vector3D acceleration;
        Vector3D angularVelocity;
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
        Vector3D rotation;                         // current orientation in radians (or quaternion ??)
        Vector3D torque;                           // rotational force
        float inertiaMoment;                        // rotational mass
*/

        // TEMPORARY INITIALISATION 
        PhysicsComponent()
            : position(0.0f, 0.0f, 0.0f)
            , velocity(0.0f, 0.0f, 0.0f)
            , acceleration (0.0f, 0.0f, 0.0f)
            , angularVelocity(0.0f, 0.0f, 0.0f)                        // damping range from 0-1, higher means more resistance
            , damping(0.98f)
            , mass(1.0f)                      // gravity is on by default

            , useGravity(true)
            , useBuoyancy(false)                    // buoyancy is off by default for backward compatibility (CHANGE WHEN NEEDED!)
            , buoyancy(0.8f)                        // 0 = sinks, 1 = neutral, >1 = floats        
            , dragCoefficient(2.0f)                 // water has higher drag than air
            , angularDrag(0.95f)
        {}
    };

    class PhysicsSystem : public ISystem {
    public:
        PhysicsSystem(World* world) : m_world(world) {}

        void OnCreate() override;
        void OnUpdate() override;
        std::string Name() const override { return "Physics"; }

        // Entities
        void AddEntity(PhysicsComponent* component);
        void RemoveEntity(PhysicsComponent* component);

        static void ApplyForce(PhysicsComponent* component, const Vector3D& force);
        static void ApplyImpulse(PhysicsComponent* component, const Vector3D& impulse);

        static void SetGravity(const Vector3D& gravity);
        static Vector3D GetGravity();

    private:
        void FixedUpdate();

        std::vector<PhysicsComponent*> m_entities;

        static Vector3D m_gravity;

        float m_accumulator = 0.0f;

        World* m_world;
    };

} // namespace Engine

#endif // PHYSICS_H