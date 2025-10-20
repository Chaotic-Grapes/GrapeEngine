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
        static Vector2D CalculateAcceleration(const ECS::Components::Rigidbody2D& rb);
        static void ApplyForce(ECS::Components::Rigidbody2D& rb, const Vector2D& force);
        static void ApplyImpulse(ECS::Components::Rigidbody2D& rb, const Vector2D& impulse);

        // Physics system control
        static void SetEnabled(bool enabled) { m_enabled = enabled; }
        static bool IsEnabled() { return m_enabled; }

        // Utility methods
        static float GetInverseMass(float mass);
        static float Dot(const Vector2D& a, const Vector2D& b);

        // Velocity manipulation
        static void ApplyVelocityDamping(ECS::Components::Rigidbody2D& rb, float dampingFactor);
        static void ReflectVelocity(Vector2D& velocity, const Vector2D& normal);
        static void ZeroVelocityComponent(Vector2D& velocity, bool isXAxis, bool isPositive);

        // Boundary collision
        struct BoundaryConstraint {
            float minX, maxX, minY, maxY;
            bool killVelocity;
        };
        
        static bool ApplyBoundaryConstraint(
            Vector2D& position, 
            Vector2D& velocity, 
            float radius,
            const BoundaryConstraint& bounds
        );

        // Circle-AABB collision resolution
        struct CircleAABBResult {
            bool collided;
            Vector2D penetrationNormal;
            float penetration;
        };

        static CircleAABBResult ResolveCircleAABBCollision(
            Vector2D& circlePosition,
            Vector2D& circleVelocity,
            const Vector2D& boxMin,
            const Vector2D& boxMax,
            float circleRadius,
            float epsilon = 0.001f
        );

        // Circle-Circle collision resolution
        struct CollisionParams {
            float restitution;
            float friction;
            float positionCorrectionPercent;
        };

        struct CircleCollisionResult {
            bool collided;
            Vector2D normal;
            float depth;
            float relativeNormalVelocity;
        };

        static CircleCollisionResult ResolveCircleCircleCollision(
            ECS::Components::Rigidbody2D& rbA,
            ECS::Components::Rigidbody2D& rbB,
            Vector2D& positionA,
            Vector2D& positionB,
            float radiusA,
            float radiusB,
            const CollisionParams& params
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
