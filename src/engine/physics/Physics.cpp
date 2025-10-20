#include "physics/Physics.h"
#include <cmath>
#include <algorithm>

namespace Engine {
    Vector2D Physics::m_gravity = Vector2D(0.0f, -9.81f);
    bool Physics::m_enabled = true;

    Vector2D Physics::CalculateAcceleration(const ECS::Components::Rigidbody2D& rb) {
        Vector2D acceleration(0.f, 0.f);
        acceleration += m_gravity * rb.GravityScale;
        acceleration += Vector2D(-rb.LinearVelocity.X * rb.LinearDamping, -rb.LinearVelocity.Y * rb.LinearDamping) / rb.Mass;
        return acceleration;
    }

    void Physics::ApplyForce(ECS::Components::Rigidbody2D& rb, const Vector2D& force) {
        if (rb.Mass > 0)
            rb.LinearVelocity += force / rb.Mass;
    }

    void Physics::ApplyImpulse(ECS::Components::Rigidbody2D& rb, const Vector2D& impulse) {
        if (rb.Mass > 0)
            rb.LinearVelocity += impulse / rb.Mass;
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    float Physics::GetInverseMass(float mass) {
        return (mass > 0.0f) ? 1.0f / mass : 0.0f;
    }

    float Physics::Dot(const Vector2D& a, const Vector2D& b) {
        return a.X * b.X + a.Y * b.Y;
    }

    // ============================================================================
    // Velocity Manipulation
    // ============================================================================

    void Physics::ApplyVelocityDamping(ECS::Components::Rigidbody2D& rb, float dampingFactor) {
        rb.LinearVelocity.X *= dampingFactor;
        rb.LinearVelocity.Y *= dampingFactor;
    }

    void Physics::ReflectVelocity(Vector2D& velocity, const Vector2D& normal) {
        const float normalVelocity = Dot(velocity, normal);
        if (normalVelocity < 0.0f) {
            velocity -= normal * normalVelocity;
        }
    }

    void Physics::ZeroVelocityComponent(Vector2D& velocity, bool isXAxis, bool isPositive) {
        if (isXAxis) {
            if ((isPositive && velocity.X > 0.0f) || (!isPositive && velocity.X < 0.0f)) {
                velocity.X = 0.0f;
            }
        } else {
            if ((isPositive && velocity.Y > 0.0f) || (!isPositive && velocity.Y < 0.0f)) {
                velocity.Y = 0.0f;
            }
        }
    }

    // ============================================================================
    // Boundary Collision
    // ============================================================================

    bool Physics::ApplyBoundaryConstraint(
        Vector2D& position,
        Vector2D& velocity,
        float radius,
        const BoundaryConstraint& bounds
    ) {
        bool collided = false;

        // X-axis bounds
        if (position.X - radius <= bounds.minX) {
            position.X = bounds.minX + radius;
            if (bounds.killVelocity && velocity.X < 0.0f) {
                velocity.X = 0.0f;
            }
            collided = true;
        } else if (position.X + radius >= bounds.maxX) {
            position.X = bounds.maxX - radius;
            if (bounds.killVelocity && velocity.X > 0.0f) {
                velocity.X = 0.0f;
            }
            collided = true;
        }

        // Y-axis bounds
        if (position.Y - radius <= bounds.minY) {
            position.Y = bounds.minY + radius;
            if (bounds.killVelocity && velocity.Y < 0.0f) {
                velocity.Y = 0.0f;
            }
            collided = true;
        } else if (position.Y + radius >= bounds.maxY) {
            position.Y = bounds.maxY - radius;
            if (bounds.killVelocity && velocity.Y > 0.0f) {
                velocity.Y = 0.0f;
            }
            collided = true;
        }

        return collided;
    }

    // ============================================================================
    // Circle-AABB Collision Resolution
    // ============================================================================

    Physics::CircleAABBResult Physics::ResolveCircleAABBCollision(
        Vector2D& circlePosition,
        Vector2D& circleVelocity,
        const Vector2D& boxMin,
        const Vector2D& boxMax,
        float circleRadius,
        float epsilon
    ) {
        CircleAABBResult result{};
        result.collided = false;

        const Vector2D closestPoint(
            std::clamp(circlePosition.X, boxMin.X, boxMax.X),
            std::clamp(circlePosition.Y, boxMin.Y, boxMax.Y)
        );

        Vector2D difference = circlePosition - closestPoint;
        float distanceSquared = Dot(difference, difference);

        if (distanceSquared >= circleRadius * circleRadius) {
            return result;
        }

        Vector2D normal;
        float penetration;

        if (distanceSquared > MIN_DISTANCE_SQUARED) {
            float distance = std::sqrt(distanceSquared);
            normal = difference / distance;
            penetration = circleRadius - distance;
        } else {
            // Circle center inside box - push along smallest axis
            const float distances[] = {
                circlePosition.X - boxMin.X,  // left
                boxMax.X - circlePosition.X,  // right
                circlePosition.Y - boxMin.Y,  // down
                boxMax.Y - circlePosition.Y   // up
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
        circlePosition += normal * (penetration + epsilon);

        // Reflect velocity if moving into the collision
        ReflectVelocity(circleVelocity, normal);

        result.collided = true;
        result.penetrationNormal = normal;
        result.penetration = penetration;

        return result;
    }

    // ============================================================================
    // Circle-Circle Collision Resolution
    // ============================================================================

    Physics::CircleCollisionResult Physics::ResolveCircleCircleCollision(
        ECS::Components::Rigidbody2D& rbA,
        ECS::Components::Rigidbody2D& rbB,
        Vector2D& positionA,
        Vector2D& positionB,
        float radiusA,
        float radiusB,
        const CollisionParams& params
    ) {
        CircleCollisionResult result{};
        result.collided = false;

        // Calculate collision normal and depth
        Vector2D delta = positionB - positionA;
        float distanceSquared = Dot(delta, delta);
        float radiusSum = radiusA + radiusB;

        if (distanceSquared >= radiusSum * radiusSum || distanceSquared < MIN_DISTANCE_SQUARED) {
            return result;
        }

        float distance = std::sqrt(distanceSquared);
        Vector2D normal = delta / distance;
        float depth = radiusSum - distance;

        // Calculate relative velocity
        Vector2D relativeVelocity = rbB.LinearVelocity - rbA.LinearVelocity;
        float normalVelocity = Dot(relativeVelocity, normal);

        // Skip if objects are separating
        if (normalVelocity > 0.0f) {
            return result;
        }

        result.collided = true;
        result.normal = normal;
        result.depth = depth;
        result.relativeNormalVelocity = normalVelocity;

        // Calculate inverse masses
        const float invMassA = GetInverseMass(rbA.Mass);
        const float invMassB = GetInverseMass(rbB.Mass);
        const float invMassSum = invMassA + invMassB;

        if (invMassSum == 0.0f) {
            return result;
        }

        // Apply restitution impulse
        const float restitution = std::clamp(params.restitution, 0.0f, 1.0f);
        const float j = -(1.0f + restitution) * normalVelocity / invMassSum;
        const Vector2D impulse = normal * j;

        rbA.LinearVelocity -= impulse * invMassA;
        rbB.LinearVelocity += impulse * invMassB;

        // Apply friction
        if (params.friction > 0.0f) {
            const Vector2D newRelativeVelocity = rbB.LinearVelocity - rbA.LinearVelocity;
            const float newNormalVelocity = Dot(newRelativeVelocity, normal);
            Vector2D tangent = newRelativeVelocity - normal * newNormalVelocity;
            const float tangentLengthSquared = Dot(tangent, tangent);

            if (tangentLengthSquared > MIN_TANGENT_LENGTH_SQUARED) {
                tangent = tangent / std::sqrt(tangentLengthSquared);
                const float jt = -Dot(newRelativeVelocity, tangent) / invMassSum;
                const float frictionImpulse = std::clamp(jt, -j * params.friction, j * params.friction);
                const Vector2D frictionVector = tangent * frictionImpulse;

                rbA.LinearVelocity -= frictionVector * invMassA;
                rbB.LinearVelocity += frictionVector * invMassB;
            }
        }

        // Position correction
        const Vector2D correction = normal * (depth * params.positionCorrectionPercent / invMassSum);
        positionA.X -= correction.X * invMassA;
        positionA.Y -= correction.Y * invMassA;
        positionB.X += correction.X * invMassB;
        positionB.Y += correction.Y * invMassB;

        return result;
    }
}