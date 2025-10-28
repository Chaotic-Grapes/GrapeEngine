#include "physics/Physics.h"
#include <cmath>
#include <algorithm>
#include "helpers/MathUtils.h"

namespace Engine {
    Vector2D Physics::m_gravity = Vector2D(0.0f, -981.f);
    bool Physics::m_enabled = true;
    bool Physics::m_worldBoundsEnabled = false;
    Physics::BoundaryConstraint Physics::m_worldBounds = { 0.0f, 1600.0f, 0.0f, 900.0f, false, 0.8f };

    Vector2D Physics::CalculateAcceleration(const ECS::Components::Rigidbody2D& rb, const ECS::Components::LinearVelocity2D& vel) {
        Vector2D acceleration(0.f, 0.f);
        //acceleration += m_gravity * rb.GravityScale;
        acceleration += Vector2D(-vel.Value.X * rb.LinearDamping, -vel.Value.Y * rb.LinearDamping);// / rb.Mass;
        return acceleration;
    }

    void Physics::ApplyForce(const ECS::Components::Rigidbody2D& rb, ECS::Components::LinearVelocity2D& vel, const Vector2D& force) {
        if (rb.Mass > 0)
            vel.Value += force / rb.Mass;
    }

    void Physics::ApplyImpulse(const ECS::Components::Rigidbody2D& rb, ECS::Components::LinearVelocity2D& vel, const Vector2D& impulse) {
        if (rb.Mass > 0)
            vel.Value += impulse / rb.Mass;
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    void Physics::SetWorldBounds(const float minX, const float maxX, const float minY, const float maxY, const bool killVelocity, const float restitution) {
        m_worldBounds.MinX = minX;
        m_worldBounds.MaxX = maxX;
        m_worldBounds.MinY = minY;
        m_worldBounds.MaxY = maxY;
        m_worldBounds.KillVelocity = killVelocity;
        m_worldBounds.Restitution = std::clamp(restitution, 0.0f, 1.0f);
        m_worldBoundsEnabled = true;
    }

    float Physics::GetInverseMass(const float mass) {
        return (mass > 0.0f) ? 1.0f / mass : 0.0f;
    }

    float Physics::Dot(const Vector2D& a, const Vector2D& b) {
        return a.X * b.X + a.Y * b.Y;
    }

    // ============================================================================
    // Velocity Manipulation
    // ============================================================================

    void Physics::ApplyVelocityDamping(ECS::Components::LinearVelocity2D& vel, const float dampingFactor) {
        vel.Value *= dampingFactor;
    }

    void Physics::ReflectVelocity(ECS::Components::LinearVelocity2D& vel, const Vector2D& normal) {
        const float normalVelocity = Dot(vel.Value, normal);
        if (normalVelocity < 0.0f) {
            vel.Value -= normal * normalVelocity;
        }
    }

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
    // Boundary Collision
    // ============================================================================

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

    Physics::CollisionResult Physics::ResolveCollision(
        const ECS::Components::Rigidbody2D& rbA, // components for RB
        const ECS::Components::Rigidbody2D& rbB,
        ECS::Components::LinearVelocity2D& velA,// components for linear vel
        ECS::Components::LinearVelocity2D& velB,
        ECS::Components::LocalTransform& transformA, // components for transform
        ECS::Components::LocalTransform& transformB,
        const Vector2D& normal, // collision normal B -> A
        const float depth, // penetration depth
        const ECS::Components::PhysicsMaterial2D& physics // physicsmaterial
    ) {
        // init collision result
        CollisionResult result{};
        result.Collided = true;
        result.Normal = normal;
        result.Depth = depth;

        //calculate inverse masses 
        const float invMassA = GetInverseMass(rbA.Mass);
        const float invMassB = GetInverseMass(rbB.Mass);
        const float invMassSum = invMassA + invMassB;

        // if mass 0 basically static, do nothing
        if (invMassSum == 0.0f) return result;

        // Relative velocity along normal 
        const Vector2D relativeVelocity = velB.Value - velA.Value;

        //get normal velocity by dot producting (relativevel and normal)
        const float normalVelocity = Dot(relativeVelocity, normal);

        //store normal velocity to return later
        result.RelativeNormalVelocity = normalVelocity;

        // if bodies normal velocity < 0 means moving towards each other 
        if (normalVelocity < 0.0f) {

            // clamp restituion between 0 to 1
            const float restitution = std::clamp(physics.Restitution, 0.0f, 1.0f);
            // restitution coefficient calculation where the derived forumla comes from
            // to become J = -(1* e) * (v_relative * N) / inverse mass sum
            const float j = -(1.0f + restitution) * normalVelocity / invMassSum;

            //impulse is normalised value of j
            const Vector2D impulse = normal * j;

            // edit values by reference for velA and B
            velA.Value -= impulse * invMassA;
            velB.Value += impulse * invMassB;

            // apply friction if physics implemented has preset it above 0
            // friction applied after normal impulse is applied using updated velocities
            // stimulate sliding forces
            if (physics.Friction > 0.0f) {

                // recalculate relative velocity after normal impulse applied
                // nevessary because normal impulse changed the velocities
                const Vector2D newRelativeVelocity = velB.Value - velA.Value;
                const float newNormalVelocity = Dot(newRelativeVelocity, normal);

                // calculate the tangent vector (perpendicular to normal, along the contact surface)
                // tangent = relative vel - (relative vel . normmal) * normal
                Vector2D tangent = newRelativeVelocity - normal * newNormalVelocity;

                // this checks if there is significant tangential motion to apply friction
                const float tangentLengthSquared = Dot(tangent, tangent);
                if (tangentLengthSquared > MIN_TANGENT_LENGTH_SQUARED) {

                    // normalize tangent vector to get friction direction 
                    tangent = tangent / std::sqrt(tangentLengthSquared);

                    //calculate friction impulse magnitude along tangent
                    // calculation = -(relative vel dot product with tanget) / invMassSum
                    // negative sign ensures friction opposes the relative motion direction
                    const float jt = -Dot(newRelativeVelocity, tangent) / invMassSum;

                    // Apply Coulomb's friction law: friction is limited by the normal force
                    const float frictionImpulse = std::clamp(jt, -j * physics.Friction, j * physics.Friction);

                    // Convert the scalar friction impulse to a vector along the tangent
                    const Vector2D frictionVector = tangent * frictionImpulse;

                    // apply friction impulse to both bodies equal and opposite
                    // object a moves against friction direction therefore -
                    velA.Value -= frictionVector * invMassA;

                    //object b moves with friction direction therefore +
                    velB.Value += frictionVector * invMassB;
                }
            }
        }

        // positional correction derived from baumgarte stabilization
        // after applying impulses objects may still be slightly interpenetrating 
        // due to numerical errors or external force. so this adjusts
        // position to separate overlapping objects prevents sinking objects to each other
        {
            // percentage of penetration to correct per frame (typically 0.2ish)
            // lower value = softer correction compared to higher value
            const float percent = physics.PositionCorrectPercent; 

            // a small penetration tolerance to prevent jittering
            // basically tolerance of penetration
            const float slop = 0.01f; 

            //calculate how much correction required
            // only correct penetrations deeper than slop threshold
            // percent value allows gradual correction over multiple frames
            const float correctionMagnitude = std::max(depth - slop, 0.0f) * percent;

            //now get correction value vector along collision to be set to the transform
            //positions of both objects 
            const Vector2D correction = normal * (correctionMagnitude / invMassSum);

            // apply position correction to both objects (equal and opposite)
            // object A moves backward (out of B) along the normal
            transformA.Position.X -= correction.X * invMassA;
            transformA.Position.Y -= correction.Y * invMassA;

            // object B moves forward (out of A) along the normal
            transformB.Position.X += correction.X * invMassB;
            transformB.Position.Y += correction.Y * invMassB;
        }

        return result;
    }
}


    

