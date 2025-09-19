#include "PhysicsCollision2DTestScene.h"
#include <cmath>
#include <iostream>
#include "math/MathHelper.h"
#include "Physics2D.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include "systems/Time.h"

constexpr float TWO_PI = 6.28318530718f;

Sandbox::PhysicsCollision2DTestScene::PhysicsCollision2DTestScene(World* world, const float width, const float height, const float dampingDelay, const unsigned seed) {
    m_world = world;
    m_worldWidth = width;
    m_worldHeight = height;
    m_elapsedTime = 0.0f;
	m_dampingDelay = dampingDelay;
    m_dampingEnabled = false;

    // Disable gravity for this test (balls should fly around freely)
    Engine::Physics2D::SetGravity(Vector2D(0.0f, 0.0f));

    CreateBoundaryLines();
    CreateBalls(50, seed); // Start with fewer balls

    std::cout << "PhysicsCollision2DTestScene initialized with " << m_balls.size() << " balls" << '\n';
}

void Sandbox::PhysicsCollision2DTestScene::CreateBoundaryLines() {
	constexpr float gap = 40.0f;
    const float xMid = m_worldWidth * 0.5f;
    const float y0 = m_worldHeight * 0.25f;
    const float y1 = m_worldHeight * 0.75f;

    // Create left boundary line
    Entity leftLine = m_world->CreateEntity();
    leftLine.AddComponent<Component::LineRenderer>(
        Vector2D(xMid - gap * 0.5f, y0),
        Vector2D(xMid - gap * 0.5f, y1),
        3.f
    );
    m_boundaryLines.push_back(leftLine);

    // Create right boundary line  
    Entity rightLine = m_world->CreateEntity();
    rightLine.AddComponent<Component::LineRenderer>(
        Vector2D(xMid + gap * 0.5f, y0),
        Vector2D(xMid + gap * 0.5f, y1),
        3.f
    );
    m_boundaryLines.push_back(rightLine);
}

void Sandbox::PhysicsCollision2DTestScene::CreateBalls(const int count, const unsigned seed) {
    m_balls.clear();
    m_balls.reserve(count);

    for (int i = 0; i < count; ++i) {
        float radius = MathHelper::Randomize<float>(18.0f, 34.0f, seed);

        // Generate pastel color using hue
        const float hue = MathHelper::Randomize<float>(0.0f, 1.0f, seed);
        const Color color(
            0.5f + 0.5f * std::cos(TWO_PI * hue + 0.0f),
            0.5f + 0.5f * std::cos(TWO_PI * hue + 2.094f),
            0.5f + 0.5f * std::cos(TWO_PI * hue + 4.188f),
            1.0f
        );

        // Create ball entity
        Entity ball = m_world->CreateEntity();

        // Set random position
		const float x = MathHelper::Randomize<float>(radius, m_worldWidth - radius, seed),
    				y = MathHelper::Randomize<float>(radius, m_worldHeight - radius, seed);

        auto& transform = ball.Transform();
        transform.Position.X = x;
        transform.Position.Y = y;

        // Add Rigidbody2D
        auto& rigidbody = ball.AddComponent<Component::Rigidbody2D>();
        rigidbody.Mass = 1.0f;
        rigidbody.Drag = 0.0f; // No drag initially
        rigidbody.GravityScale = 0.0f; // No gravity for this test

        // Set random velocity
        const float speed = MathHelper::Randomize<float>(100.0f, 400.0f, seed); // Reduced speed for better visualization
        const float angle = MathHelper::Randomize<float>(0.0f, TWO_PI, seed);
        rigidbody.Velocity.X = std::cos(angle) * speed;
        rigidbody.Velocity.Y = std::sin(angle) * speed;

        // Add visual components
        auto& shapeRenderer = ball.AddComponent<Component::ShapeRenderer2D>();
		shapeRenderer.Type = Component::ShapeRenderer2D::ShapeType::Circle;
		shapeRenderer.Radius = radius;
        shapeRenderer.FillColor = color;

        auto& circleCollider = ball.AddComponent<Component::CircleCollider2D>(radius);

        m_balls.push_back(ball);
        std::cout << "Created ball (" << i + 1 << ") with ENT ID " << ball.GetId() << " at (" << x << ", " << y << ") with radius " << radius << '\n';
    }
}

void Sandbox::PhysicsCollision2DTestScene::OnUpdate() {
    m_elapsedTime = static_cast<float>(Time::ElapsedTime());

    // Enable damping after specified delay
    if (!m_dampingEnabled && m_elapsedTime >= m_dampingDelay) {
        m_dampingEnabled = true;

        for (auto& ball : m_balls) {
            auto* rigidbody = ball.GetComponent<Component::Rigidbody2D>();
            if (rigidbody) {
                rigidbody->Drag = 0.5f;
            }
        }

        std::cout << "Damping enabled after " << m_dampingDelay << " seconds!" << '\n';
    }

    UpdateBallCollisions();
}

void Sandbox::PhysicsCollision2DTestScene::UpdateBallCollisions() {
    for (auto& ball : m_balls) {
        auto* rigidbody = ball.GetComponent<Component::Rigidbody2D>();
        const auto* circleCollider = ball.GetComponent<Component::CircleCollider2D>();
        auto& transform = ball.Transform();

        if (!rigidbody || !circleCollider)
            continue;

        const float radius = circleCollider->Radius;

        // Simple boundary collision (bounce off-screen edges)
        bool bounced = false;

        // Left/Right walls
        if (transform.Position.X - radius <= 0.0f) {
            transform.Position.X = radius;
            rigidbody->Velocity.X = std::abs(rigidbody->Velocity.X);
            bounced = true;
        }
        else if (transform.Position.X + radius >= m_worldWidth) {
            transform.Position.X = m_worldWidth - radius;
            rigidbody->Velocity.X = -std::abs(rigidbody->Velocity.X);
            bounced = true;
        }

        // Top/Bottom walls
        if (transform.Position.Y - radius <= 0.0f) {
            transform.Position.Y = radius;
            rigidbody->Velocity.Y = std::abs(rigidbody->Velocity.Y);
            bounced = true;
        }
        else if (transform.Position.Y + radius >= m_worldHeight) {
            transform.Position.Y = m_worldHeight - radius;
            rigidbody->Velocity.Y = -std::abs(rigidbody->Velocity.Y);
            bounced = true;
        }

        // TODO: Add collision with center lines with the collision system
    }
}

void Sandbox::PhysicsCollision2DTestScene::Cleanup() {
    m_balls.clear();
    m_boundaryLines.clear();
}