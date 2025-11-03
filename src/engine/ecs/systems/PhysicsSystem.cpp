#include "ecs/systems/PhysicsSystem.h"
#include "services/Time.h"
#include <iostream>
#include "physics/Collision.h"
#include "ecs/Entity.h"
#include <services/OverlayService.h>

namespace Engine {
    Vector2D PhysicsSystem::m_gravity = Vector2D(0.0f, -9.81f);
    bool PhysicsSystem::m_enabled = true;

    void PhysicsSystem::OnCreate() {
        std::cout << "PhysicsSystem Initialized" << '\n';
    }

    void PhysicsSystem::OnUpdate() {
        if (!m_enabled) return;

        // 2 instances when physics should run:
        // 1) Playing (IsGamePlaying() == true, IsStepRequested() == false) 
        // 2) Paused + STEP (IsGamePlaying() == false, IsStepRequested() == true)
        auto* overlay = m_world->GetSystem<Overlay>();
        if (!overlay) {
            return; 
        }

        // 1) Physics runs when m_gameState == Playing
        // So if m_gameState == Paused for e.g. it just freezes the frame (somehow it all works out)
        if (!overlay->IsGamePlaying() && !overlay->IsStepRequested()) {
            return;
        }

        // 2) Physics runs when m_gameState == Paused + STEP (step-by-step physics mode)
        // Clear step flag if set (1 frame)
        if (overlay->IsStepRequested()) {
            overlay->ClearStepRequest();
        }

        // Query once
        const auto entities = m_world->GetEntityManager().Query<Component::Rigidbody2D, Component::Transform>();
        const auto colliderEntities = m_world->GetEntityManager().Query<Component::Rigidbody2D, Component::Transform, Component::Collider2D>();
        const auto lines = m_world->GetEntityManager().Query<Component::LineRenderer>();

        for (auto& [rb, transform] : entities) {
            // There's STATIC, KINEMATIC, and DYNAMIC body types
            // Skip static bodies
            if (rb.BodyType == Component::Rigidbody2D::Static)
                continue;

            Vector2D intendedPos = transform.Position + rb.LinearVelocity * Time::FixedDeltaTime();

            // Apply gravity and drag
            Vector2D acceleration(0.f, 0.f);
            acceleration += m_gravity * rb.GravityScale;
            acceleration += Vector2D(-rb.LinearVelocity.X * rb.LinearDamping, -rb.LinearVelocity.Y * rb.LinearDamping) / rb.Mass;

            // TODO: Add a PolygonCollider2D for line segments and complex shapes
			// DO NOT ADD "LineCollider2D" as it is basically PolygonCollider2D with 2 points
			// Using "LineRenderer" component for boundary lines for now
            // Collision: Circle vs boundary lines
            for (auto& [crb, ctransform, collider] : colliderEntities) {
                if (const auto* circle = dynamic_cast<Component::CircleCollider2D*>(&collider)) {
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
    void PhysicsSystem::AddForce(Component::Rigidbody2D& rb, const Vector2D& force) {
        if (rb.Mass > 0)
            rb.LinearVelocity += force / rb.Mass;
    }

    void PhysicsSystem::AddImpulse(Component::Rigidbody2D& rb, const Vector2D& impulse) {
        if (rb.Mass > 0)
            rb.LinearVelocity += impulse / rb.Mass;
    }
}
