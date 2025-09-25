#ifndef PHYSICS2D_H
#define PHYSICS2D_H

#include "ecs/Components.h"
#include "ecs/ISystem.h"
#include "ecs/World.h"

class Entity;
namespace Engine {
    class Physics2D : public ISystem {
    public:
        Physics2D(World* world) : m_world(world) {}

        void OnCreate() override;
        void OnUpdate() override;
        void OnFixedUpdate() override {}
        std::string Name() const override { return "Physics2D"; }

        static void SetGravity(const Vector2D& gravity) { m_gravity = gravity; }
        static Vector2D GetGravity() { return m_gravity; }

        static void SetEnabled(bool enabled) { m_enabled = enabled; }
        static bool IsEnabled() { return m_enabled; }

        static void AddForce(Component::Rigidbody2D& rb, const Vector2D& force);
        static void AddImpulse(Component::Rigidbody2D& rb, const Vector2D& impulse);

    private:
        void UpdateRigidbody(Component::Rigidbody2D& rb, Component::Transform& t, Component::Collider2D* col);

        World* m_world;
        static Vector2D m_gravity;

        static bool m_enabled;
    };
}

#endif