#include "Physics2D.h"
#include "systems/Time.h"
#include <iostream>
#include "Collision.h"
#include "ecs/Entity.h"

namespace Engine {
    Vector2D Physics2D::m_gravity = Vector2D(0.0f, -9.81f);

    void Physics2D::OnCreate() {
        std::cout << "Physics2D Initialized" << '\n';
    }

    void Physics2D::OnUpdate() {
        // Accumulate time for fixed timestep
        m_accumulator += Time::DeltaTime();
		const auto& fixedTime = Time::FixedDeltaTime();

        // Fixed timestep physics updates
        while (m_accumulator >= fixedTime) {
            FixedUpdate();
            m_accumulator -= fixedTime;
        }
    }

    void Physics2D::FixedUpdate() {
		// Iterate over all entities with Rigidbody2D + Transform + Collider2D components
        const auto entities = m_world->GetEntityManager().Query<Component::Rigidbody2D, Component::Transform, Component::Collider2D>();
        for (auto& [rb, transform, collider] : entities) {
            UpdateRigidbody(rb, transform, &collider);
		}
    }

    void Physics2D::UpdateRigidbody(Component::Rigidbody2D& rb, Component::Transform& t, Component::Collider2D* col) {
		// There's STATIC, KINEMATIC, and DYNAMIC body types
		// Skip static bodies
        if (rb.BodyType == Component::Rigidbody2D::Static)
            return;

        Vector2D intendedPos = t.Position + rb.Velocity * Time::FixedDeltaTime();

        // Apply gravity and drag
        Vector2D acceleration(0.f, 0.f);
        acceleration += m_gravity * rb.GravityScale;
        acceleration += Vector2D(-rb.Velocity.X * rb.Drag, -rb.Velocity.Y * rb.Drag) / rb.Mass;

        // TODO: Add a PolygonCollider2D for line segments and complex shapes
		// DO NOT ADD "LineCollider2D" as it is basically PolygonCollider2D with 2 points
		// Using "LineRenderer" component for boundary lines for now
        // Collision: Circle vs boundary lines
        if (const auto* circle = dynamic_cast<Component::CircleCollider2D*>(col)) {
            auto lines = m_world->GetEntityManager().Query<Component::LineRenderer>();
            for (auto& [lineEntity] : lines) {
                Collision::LineSegment seg = Collision::MakeSegment(lineEntity.Start, lineEntity.End);
                Vector2D contact, normal;
                float tHit;

                Collision::Circle c{ t.Position, circle->Radius };
                if (Collision::CircleVsSegmentSweep(c, intendedPos, seg, contact, normal, tHit)) {
                    Vector2D reflectedDir;
                    Collision::CircleSegmentResponse(contact, normal, intendedPos, reflectedDir);
                    rb.Velocity = reflectedDir * rb.Velocity.Length(); // preserve speed along new direction
                }
            }
        }

        // TODO: handle other collisions here

        // Integrate acceleration
        rb.Velocity += acceleration * Time::FixedDeltaTime();
        t.Position = intendedPos;

        if (!rb.FreezeRotation)
            t.Rotation += rb.AngularVelocity * Time::FixedDeltaTime();
    }

    // Static force/impulse
    void Physics2D::AddForce(Component::Rigidbody2D& rb, const Vector2D& force) {
        if (rb.Mass > 0)
            rb.Velocity += force / rb.Mass;
    }

    void Physics2D::AddImpulse(Component::Rigidbody2D& rb, const Vector2D& impulse) {
        if (rb.Mass > 0)
            rb.Velocity += impulse / rb.Mass;
    }
}
