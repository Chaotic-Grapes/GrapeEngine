

#include "test.h"
#include "../include/graphics/renderer.hpp" // same include style as Engine.cpp
#include "../include/graphics/polygon-utils.hpp"
#include <glm/vec2.hpp>
#include <random>
#include <cmath>
#include <algorithm>
#include "collision.h"
#include "../include/graphics/debugDraw2D.hpp"  
#include <glm/gtc/constants.hpp>

// Tiny helpers for conversions (renderer uses glm::vec2)
static inline glm::vec2 ToGLM2(const glm::vec3& v) { return { v.x, v.y }; }
static inline Vector2D  ToV2(const glm::vec3& v) { return { v.x, v.y }; }


void TestScene::init(Engine::PhysicsSystem* physics, float winW, float winH, unsigned seed) {
    physics_ = physics;
    width_ = winW;
    height_ = winH;

    // lines with gap
    const float gap = 40.0f; 
    const float xMid = width_ * 0.5f;
    const float y0 = height_ * 0.25f;
    const float y1 = height_ * 0.75f;

    // Left line at x = mid - gap/2, right line at x = mid + gap/2
    segLeft_ = Collision::MakeSegment(Vector2D(xMid - 0.5f * gap, y0),
        Vector2D(xMid - 0.5f * gap, y1));
    segRight_ = Collision::MakeSegment(Vector2D(xMid + 0.5f * gap, y0),
        Vector2D(xMid + 0.5f * gap, y1));

    spawnBalls(10, seed);  // create & register 10 balls (randomized)
}

void TestScene::update(float dt) {
    elapsed_ += dt;

    // start damp
    if (!dampingEnabled_ && elapsed_ >= 7.0f) {
        dampingEnabled_ = true;
        for (auto& b : balls_) b.physics.damping = 0.98f;
    }

    for (auto& b : balls_) {
        bounceAndClamp(b);                // reflect off world edges
        collideWithCenterLinesBounce(b);  // reflect off the two center lines
    }
}

void TestScene::render(Renderer& r) {
    // draw center lines
    const glm::vec4 lineColor(1.0f, 1.0f, 1.0f, 1.0f);
    const float thickness = 3.0f;
    const GLuint tex = 0; // no texture

    DebugDraw2D::Line(r,
        glm::vec2(segLeft_.p0.x, segLeft_.p0.y),
        glm::vec2(segLeft_.p1.x, segLeft_.p1.y),
        thickness, lineColor, tex);

    DebugDraw2D::Line(r,
        glm::vec2(segRight_.p0.x, segRight_.p0.y),
        glm::vec2(segRight_.p1.x, segRight_.p1.y),
        thickness, lineColor, tex);

    for (const auto& b : balls_) {
        DebugDraw2D::Circle(r,
            glm::vec2(b.physics.position.x, b.physics.position.y),
            b.radius, b.color, 48, /*textureId*/ 0);
    }
}


void TestScene::spawnBalls(int count, unsigned seed) {
    balls_.clear();
    balls_.reserve(count);

    // seeder
    unsigned usedSeed = (seed != 0) ? seed : std::random_device{}();
    std::mt19937 rng(usedSeed);

    std::uniform_real_distribution<float> radDist(18.0f, 34.0f);
    std::uniform_real_distribution<float> hueDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> speedDist(350.0f, 1500.0f);
    constexpr float TwoPi = 6.28318530718f;
    std::uniform_real_distribution<float> angleDist(0.0f, TwoPi);

    for (int i = 0; i < count; ++i) {
        float r = radDist(rng);

        // Pastel colours
        float h = hueDist(rng);
        glm::vec4 color(
            0.5f + 0.5f * std::cos(TwoPi * h + 0.0f),
            0.5f + 0.5f * std::cos(TwoPi * h + 2.094f),
            0.5f + 0.5f * std::cos(TwoPi * h + 4.188f),
            1.0f
        );

        CircleEntity e(r, color);

        // Random position fully inside the screen
        std::uniform_real_distribution<float> xDist(r, width_ - r);
        std::uniform_real_distribution<float> yDist(r, height_ - r);
        e.physics.position = glm::vec3(xDist(rng), yDist(rng), 0.0f);

        // Random velocity from speed + angle
        float ang = angleDist(rng);
        float speed = speedDist(rng);
        e.physics.velocity = glm::vec3(std::cos(ang) * speed, std::sin(ang) * speed, 0.0f);

        // No damping initially
        e.physics.damping = 1.0f;

        balls_.push_back(e);
        physics_->AddEntity(&balls_.back().physics); // register stable address
    }
}

// Keep inside the screen using your Vector2D::Clamp; 
void TestScene::bounceAndClamp(CircleEntity& e) {
    const float r = e.radius;
    auto& p = e.physics.position;
    auto& v = e.physics.velocity;

    Vector2D pre(p.x, p.y);
    Vector2D lo(r, r), hi(width_ - r, height_ - r);
    Vector2D post = Vector2D::ClampVector(pre, lo, hi);

    if (post.x != pre.x) v.x = -v.x; // bounced on left/right edge
    if (post.y != pre.y) v.y = -v.y; // bounced on bottom/top edge

    p.x = post.x;
    p.y = post.y;
}

// Bounce off the two center lines using closest-point of contact + reflection
void TestScene::collideWithCenterLinesBounce(CircleEntity& e) {
    const float r = e.radius;
    auto& p = e.physics.position;
    auto& v = e.physics.velocity;

    const Collision::LineSegment segs[2] = { segLeft_, segRight_ };
    const Vector2D P = ToV2(p);

    for (const auto& seg : segs) {
        float t = 0.0f;
        Vector2D Q; // closest point on the segment to the circle center

       
        if (Collision::PointVsSegment(P, seg.p0, seg.p1, r, &t, &Q)) {
            // Normal from line to center
            Vector2D m = P - Q;                    
            float m2 = m.SquareLength();
            Vector2D n = (m2 > 0.0f) ? m * (1.0f / std::sqrt(m2))
                : seg.normal; // fallback if numerically right on the line

            // Position correction: place center exactly r away from line
            Vector2D newCenter = Q + n * r;
            p.x = newCenter.x;
            p.y = newCenter.y;

            // Reflect velocity: v' = v - 2*(v·n)*n
            glm::vec2 ng(n.x, n.y);
            glm::vec2 v2(v.x, v.y);
            glm::vec2 vRef = v2 - 2.0f * glm::dot(v2, ng) * ng;

            v.x = vRef.x;
            v.y = vRef.y;

        
            return;
        }
    }
}
