#include "physics/Physics.h"
#include <cmath>
#include <algorithm>
#include "helpers/MathUtils.h"

namespace Engine {
    Vector2D Physics::m_gravity = Vector2D(0.0f, -981.f);
    bool Physics::m_enabled = true;
    bool Physics::m_worldBoundsEnabled = false;
    Physics::BoundaryConstraint Physics::m_worldBounds = {0.0f, 1600.0f, 0.0f, 900.0f, false, 0.8f};

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
        } else {
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
            } else if (velocity.X < 0.0f) {
                // Bounce with restitution
                velocity.X = -velocity.X * restitution;
            }
            collided = true;
        } else if (position.X + radius >= bounds.MaxX) {
            position.X = bounds.MaxX - radius;
            if (bounds.KillVelocity) {
                velocity.X = 0.0f;
            } else if (velocity.X > 0.0f) {
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
            } else if (velocity.Y < 0.0f) {
                // Bounce with restitution
                velocity.Y = -velocity.Y * restitution;
            }
            collided = true;
        } else if (position.Y + radius >= bounds.MaxY) {
            position.Y = bounds.MaxY - radius;
            if (bounds.KillVelocity) {
                velocity.Y = 0.0f;
            } else if (velocity.Y > 0.0f) {
                // Bounce with restitution
                velocity.Y = -velocity.Y * restitution;
            }
            collided = true;
        }

        return collided;
    }

    // ============================================================================
    // Circle-AABB Collision Resolution
    // ============================================================================

    Physics::CircleAABBResult Physics::ResolveCircleAABBCollision(
        ECS::Components::LocalTransform& circleTransform,
        ECS::Components::LinearVelocity2D& circleVelocity,
        const Vector2D& boxMin,
        const Vector2D& boxMax,
        const float circleRadius,
        const float epsilon
    ) {
        CircleAABBResult result{};
        result.Collided = false;

        const Vector2D closestPoint(
            std::clamp(circleTransform.Position.X, boxMin.X, boxMax.X),
            std::clamp(circleTransform.Position.Y, boxMin.Y, boxMax.Y)
        );

        const Vector2D difference = MathUtils::ToVector2D(circleTransform.Position) - closestPoint;
        const float distanceSquared = Dot(difference, difference);

        if (distanceSquared >= circleRadius * circleRadius) {
            return result;
        }

        Vector2D normal;
        float penetration;

        if (distanceSquared > MIN_DISTANCE_SQUARED) {
            const float distance = std::sqrt(distanceSquared);
            normal = difference / distance;
            penetration = circleRadius - distance;
        } else {
            // Circle center inside box - push along smallest axis
            const float distances[] = {
                circleTransform.Position.X - boxMin.X,  // left
                boxMax.X - circleTransform.Position.X,  // right
                circleTransform.Position.Y - boxMin.Y,  // down
                boxMax.Y - circleTransform.Position.Y   // up
            };

            const auto minIt = std::min_element(std::begin(distances), std::end(distances));
            const size_t minIndex = std::distance(std::begin(distances), minIt);

            const Vector2D normals[] = {
                {-1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 1.0f}
            };

            normal = normals[minIndex];
            penetration = *minIt;
        }

        // Apply position correction
        circleTransform.Position += MathUtils::ToVector3D(normal * (penetration + epsilon));

        // Reflect velocity if moving into the collision
        ReflectVelocity(circleVelocity, normal);

        result.Collided = true;
        result.PenetrationNormal = normal;
        result.Penetration = penetration;

        return result;
    }

    // ============================================================================
    // Circle-Circle Collision Resolution
    // ============================================================================

    Physics::CircleCollisionResult Physics::ResolveCircleCircleCollision(
        const ECS::Components::Rigidbody2D& rbA,
        const ECS::Components::Rigidbody2D& rbB,
        ECS::Components::LinearVelocity2D& velA,
        ECS::Components::LinearVelocity2D& velB,
        ECS::Components::LocalTransform& transformA,
        ECS::Components::LocalTransform& transformB,
        const float radiusA,
        const float radiusB,
        const Vector2D& offsetA,
        const Vector2D& offsetB,
        const ECS::Components::PhysicsMaterial2D& physics
    ) {
        CircleCollisionResult result{};
        result.Collided = false;

        // Calculate actual circle centers with offsets
        const Vector2D centerA = MathUtils::ToVector2D(transformA.Position) + offsetA;
        const Vector2D centerB = MathUtils::ToVector2D(transformB.Position) + offsetB;

        // Calculate collision normal and depth
        const Vector2D delta = centerB - centerA;
        const float distanceSquared = Dot(delta, delta);
        const float radiusSum = radiusA + radiusB;

        if (distanceSquared >= radiusSum * radiusSum || distanceSquared < MIN_DISTANCE_SQUARED) {
            return result;
        }

        const float distance = std::sqrt(distanceSquared);
        const Vector2D normal = delta / distance;
        const float depth = radiusSum - distance;

        // Calculate relative velocity
        const Vector2D relativeVelocity = velB.Value - velA.Value;
        const float normalVelocity = Dot(relativeVelocity, normal);

        // Skip if objects are separating
        if (normalVelocity > 0.0f) {
            return result;
        }

        result.Collided = true;
        result.Normal = normal;
        result.Depth = depth;
        result.RelativeNormalVelocity = normalVelocity;

        // Calculate inverse masses
        const float invMassA = GetInverseMass(rbA.Mass);
        const float invMassB = GetInverseMass(rbB.Mass);
        const float invMassSum = invMassA + invMassB;

        if (invMassSum == 0.0f) {
            return result;
        }

        // Apply restitution impulse
        const float restitution = std::clamp(physics.Restitution, 0.0f, 1.0f);
        const float j = -(1.0f + restitution) * normalVelocity / invMassSum;
        const Vector2D impulse = normal * j;

        velA.Value -= impulse * invMassA;
        velB.Value += impulse * invMassB;

        // Apply friction
        if (physics.Friction > 0.0f) {
            const Vector2D newRelativeVelocity = velB.Value - velA.Value;
            const float newNormalVelocity = Dot(newRelativeVelocity, normal);
            Vector2D tangent = newRelativeVelocity - normal * newNormalVelocity;
            const float tangentLengthSquared = Dot(tangent, tangent);

            if (tangentLengthSquared > MIN_TANGENT_LENGTH_SQUARED) {
                tangent = tangent / std::sqrt(tangentLengthSquared);
                const float jt = -Dot(newRelativeVelocity, tangent) / invMassSum;
                const float frictionImpulse = std::clamp(jt, -j * physics.Friction, j * physics.Friction);
                const Vector2D frictionVector = tangent * frictionImpulse;

                velA.Value -= frictionVector * invMassA;
                velB.Value += frictionVector * invMassB;
            }
        }

        // Position correction
        const Vector2D correction = normal * (depth * physics.PositionCorrectPercent / invMassSum);
        transformA.Position.X -= correction.X * invMassA;
        transformA.Position.Y -= correction.Y * invMassA;
        transformB.Position.X += correction.X * invMassB;
        transformB.Position.Y += correction.Y * invMassB;

        return result;
    }
}
