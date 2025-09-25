#pragma once

#include "Math/Vector2D.h"
#include <glm/vec4.hpp>
#include "Physics.h"
#include "collision.h"
#include "../include/graphics/renderer.hpp"   
#include "../include/graphics/debugDraw2D.hpp"  
#include "../include/graphics/polygon-utils.hpp"
#include "Math/Vector4D.h"


struct CircleEntity {
    Engine::PhysicsComponent physics;  // position (glm::vec3), velocity, damping
    float     radius;
    glm::vec4 color;

    CircleEntity(float r = 20.0f, const  glm::vec4& col = glm::vec4(1, 0, 0, 1))
        : radius(r), color(col)
    {
        physics.position = Vector3D(0.0f, 0.0f, 0.0f);
        physics.velocity = Vector3D(0.0f, 0.0f, 0.0f);
        physics.damping = 1.0f; // start with NO slowdown; will enable after 5s
    }
};

// Player triangle (isosceles, upright)
struct PlayerTriangle {
    Engine::PhysicsComponent physics;
    float width = 80.0f;   // base width
    float height = 90.0f;   // tip-to-base height
    glm::vec4 color = glm::vec4(1, 0.9f, 0.2f, 1);

    PlayerTriangle() {
        physics.position = Vector3D(0, 0, 0);
        physics.velocity = Vector3D(0, 0, 0);
        physics.damping = 1.0f;
        physics.useGravity = true;
        physics.dragCoefficient = 0.02f; // light air drag
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
    void updatePlayer(float dt);                  // input, jump, clamp
    void updatePlayerSegments();                  // build 3 segments from triangle verts
    void collideCircleWithTriangle(CircleEntity& e); // circle vs each triangle edge

    static inline Vector2D ToV2(const Vector3D& v) { return { v.X, v.Y }; }

private:
    Engine::PhysicsSystem* physics_ = nullptr;     // not owned
    std::vector<CircleEntity> balls_;              // owns components (stable addresses)

    PlayerTriangle player_;
    Collision::LineSegment triSeg_[3];             // triangle edges as segments

    float width_ = 1600.0f;
    float height_ = 900.0f;

    Collision::LineSegment segLeft_;
    Collision::LineSegment segRight_;

    float elapsed_ = 0.0f;         // seconds since start
    bool  dampingEnabled_ = false; // flip at >= 5s

    float dampingStartTime_ = 5.0f;
    float dampingValue_ = 0.995f;              // gentle slowdown

    bool  playerGrounded_ = false;
};