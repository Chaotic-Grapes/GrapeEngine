/**
 * @Name:  Daniel Kay Neo Zuo Feng
 * @Email: k.danielneozuofeng@digipen.edu
 * @File: Physics.cpp
 * @Brief: Core 2D physics system implementing forces, damping, collisions, and world constraints.
 *
 * @Details:
 * Provides fundamental physics utilities for the engine, including acceleration,
 * impulse and force application, boundary constraint handling, and full collision
 * resolution with restitution and friction responses between entities.
 * Used by PhysicsSystem.cpp for per-frame updates and collision response.
 *
 * @References:
 * https://box2d.org/documentation/
 * https://box2d.org/files/ErinCatto_SequentialImpulses_GDC2006.pdf
 * https://box2d.org/files/ErinCatto_IterativeDynamics_GDC2005.pdf
 * https://gafferongames.com/post/integration_basics/
 * https://www.toptal.com/game/video-game-physics-part-i-an-introduction-to-rigid-body-dynamics
 * 
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "physics/Physics.h"
#include <cmath>
#include <algorithm>
#include "helpers/MathUtils.h"

namespace Engine {

    Vector2D Physics::m_gravity = Vector2D(0.0f, -9.81f);
    bool Physics::m_enabled = true;
    bool Physics::m_worldBoundsEnabled = false;
    Physics::BoundaryConstraint Physics::m_worldBounds = { 0.0f, 1600.0f, 0.0f, 900.0f, false, 0.8f };

    /**
     * @brief Compute linear acceleration from damping terms.
     * @param rb Rigidbody parameters.
     * @param vel Current linear velocity.
     * @return Linear acceleration.
     */
    Vector2D Physics::CalculateAcceleration(const ECS::Components::Rigidbody2D& rb, const ECS::Components::LinearVelocity2D& vel) {
        Vector2D acceleration(0.f, 0.f);
        //acceleration += m_gravity * rb.GravityScale;
        acceleration += Vector2D(-vel.Value.X * rb.LinearDamping, -vel.Value.Y * rb.LinearDamping);// / rb.Mass;
        return acceleration;
    }

    /**
     * @brief Apply a continuous force as velocity change.
     * @param rb Rigidbody parameters.
     * @param vel Velocity to modify.
     * @param force Force in world space.
     */
    void Physics::ApplyForce(const ECS::Components::Rigidbody2D& rb, ECS::Components::LinearVelocity2D& vel, const Vector2D& force) {
        if (rb.Mass > 0)
            vel.Value += force / rb.Mass;
    }

    /**
     * @brief Apply an instantaneous impulse as velocity change.
     * @param rb Rigidbody parameters.
     * @param vel Velocity to modify.
     * @param impulse Impulse in world space.
     */
    void Physics::ApplyImpulse(const ECS::Components::Rigidbody2D& rb, ECS::Components::LinearVelocity2D& vel, const Vector2D& impulse) {
        if (rb.Mass > 0)
            vel.Value += impulse / rb.Mass;
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    /**
     * @brief Configure global world-boundary constraints.
     * @param minX Minimum X bound.
     * @param maxX Maximum X bound.
     * @param minY Minimum Y bound.
     * @param maxY Maximum Y bound.
     * @param killVelocity Whether to zero velocity on boundary collision.
     * @param restitution Bounce coefficient in [0,1].
     */
    void Physics::SetWorldBounds(const float minX, const float maxX, const float minY, const float maxY, const bool killVelocity, const float restitution) {
        m_worldBounds.MinX = minX;
        m_worldBounds.MaxX = maxX;
        m_worldBounds.MinY = minY;
        m_worldBounds.MaxY = maxY;
        m_worldBounds.KillVelocity = killVelocity;
        m_worldBounds.Restitution = std::clamp(restitution, 0.0f, 1.0f);
        m_worldBoundsEnabled = true;
    }

    /**
     * @brief Return inverse mass with static-body safety.
     * @param mass Mass value.
     * @return `1/mass` for positive mass, else `0`.
     */
    float Physics::GetInverseMass(const float mass) {
        return (mass > 0.0f) ? 1.0f / mass : 0.0f;
    }

    /**
     * @brief Compute 2D dot product.
     * @param a First vector.
     * @param b Second vector.
     * @return Dot product result.
     */
    float Physics::Dot(const Vector2D& a, const Vector2D& b) {
        return a.X * b.X + a.Y * b.Y;
    }

    // ============================================================================
    // Velocity Manipulation
    // ============================================================================

    /**
     * @brief Scale linear velocity by a damping factor.
     * @param vel Velocity to damp.
     * @param dampingFactor Multiplicative damping factor.
     */
    void Physics::ApplyVelocityDamping(ECS::Components::LinearVelocity2D& vel, const float dampingFactor) {
        vel.Value *= dampingFactor;
    }

    /**
     * @brief Remove inward normal component from velocity.
     * @param vel Velocity to modify.
     * @param normal Surface normal.
     */
    void Physics::ReflectVelocity(ECS::Components::LinearVelocity2D& vel, const Vector2D& normal) {
        const float normalVelocity = Dot(vel.Value, normal);
        if (normalVelocity < 0.0f) {
            vel.Value -= normal * normalVelocity;
        }
    }

    /**
     * @brief Zero selected signed velocity component.
     * @param vel Velocity to modify.
     * @param isXAxis True to target X, false to target Y.
     * @param isPositive True to zero positive component, false for negative component.
     */
    void Physics::ZeroVelocityComponent(ECS::Components::LinearVelocity2D& vel, const bool isXAxis, const bool isPositive) {
        if (isXAxis) {
            if ((isPositive && vel.Value.X > 0.0f) || (!isPositive && vel.Value.X < 0.0f)) {
                vel.Value.X = 0.0f;
            }
        }
        else {
            if ((isPositive && vel.Value.Y > 0.0f) || (!isPositive && vel.Value.Y < 0.0f)) {
                vel.Value.Y = 0.0f;
            }
        }
    }

    // ============================================================================
    // Angular Damping
    // ============================================================================

    /**
     * @brief Compute angular acceleration from angular damping.
     * @param rb Rigidbody parameters.
     * @param angVel Current angular velocity.
     * @return Angular acceleration.
     */
    float Physics::CalculateAngularAcceleration(const ECS::Components::Rigidbody2D& rb, const ECS::Components::AngularVelocity2D& angVel) {
        return -angVel.Value * rb.AngularDamping;
    }

    // ============================================================================
    // Boundary Collision
    // ============================================================================

    /**
     * @brief Resolve a circle-like boundary collision against world bounds.
     * @param position Body position.
     * @param velocity Body velocity.
     * @param radius Body collision radius.
     * @param bounds Boundary constraints.
     * @param entityRestitution Optional per-entity restitution override.
     * @return True when any boundary collision occurred.
     */
    bool Physics::ApplyBoundaryConstraint(
        Vector2D& position,
        Vector2D& velocity,
        const float radius,
        const BoundaryConstraint& bounds,
        const float entityRestitution
    ) {
        bool collided = false;

        // Use entity restitution if provided, otherwise use bounds restitution
        const float restitution = (entityRestitution >= 0.0f) ? entityRestitution : bounds.Restitution;

        // X-axis bounds
        if (position.X - radius <= bounds.MinX) {
            position.X = bounds.MinX + radius;
            if (bounds.KillVelocity) {
                velocity.X = 0.0f;
            }
            else if (velocity.X < 0.0f) {
                // Bounce with restitution
                velocity.X = -velocity.X * restitution;
            }
            collided = true;
        }
        else if (position.X + radius >= bounds.MaxX) {
            position.X = bounds.MaxX - radius;
            if (bounds.KillVelocity) {
                velocity.X = 0.0f;
            }
            else if (velocity.X > 0.0f) {
                // Bounce with restitution
                velocity.X = -velocity.X * restitution;
            }
            collided = true;
        }

        // Y-axis bounds
        if (position.Y - radius <= bounds.MinY) {
            position.Y = bounds.MinY + radius;
            if (bounds.KillVelocity) {
                velocity.Y = 0.0f;
            }
            else if (velocity.Y < 0.0f) {
                // Bounce with restitution
                velocity.Y = -velocity.Y * restitution;
            }
            collided = true;
        }
        else if (position.Y + radius >= bounds.MaxY) {
            position.Y = bounds.MaxY - radius;
            if (bounds.KillVelocity) {
                velocity.Y = 0.0f;
            }
            else if (velocity.Y > 0.0f) {
                // Bounce with restitution
                velocity.Y = -velocity.Y * restitution;
            }
            collided = true;
        }

        return collided;
    }

    // ============================================================================
    // Generic resolve Collision
    // ============================================================================
    //Physics::CollisionResult Physics::ResolveCollision(
    //    const ECS::Components::Rigidbody2D& rbA,
    //    const ECS::Components::Rigidbody2D& rbB,
    //    ECS::Components::LinearVelocity2D& velA,
    //    ECS::Components::LinearVelocity2D& velB,
    //    ECS::Components::LocalTransform& transformA,
    //    ECS::Components::LocalTransform& transformB,
    //    const Vector2D& normal,
    //    const float depth,
    //    const ECS::Components::PhysicsMaterial2D& physics
    //) {

 
    //    // Create a result structure to store collision outcome
    //    CollisionResult result{};
    //    result.Collided = true;          // Mark collision as valid
    //    result.Normal = normal;          // Store collision normal (direction of impact)
    //    result.Depth = depth;            // Store penetration depth

    //    // Compute inverse masses (1/mass); static objects will have 0
    //    const float invMassA = GetInverseMass(rbA.Mass);
    //    const float invMassB = GetInverseMass(rbB.Mass);
    //    const float invMassSum = invMassA + invMassB;

    //    // If both objects are static (no inverse mass), no need to resolve
    //    if (invMassSum == 0.0f) return result;

    //    // Compute relative velocity (B relative to A)
    //    const Vector2D relativeVelocity = velB.Value - velA.Value;

    //    // Project relative velocity onto collision normal to get speed along collision axis
    //    const float normalVelocity = Dot(relativeVelocity, normal);
    //    result.RelativeNormalVelocity = normalVelocity;

    //    // ========================================================================
    //    // IMPULSE RESOLUTION (velocity change due to collision impact)
    //    // ========================================================================

    //    // Only resolve if objects are moving toward each other (negative normal velocity)
    //    if (normalVelocity < 0.0f) {
    //        // Clamp restitution (bounciness) between 0 and 1
    //        const float restitution = std::clamp(physics.Restitution, 0.0f, 1.0f);

    //        // Calculate impulse scalar using standard impulse equation:
    //        // j = -(1 + e) * (Vr . n) / (1/mA + 1/mB)
    //        const float j = -(1.0f + restitution) * normalVelocity / invMassSum;

    //        // Impulse vector in direction of collision normal
    //        const Vector2D impulse = normal * j;

    //        // Apply equal and opposite impulses based on inverse mass
    //        velA.Value -= impulse * invMassA;
    //        velB.Value += impulse * invMassB;

    //        // ====================================================================
    //        // FRICTION RESOLUTION (velocity changes tangential to the contact)
    //        // ====================================================================
    //        if (physics.Friction > 0.0f) {
    //            // Recalculate new relative velocity after normal impulse
    //            const Vector2D newRelativeVelocity = velB.Value - velA.Value;
    //            const float newNormalVelocity = Dot(newRelativeVelocity, normal);

    //            // Compute tangent vector (direction perpendicular to collision normal)
    //            Vector2D tangent = newRelativeVelocity - normal * newNormalVelocity;

    //            // Compute squared tangent length to avoid divide-by-zero
    //            const float tangentLengthSquared = Dot(tangent, tangent);
    //            if (tangentLengthSquared > MIN_TANGENT_LENGTH_SQUARED) {
    //                // Normalize tangent vector
    //                tangent = tangent / std::sqrt(tangentLengthSquared);

    //                // Compute friction impulse magnitude:
    //                // jt = -(Vr . t) / (1/mA + 1/mB)
    //                const float jt = -Dot(newRelativeVelocity, tangent) / invMassSum;

    //                // Clamp friction impulse to the Coulomb friction cone
    //                const float frictionImpulse = std::clamp(
    //                    jt, -j * physics.Friction, j * physics.Friction
    //                );

    //                // Compute final friction vector
    //                const Vector2D frictionVector = tangent * frictionImpulse;

    //                // Apply tangential (friction) impulses to both bodies
    //                velA.Value -= frictionVector * invMassA;
    //                velB.Value += frictionVector * invMassB;
    //            }
    //        }
    //    }

    //    // ========================================================================
    //    // POSITION CORRECTION (push objects apart)
    //    // IMPROVED FOR THIN RECTANGLES
    //    // ========================================================================
    //    {
    //        // Use smaller slop for thin objects to prevent tunneling
    //        const float slop = 0.001f;  // Increased from 0.01f

    //        // Use higher percentage for aggressive correction
    //        const float percent = std::min(physics.PositionCorrectPercent, 0.95f);

    //        // Calculate base correction
    //        float correctionMagnitude = std::max(depth - slop, 0.0f) * percent;

    //        // This prevents balls from getting stuck in thin walls
    //        if (depth > slop * 2.0f) {
    //            // Deep penetration - likely tunneled through thin wall
    //            // Use even more aggressive correction
    //            const float deepPenetrationBoost = 1.5f;
    //            correctionMagnitude = std::max(depth - slop, 0.0f) * percent * deepPenetrationBoost;
    //        }

    //        const Vector2D correction = normal * (correctionMagnitude / invMassSum);

    //        // Apply position correction
    //        transformA.Position.X -= correction.X * invMassA;
    //        transformA.Position.Y -= correction.Y * invMassA;
    //        transformB.Position.X += correction.X * invMassB;
    //        transformB.Position.Y += correction.Y * invMassB;

    //        // Kill velocity toward wall for deeply penetrated objects
    //        if (depth > slop * 3.0f) {
    //            // Check velocity toward collision normal
    //            const float vNormalA = Dot(velA.Value, normal);
    //            const float vNormalB = Dot(velB.Value, normal);

    //            // If object A is moving toward B (negative normal velocity), dampen it
    //            if (vNormalA < 0.0f && invMassA > 0.0f) {
    //                velA.Value -= normal * vNormalA * 0.5f;  // Remove 50% of inward velocity
    //            }

    //            // If object B is moving toward A (positive normal velocity), dampen it
    //            if (vNormalB > 0.0f && invMassB > 0.0f) {
    //                velB.Value -= normal * vNormalB * 0.5f;  // Remove 50% of inward velocity
    //            }
    //        }
    //    }

    //    return result;
    //}



    /**
     * @brief Resolve collision impulse and position correction for a contact manifold.
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
    Physics::CollisionResult Physics::ResolveCollisionManifold(
        const ECS::Components::Rigidbody2D& rbA,
        const ECS::Components::Rigidbody2D& rbB,
        ECS::Components::LinearVelocity2D& velA,
        ECS::Components::LinearVelocity2D& velB,
        ECS::Components::LocalTransform& transformA,
        ECS::Components::LocalTransform& transformB,
        const Collision::ContactManifold& manifold,
        const ECS::Components::PhysicsMaterial2D& physics)
    {
        CollisionResult result{};
        result.Collided = true;
        result.Normal = manifold.normal;
        result.Depth = manifold.penetration;

        const float invMassA = GetInverseMass(rbA.Mass);
        const float invMassB = GetInverseMass(rbB.Mass);
        const float invMassSum = invMassA + invMassB;

        if (invMassSum == 0.0f) return result;

        // ========================================================================
        // RESOLVE EACH CONTACT POINT
        // ========================================================================

        for (int i = 0; i < manifold.pointCount; ++i) {
            // const Vector2D& contactPoint = manifold.points[i];

            // Calculate relative velocity at this specific contact point
            // For now, we're not handling rotation, so velocity is same everywhere
            // (You'll need to add angular velocity later for full physics)
            const Vector2D relativeVelocity = velB.Value - velA.Value;
            const float normalVelocity = Dot(relativeVelocity, manifold.normal);

            // Only resolve if moving toward each other
            if (normalVelocity < 0.0f) {
                // Restitution (bounciness)
                const float restitution = std::clamp(physics.Restitution, 0.0f, 1.0f);

                // Calculate impulse scalar
                // j = -(1 + e) * vrel·n / (1/mA + 1/mB)
                // Divide by pointCount to distribute impulse across contacts
                const float j = -(1.0f + restitution) * normalVelocity / (invMassSum * manifold.pointCount);

                // Apply normal impulse
                const Vector2D impulse = manifold.normal * j;
                velA.Value -= impulse * invMassA;
                velB.Value += impulse * invMassB;

                // ================================================================
                // FRICTION
                // ================================================================

                if (physics.Friction > 0.0f) {
                    // Recalculate relative velocity after normal impulse
                    const Vector2D newRelativeVelocity = velB.Value - velA.Value;
                    const float newNormalVelocity = Dot(newRelativeVelocity, manifold.normal);

                    // Tangent direction (perpendicular to normal)
                    Vector2D tangent = newRelativeVelocity - manifold.normal * newNormalVelocity;
                    const float tangentLengthSq = Dot(tangent, tangent);

                    if (tangentLengthSq > MIN_TANGENT_LENGTH_SQUARED) {
                        tangent = tangent / std::sqrt(tangentLengthSq);

                        // Friction impulse
                        const float jt = -Dot(newRelativeVelocity, tangent) / (invMassSum * manifold.pointCount);

                        // Clamp to friction cone (Coulomb friction)
                        const float maxFriction = physics.Friction * std::abs(j);
                        const float frictionImpulse = std::clamp(jt, -maxFriction, maxFriction);

                        const Vector2D frictionVector = tangent * frictionImpulse;
                        velA.Value -= frictionVector * invMassA;
                        velB.Value += frictionVector * invMassB;
                    }
                }
            }
        }

        // ========================================================================
        // POSITION CORRECTION (applied once per manifold, not per contact)
        // ========================================================================

        const float slop = 0.0001f;
        const float percent = std::clamp(physics.PositionCorrectPercent, 0.0f, 1.0f);

        const float correctionMagnitude = std::max(manifold.penetration - slop, 0.0f) * percent;
        const Vector2D correction = manifold.normal * (correctionMagnitude / invMassSum);

        transformA.Position.X -= correction.X * invMassA;
        transformA.Position.Y -= correction.Y * invMassA;
        transformB.Position.X += correction.X * invMassB;
        transformB.Position.Y += correction.Y * invMassB;

        return result;
    }

}
