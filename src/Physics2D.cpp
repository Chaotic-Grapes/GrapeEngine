#include "Physics2D.h"
#include "systems/Time.h"
#include <iostream>
#include "Collision.h"
#include "ecs/Entity.h"

namespace Engine {
    Vector2D Physics2D::m_gravity = Vector2D(0.0f, -9.81f);
    bool Physics2D::m_enabled = true;

    void Physics2D::OnCreate() {
        std::cout << "Physics2D Initialized" << '\n';
    }

    void Physics2D::OnUpdate() {
        if (!m_enabled) return;

		// Iterate over all entities with Rigidbody2D + Transform + Collider2D components
        const auto entities = m_world->GetEntityManager().Query<Component::Rigidbody2D, Component::Transform>();
        for (auto& [rb, transform] : entities) {
            // There's STATIC, KINEMATIC, and DYNAMIC body types
            // Skip static bodies
            if (rb.BodyType == Component::Rigidbody2D::Static)
                return;

            Vector2D intendedPos = transform.Position + rb.LinearVelocity * Time::FixedDeltaTime();

            // Apply gravity and drag
            Vector2D acceleration(0.f, 0.f);
            acceleration += m_gravity * rb.GravityScale;
            acceleration += Vector2D(-rb.LinearVelocity.X * rb.LinearDamping, -rb.LinearVelocity.Y * rb.LinearDamping) / rb.Mass;

            // TODO: Add a PolygonCollider2D for line segments and complex shapes
            // DO NOT ADD "LineCollider2D" as it is basically PolygonCollider2D with 2 points
            // Using "LineRenderer" component for boundary lines for now
            // Collision: Circle vs boundary lines
            const auto colliderEntities = m_world->GetEntityManager().Query<Component::Rigidbody2D, Component::Transform, Component::Collider2D>();

            for (auto& [crb, ctransform, collider] : colliderEntities) {
                if (const auto* circle = dynamic_cast<Component::CircleCollider2D*>(&collider)) {
                    auto lines = m_world->GetEntityManager().Query<Component::LineRenderer>();
                    for (auto& [lineEntity] : lines) {
                        Collision::LineSegment seg = Collision::MakeSegment(lineEntity.Start, lineEntity.End);
                        Vector2D contact, normal;
                        float tHit;

                        Collision::Circle c{ ctransform.Position, circle->Radius };
                        if (Collision::CircleVsSegmentSweep(c, intendedPos, seg, contact, normal, tHit)) {
                            Vector2D reflectedDir;
                            Collision::CircleSegmentResponse(contact, normal, intendedPos, reflectedDir);
                            crb.LinearVelocity = reflectedDir * crb.LinearVelocity.Length(); // preserve speed along new direction
                        }
                    }
                }
            }

            // TODO: handle other collisions here

            // Integrate acceleration
            rb.LinearVelocity += acceleration * Time::FixedDeltaTime();
            transform.Position = intendedPos;

            if (!rb.FreezeRotation)
                transform.Rotation += rb.AngularVelocity * Time::FixedDeltaTime();
        }
    }     
    

    // Static force/impulse
    void Physics2D::AddForce(Component::Rigidbody2D& rb, const Vector2D& force) {
        if (rb.Mass > 0)
            rb.LinearVelocity += force / rb.Mass;
    }

    void Physics2D::AddImpulse(Component::Rigidbody2D& rb, const Vector2D& impulse) {
        if (rb.Mass > 0)
            rb.LinearVelocity += impulse / rb.Mass;
    }
}
