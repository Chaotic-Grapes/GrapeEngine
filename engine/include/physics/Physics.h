/* Start Header *****************************************************************/
/*!
\file   Physics.h
\author Dalton Koh Shi Hao (100%)
\par    d.koh@digipen.edu

\brief
Declaration of the Physics class for 2D physics simulation. Provides methods for
gravity management, force application, collision resolution, and world boundary
constraints. Includes utilities for computing world-space shapes from entity
components.
*/
/* End Header *******************************************************************/

#ifndef PHYSICS_H
#define PHYSICS_H

#include "Export.h"
#include "ecs/Components.h"
#include "math/Vector2D.h"
#include "Collision.h"
#include <algorithm>
#include <cmath>

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
        // Check whether world bounds enabled.
        static bool IsWorldBoundsEnabled() { return m_worldBoundsEnabled; }
        // Return the current world bounds configuration.
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

            // Rotate local offset by entity rotation
            const Quaternion& q = transform.Rotation;
            const float angle = std::atan2(
                2.0f * (q.W * q.Z + q.X * q.Y),
                1.0f - 2.0f * (q.Y * q.Y + q.Z * q.Z)
            );
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const Vector2D offset{
                circle.Offset.X * c - circle.Offset.Y * s,
                circle.Offset.X * s + circle.Offset.Y * c
            };

            result.Center.X = transform.Position.X + offset.X;
            result.Center.Y = transform.Position.Y + offset.Y;
            
            // Apply scale (use average of X and Y scale)
            const float scale = (std::abs(transform.Scale.X) + std::abs(transform.Scale.Y)) * 0.5f;
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
            const WorldOBB obb = GetWorldOBB(box, transform);

            // Compute AABB from OBB corners (for broad-phase)
            Vector2D corners[4] = {
                obb.Center + obb.AxisX * obb.HalfExtents.X + obb.AxisY * obb.HalfExtents.Y,
                obb.Center - obb.AxisX * obb.HalfExtents.X + obb.AxisY * obb.HalfExtents.Y,
                obb.Center - obb.AxisX * obb.HalfExtents.X - obb.AxisY * obb.HalfExtents.Y,
                obb.Center + obb.AxisX * obb.HalfExtents.X - obb.AxisY * obb.HalfExtents.Y
            };

            // Find min/max X and Y from corners
            float minX = corners[0].X;
            float maxX = corners[0].X;
            float minY = corners[0].Y;
            float maxY = corners[0].Y;

            // Iterate over remaining corners to find min/max X and Y
            for (int i = 1; i < 4; ++i) {
                minX = std::min(minX, corners[i].X);
                maxX = std::max(maxX, corners[i].X);
                minY = std::min(minY, corners[i].Y);
                maxY = std::max(maxY, corners[i].Y);
            }

            // Construct AABB
            WorldAABB result;
            result.Center.X = (minX + maxX) * 0.5f;
            result.Center.Y = (minY + maxY) * 0.5f;
            result.HalfExtents.X = (maxX - minX) * 0.5f;
            result.HalfExtents.Y = (maxY - minY) * 0.5f;

            return result;
        }

        /**
         * @brief Compute world-space OBB from components.
         * Applies transform position, collider offset, scale, and rotation.
         */
        static WorldOBB GetWorldOBB(
            const ECS::Components::BoxCollider2D& box,
            const ECS::Components::LocalTransform& transform)
        {
            WorldOBB result;

            // Compute entity Z rotation from quaternion
            const Quaternion& q = transform.Rotation;
            const float entityAngle = std::atan2(
                2.0f * (q.W * q.Z + q.X * q.Y),
                1.0f - 2.0f * (q.Y * q.Y + q.Z * q.Z)
            );

            // Precompute cosine and sine of entity rotation
            const float c = std::cos(entityAngle);
            const float s = std::sin(entityAngle);

            // Rotate local offset by entity rotation
            const Vector2D offset{ box.Offset.X, box.Offset.Y };
            const Vector2D rotatedOffset{
                offset.X * c - offset.Y * s,
                offset.X * s + offset.Y * c
            };

            result.Center.X = transform.Position.X + rotatedOffset.X;
            result.Center.Y = transform.Position.Y + rotatedOffset.Y;

            // Apply scale to half-extents
            result.HalfExtents.X = box.HalfExtents.X * std::abs(transform.Scale.X);
            result.HalfExtents.Y = box.HalfExtents.Y * std::abs(transform.Scale.Y);

            // Compute final rotation
            result.Rotation = entityAngle + box.Rotation;

            // Compute axes
            const float cr = std::cos(result.Rotation);
            const float sr = std::sin(result.Rotation);
            result.AxisX = Vector2D(cr, sr);
            result.AxisY = Vector2D(-sr, cr);

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
