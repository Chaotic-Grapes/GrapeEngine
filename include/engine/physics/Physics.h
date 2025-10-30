#ifndef PHYSICS_H
#define PHYSICS_H

#include "ecs/Components.h"
#include "math/Vector2D.h"

namespace Engine {
    class Physics {
    public:
        // Boundary collision
        struct BoundaryConstraint {
            float MinX, MaxX, MinY, MaxY;
            bool KillVelocity;
            float Restitution; // Bounciness when hitting boundaries (0 = no bounce, 1 = perfect bounce)
        };

        //Generic Collision 
        struct CollisionResult {
            bool Collided; //flag to show collision
            Vector2D Normal;
            float Depth;
            float RelativeNormalVelocity;
        };

        // Gravity management
        static void SetGravity(const Vector2D& gravity) { m_gravity = gravity; }
        static Vector2D GetGravity() { return m_gravity; }

        // Force and impulse application
        static Vector2D CalculateAcceleration(const ECS::Components::Rigidbody2D& rb, const ECS::Components::LinearVelocity2D& vel);
        static void ApplyForce(const ECS::Components::Rigidbody2D& rb, ECS::Components::LinearVelocity2D& vel, const Vector2D& force);
        static void ApplyImpulse(const ECS::Components::Rigidbody2D& rb, ECS::Components::LinearVelocity2D& vel, const Vector2D& impulse);

        // Physics system control
        static void SetEnabled(bool enabled) { m_enabled = enabled; }
        static bool IsEnabled() { return m_enabled; }

        // World boundary management
        static void SetWorldBounds(float minX, float maxX, float minY, float maxY, bool killVelocity = false, float restitution = 0.8f);
        static void EnableWorldBounds(bool enable) { m_worldBoundsEnabled = enable; }
        static bool IsWorldBoundsEnabled() { return m_worldBoundsEnabled; }
        static const BoundaryConstraint& GetWorldBounds() { return m_worldBounds; }

        // Utility methods
        static float GetInverseMass(float mass);
        static float Dot(const Vector2D& a, const Vector2D& b);

        // Velocity manipulation
        static void ApplyVelocityDamping(ECS::Components::LinearVelocity2D& vel, const float dampingFactor);
        static void ReflectVelocity(ECS::Components::LinearVelocity2D& vel, const Vector2D& normal);
        static void ZeroVelocityComponent(ECS::Components::LinearVelocity2D& vel, bool isXAxis, bool isPositive);

        // Angular Damping
        static void ApplyAngularDamping(ECS::Components::AngularVelocity2D& angularVel, float dampingFactor);

        static bool ApplyBoundaryConstraint(
            Vector2D& position, 
            Vector2D& velocity, 
            float radius,
            const BoundaryConstraint& bounds,
            float entityRestitution = -1.0f  // -1 means use bounds.Restitution
        );

        static CollisionResult ResolveCollision(
            const ECS::Components::Rigidbody2D& rbA,
            const ECS::Components::Rigidbody2D& RbB,
            ECS::Components::LinearVelocity2D& velA,
            ECS::Components::LinearVelocity2D& VelB,
            ECS::Components::LocalTransform& transformA,
            ECS::Components::LocalTransform& transformB,
            const Vector2D& normal,
            float depth,
            const ECS::Components::PhysicsMaterial2D& physics
        );

    private:
        static Vector2D m_gravity;
        static bool m_enabled;
        static bool m_worldBoundsEnabled;
        static BoundaryConstraint m_worldBounds;

        // Helper constants
        static constexpr float MIN_DISTANCE_SQUARED = 1e-6f;
        static constexpr float MIN_TANGENT_LENGTH_SQUARED = 1e-12f;
    };
}

#endif
