#ifndef PHYSICS_H
#define PHYSICS_H

#include "Export.h"
#include "ecs/Components.h"
#include "math/Vector2D.h"
#include "Collision.h"

namespace Engine {
    class GRAPEENGINE_API Physics {
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
        static float CalculateAngularAcceleration(const ECS::Components::Rigidbody2D& rb, const ECS::Components::AngularVelocity2D& angVel);

        static bool ApplyBoundaryConstraint(
            Vector2D& position, 
            Vector2D& velocity, 
            float radius,
            const BoundaryConstraint& bounds,
            float entityRestitution = -1.0f  // -1 means use bounds.Restitution
        );

        static CollisionResult ResolveCollisionManifold(
            const ECS::Components::Rigidbody2D& rbA,
            const ECS::Components::Rigidbody2D& rbB,
            ECS::Components::LinearVelocity2D& velA,
            ECS::Components::LinearVelocity2D& velB,
            ECS::Components::LocalTransform& transformA,
            ECS::Components::LocalTransform& transformB,
            const Collision::ContactManifold& manifold,
            const ECS::Components::PhysicsMaterial2D& physics
        );

        // =========================================================================
        // World-space shape helpers (Step 1 of refactor)
        // =========================================================================
        // These compute world-space shapes by applying transform position, collider
        // offset, and scale - ensuring consistent shapes between broad and narrow phase.

        /**
         * @brief Compute world-space circle from components.
         * Applies transform position, collider offset, and scale to radius.
         */
        static WorldCircle GetWorldCircle(
            const ECS::Components::CircleCollider2D& circle,
            const ECS::Components::LocalTransform& transform)
        {
            WorldCircle result;
            result.Center.X = transform.Position.X + circle.Offset.X;
            result.Center.Y = transform.Position.Y + circle.Offset.Y;
            
            // Apply scale (use average of X and Y scale)
            float scale = (transform.Scale.X + transform.Scale.Y) * 0.5f;
            result.Radius = circle.Radius * scale;
            
            return result;
        }

        /**
         * @brief Compute world-space AABB from components.
         * Applies transform position, collider offset, and scale to half-extents.
         */
        static WorldAABB GetWorldAABB(
            const ECS::Components::BoxCollider2D& box,
            const ECS::Components::LocalTransform& transform)
        {
            WorldAABB result;
            result.Center.X = transform.Position.X + box.Offset.X;
            result.Center.Y = transform.Position.Y + box.Offset.Y;
            
            // Apply scale to half-extents
            result.HalfExtents.X = box.HalfExtents.X * transform.Scale.X;
            result.HalfExtents.Y = box.HalfExtents.Y * transform.Scale.Y;
            
            return result;
        }

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
