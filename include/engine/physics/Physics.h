#ifndef PHYSICS_H
#define PHYSICS_H

#include "ecs/Components.h"
#include "math/Vector2D.h"

namespace Engine {
    class Physics {
    public:
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

        // Utility methods
        static float GetInverseMass(float mass);
        static float Dot(const Vector2D& a, const Vector2D& b);

        // Velocity manipulation
        static void ApplyVelocityDamping(ECS::Components::LinearVelocity2D& vel, const float dampingFactor);
        static void ReflectVelocity(ECS::Components::LinearVelocity2D& vel, const Vector2D& normal);
        static void ZeroVelocityComponent(ECS::Components::LinearVelocity2D& vel, bool isXAxis, bool isPositive);

        // Boundary collision
        struct BoundaryConstraint {
            float MinX, MaxX, MinY, MaxY;
            bool KillVelocity;
        };
        
        static bool ApplyBoundaryConstraint(
            Vector2D& position, 
            Vector2D& velocity, 
            float radius,
            const BoundaryConstraint& bounds
        );

        // Circle-AABB collision resolution
        struct CircleAABBResult {
            bool Collided;
            Vector2D PenetrationNormal;
            float Penetration;
        };

        static CircleAABBResult ResolveCircleAABBCollision(
            ECS::Components::LocalTransform& circleTransform,
            ECS::Components::LinearVelocity2D& circleVelocity,
            const Vector2D& boxMin,
            const Vector2D& boxMax,
            float circleRadius,
            float epsilon = 0.001f
        );

        struct CircleCollisionResult {
            bool Collided;
            Vector2D Normal;
            float Depth;
            float RelativeNormalVelocity;
        };

        static CircleCollisionResult ResolveCircleCircleCollision(
            const ECS::Components::Rigidbody2D& rbA,
            const ECS::Components::Rigidbody2D& rbB,
            ECS::Components::LinearVelocity2D& velA,
            ECS::Components::LinearVelocity2D& velB,
            ECS::Components::LocalTransform& transformA,
            ECS::Components::LocalTransform& transformB,
            float radiusA,
            float radiusB,
            const ECS::Components::PhysicsMaterial2D& physics
        );

    private:
        static Vector2D m_gravity;
        static bool m_enabled;

        // Helper constants
        static constexpr float MIN_DISTANCE_SQUARED = 1e-6f;
        static constexpr float MIN_TANGENT_LENGTH_SQUARED = 1e-12f;
    };
}

#endif
