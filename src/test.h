#pragma once

#include "Math/Vector2D.h"
#include <glm/vec4.hpp>
#include "Physics.h"
#include "collision.h"
#include "../include/graphics/renderer.hpp"   


struct CircleEntity {
    Engine::PhysicsComponent physics;  // position (glm::vec3), velocity, damping
    float     radius;
    glm::vec4 color;

    CircleEntity(float r = 20.0f, const glm::vec4& col = glm::vec4(1, 0, 0, 1))
        : radius(r), color(col)
    {
        physics.position = glm::vec3(0, 0, 0);
        physics.velocity = glm::vec3(0, 0, 0);
        physics.damping = 1.0f; // start with NO slowdown; will enable after 5s
    }
};

class TestScene {
public:
    void init(Engine::PhysicsSystem* physics, float winW, float winH, unsigned seed = 0);
    void update(float dt);    // screen clamp+bounce; center-line bounce; enable damping at 5s
    void render(Renderer& r); // draw center lines + balls

private:
    // internals
    void spawnBalls(int count, unsigned seed);
    void bounceAndClamp(CircleEntity& e);          // keep inside screen; reflect on edges
    void collideWithCenterLinesBounce(CircleEntity& e); // reflect on center lines

private:
    Engine::PhysicsSystem* physics_ = nullptr;     // not owned
    std::vector<CircleEntity> balls_;              // owns components (stable addresses)

    float width_ = 1600.0f;
    float height_ = 900.0f;

    Collision::LineSegment segLeft_;
    Collision::LineSegment segRight_;

    float elapsed_ = 0.0f;         // seconds since start
    bool  dampingEnabled_ = false; // flip at >= 5s
};