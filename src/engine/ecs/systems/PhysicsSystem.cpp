#include "ecs/systems/PhysicsSystem.h"
#include "services/Time.h"
#include <iostream>
#include "physics/Collision.h"
#include "ecs/Entity.h"
#include "DynamicCollision.h"

namespace Engine {
    Vector2D PhysicsSystem::m_gravity = Vector2D(0.0f, -9.81f);
    bool PhysicsSystem::m_enabled = true;

    void PhysicsSystem::OnCreate() {
        std::cout << "PhysicsSystem Initialized" << '\n';
    }

    void PhysicsSystem::OnUpdate() {
        if (!m_enabled) return;

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

            //circle vs circle collision
            
            //check dynamic cirlcles vs other circles 
            for (size_t i = 0; i < colliderEntities.size(); ++i) {

                // extract rigidbody compo/transform/collider from collider entity
                auto& [crb, ctransform, collider] = colliderEntities[i];

                //skip for static types
                if (crb.BodyType == Component::Rigidbody2D::Static) continue;

                //check if entity has circlecollider in it
                auto* circle = dynamic_cast<Component::CircleCollider2D*>(&collider);
                if (!circle) continue;

                //check against all other entities after this one eg. -> i+1
                for (size_t j = i + 1; j < colliderEntities.size(); ++j) {

                    //extractor for other components for other enttititiiesss
                    auto& [crb2, ctransform2, collider2] = colliderEntities[j];

                    // Check if other entity has a circle collider
                    const auto* circle2 = dynamic_cast<Component::CircleCollider2D*>(&collider2);
                    if (!circle2) continue;

                    // Build DynamicCollision circle shapes
                    DynCol::Circle c1{ ctransform.Position + collider.Offset, circle->Radius };
                    DynCol::Circle c2{ ctransform2.Position + collider2.Offset, circle2->Radius };

                    // now detect for collision
                    DynCol::Manifold manifold;
                    if (DynCol::Overlap(c1, c2, &manifold)) {
                        // resolve collision response
                        DynCol::ResolveCollision(
                            ctransform.Position,                              // entity 1 position
                            crb.LinearVelocity,                               // entity 1 velocity
                            crb.Mass,                                         // entity 1 mass
                            crb.BodyType == Component::Rigidbody2D::Static,  // staticA - is entity 1 static?
                            ctransform2.Position,                             // entity 2 position
                            crb2.LinearVelocity,                              // entity 2 velocity
                            crb2.Mass,                                        // entity 2 mass
                            crb2.BodyType == Component::Rigidbody2D::Static, // staticis entity 2 static?
                            manifold,                                         // m - collision data
                            0.5f                                              // restitution - bounciness (optional)
                        );
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
