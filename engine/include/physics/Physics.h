/* Start Header *****************************************************************/
/*!
\file   Physics.h
\author Dalton Koh Shi Hao , 2403250
\par    d.koh@digipen.edu

\brief
Declaration of the Physics class for 2D physics simulation. Provides methods for
gravity management, force application, collision resolution, and world boundary
constraints. Includes utilities for computing world-space shapes from entity
components.
*
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
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
        /**
         * @brief Global world-boundary constraint configuration.
         */
        struct BoundaryConstraint {
            float MinX, MaxX, MinY, MaxY;
            bool KillVelocity;
            float Restitution; // Bounciness when hitting boundaries (0 = no bounce, 1 = perfect bounce)
        };

        /**
         * @brief Result payload for collision manifold resolution.
         */
        struct CollisionResult {
            bool Collided; //flag to show collision
            Vector2D Normal;
            float Depth;
            float RelativeNormalVelocity;
        };

        /**
         * @brief Set global gravity acceleration.
         * @param gravity Gravity vector in world units per second squared.
         */
        static void SetGravity(const Vector2D& gravity) { m_gravity = gravity; }

        /**
         * @brief Get global gravity acceleration.
         * @return Current gravity vector.
         */
        static Vector2D GetGravity() { return m_gravity; }

        /**
         * @brief Compute linear acceleration from damping terms.
         * @param rb Rigidbody parameters.
         * @param vel Current linear velocity.
         * @return Linear acceleration.
         */
        static Vector2D CalculateAcceleration(const ECS::Components::Rigidbody2D& rb, const ECS::Components::LinearVelocity2D& vel);

        /**
         * @brief Apply a continuous force as velocity change.
         * @param rb Rigidbody parameters.
         * @param vel Velocity to modify.
         * @param force Force in world space.
         */
        static void ApplyForce(const ECS::Components::Rigidbody2D& rb, ECS::Components::LinearVelocity2D& vel, const Vector2D& force);

        /**
         * @brief Apply an instantaneous impulse as velocity change.
         * @param rb Rigidbody parameters.
         * @param vel Velocity to modify.
         * @param impulse Impulse in world space.
         */
        static void ApplyImpulse(const ECS::Components::Rigidbody2D& rb, ECS::Components::LinearVelocity2D& vel, const Vector2D& impulse);

        /**
         * @brief Enable or disable physics processing globally.
         * @param enabled True to enable physics.
         */
        static void SetEnabled(bool enabled) { m_enabled = enabled; }

        /**
         * @brief Check whether global physics processing is enabled.
         * @return True when physics is enabled.
         */
        static bool IsEnabled() { return m_enabled; }

        /**
         * @brief Configure world-boundary limits and response behavior.
         * @param minX Minimum X bound.
         * @param maxX Maximum X bound.
         * @param minY Minimum Y bound.
         * @param maxY Maximum Y bound.
         * @param killVelocity Whether to zero velocity on collision.
         * @param restitution Bounce coefficient in [0,1].
         */
        static void SetWorldBounds(float minX, float maxX, float minY, float maxY, bool killVelocity = false, float restitution = 0.8f);

        /**
         * @brief Enable or disable world-boundary enforcement.
         * @param enable True to enforce configured world bounds.
         */
        static void EnableWorldBounds(bool enable) { m_worldBoundsEnabled = enable; }

        /**
         * @brief Check whether world-boundary constraints are enabled.
         * @return True when world bounds are enabled.
         */
        static bool IsWorldBoundsEnabled() { return m_worldBoundsEnabled; }

        /**
         * @brief Access current world-boundary configuration.
         * @return Boundary constraint settings.
         */
        static const BoundaryConstraint& GetWorldBounds() { return m_worldBounds; }

        /**
         * @brief Return inverse mass with static-body safety.
         * @param mass Mass value.
         * @return `1/mass` for positive mass, else `0`.
         */
        static float GetInverseMass(float mass);

        /**
         * @brief Compute 2D dot product.
         * @param a First vector.
         * @param b Second vector.
         * @return Dot product result.
         */
        static float Dot(const Vector2D& a, const Vector2D& b);

        /**
         * @brief Scale linear velocity by a damping factor.
         * @param vel Velocity to damp.
         * @param dampingFactor Multiplicative damping factor.
         */
        static void ApplyVelocityDamping(ECS::Components::LinearVelocity2D& vel, const float dampingFactor);

        /**
         * @brief Remove inward normal component from velocity.
         * @param vel Velocity to modify.
         * @param normal Surface normal.
         */
        static void ReflectVelocity(ECS::Components::LinearVelocity2D& vel, const Vector2D& normal);

        /**
         * @brief Zero one signed component of velocity.
         * @param vel Velocity to modify.
         * @param isXAxis True to target X, false to target Y.
         * @param isPositive True to zero positive values, false for negative values.
         */
        static void ZeroVelocityComponent(ECS::Components::LinearVelocity2D& vel, bool isXAxis, bool isPositive);

        /**
         * @brief Compute angular acceleration from angular damping.
         * @param rb Rigidbody parameters.
         * @param angVel Current angular velocity.
         * @return Angular acceleration.
         */
        static float CalculateAngularAcceleration(const ECS::Components::Rigidbody2D& rb, const ECS::Components::AngularVelocity2D& angVel);

        /**
         * @brief Resolve a circle-like boundary collision against world bounds.
         * @param position Body position.
         * @param velocity Body velocity.
         * @param radius Body collision radius.
         * @param bounds Boundary constraints.
         * @param entityRestitution Optional per-entity restitution override.
         * @return True when any boundary collision occurred.
         */
        static bool ApplyBoundaryConstraint(
            Vector2D& position, 
            Vector2D& velocity, 
            float radius,
            const BoundaryConstraint& bounds,
            float entityRestitution = -1.0f  // -1 means use bounds.Restitution
        );

        /**
         * @brief Resolve impulse and position correction for a contact manifold.
         * @param rbA Rigidbody A.
         * @param rbB Rigidbody B.
         * @param velA Velocity A (in/out).
         * @param velB Velocity B (in/out).
         * @param transformA Transform A (in/out).
         * @param transformB Transform B (in/out).
         * @param manifold Contact manifold.
         * @param physics Effective collision material.
         * @return Collision resolution summary.
         */
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


        // World-space shape helpers
        // These compute world-space shapes by applying transform position, collider
        // offset, and scale - ensuring consistent shapes between broad and narrow phase.

        /**
         * @brief Compute world-space circle from components.
         * @param circle Circle collider component.
         * @param transform Entity transform component.
         * @return World-space circle.
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
         * @param box Box collider component.
         * @param transform Entity transform component.
         * @return World-space AABB enclosing the oriented box.
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
         * @param box Box collider component.
         * @param transform Entity transform component.
         * @return World-space oriented bounding box.
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
