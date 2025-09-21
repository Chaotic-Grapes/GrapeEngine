#include "PhysicsCollision2DTest.h"
#include <cmath>
#include <iostream>
#include "math/MathHelper.h"
#include "Physics2D.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include "ecs/Components.h"
#include "systems/Time.h"
#include "systems/Window.h"
#include "systems/WindowManager.h"
#include "input.h"

using Component::Rigidbody2D;
using Component::ShapeRenderer2D;
using Component::CircleCollider2D;
using Component::LineRenderer;


constexpr float TWO_PI = 6.28318530718f;

Sandbox::PhysicsCollision2DTestScene::PhysicsCollision2DTestScene(const int width, const int height, const float dampingDelay) {
    CREATE_WINDOW("Physics & Collision Test", width, height);
    m_worldWidth = static_cast<float>(width);
    m_worldHeight = static_cast<float>(height);
    m_elapsedTime = 0.0f;
	m_dampingDelay = dampingDelay;
    m_dampingEnabled = false;

    // Disable gravity for this test (balls should fly around freely)
    Engine::Physics2D::SetGravity(Vector2D(0.0f, -800.0f));
}

void Sandbox::PhysicsCollision2DTestScene::OnStart(World& world) {
    CreateBoundaryLines(world);
    SpawnBalls(world, 10); // Start with fewer balls

    std::cout << "PhysicsCollision2DTestScene initialized with " << m_balls.size() << " balls" << '\n';
    
    //test tri
    CreateTriangle(world);
}

void Sandbox::PhysicsCollision2DTestScene::CreateBoundaryLines(World& world) {
	constexpr float gap = 150.0f;
    const float xMid = m_worldWidth * 0.5f;
    const float y0 = m_worldHeight * 0.25f;
    const float y1 = m_worldHeight * 0.75f;

    // Create left boundary line
    Entity leftLine = world.CreateEntity();
    leftLine.AddComponent<Component::LineRenderer>(
        Vector2D(xMid - gap * 0.5f, y0),
        Vector2D(xMid - gap * 0.5f, y1),
        3.f
    );
    m_boundaryLines.push_back(leftLine);

    // Create right boundary line  
    Entity rightLine = world.CreateEntity();
    rightLine.AddComponent<Component::LineRenderer>(
        Vector2D(xMid + gap * 0.5f, y0),
        Vector2D(xMid + gap * 0.5f, y1),
        3.f
    );
    m_boundaryLines.push_back(rightLine);
}

void Sandbox::PhysicsCollision2DTestScene::SpawnBalls(World& world, const int count, const unsigned seed) {
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
        Entity ball = world.CreateEntity();

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
        rigidbody.GravityScale = 1.0f; // No gravity for this test

        // Set random velocity
        const float speed = MathHelper::Randomize<float>(200.0f, 300.0f, seed); 
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

void Sandbox::PhysicsCollision2DTestScene::CreateTriangle(World& world) {
    Entity e = world.CreateEntity();
    m_playerId = e.GetId();

    const float cx = m_worldWidth * 0.25f;
    const float cy = m_worldHeight * 0.50f;

    auto& tr = e.Transform();
    tr.Position.X = cx;
    tr.Position.Y = cy;

    auto& rb = e.AddComponent<Component::Rigidbody2D>();
    rb.Mass = 2.0f;
    rb.GravityScale = 1.0f;     // gravity on (balls have 0)
    rb.Drag = 0.10f;

    e.AddComponent<Component::CircleCollider2D>(m_triHalfHeight);

    // Give it a starting velocity like the balls do:
    rb.Velocity.X = 250.0f;     // NEW: immediate visible motion
    rb.Velocity.Y = 0.0f;

    // Build world-space triangle points around (cx, cy)
    std::vector<Vector2D> pts;
    pts.emplace_back(cx + 0.0f, cy + m_triHalfHeight);
    pts.emplace_back(cx - m_triHalfBase, cy - m_triHalfHeight);
    pts.emplace_back(cx + m_triHalfBase, cy - m_triHalfHeight);

    const Color fill(0.95f, 0.90f, 0.20f, 1.0f);
    Component::ShapeRenderer2D triShape = Component::ShapeRenderer2D::Polygon(pts, fill, /*closed*/ true);
    e.AddComponent<Component::ShapeRenderer2D>(triShape);
}

void Sandbox::PhysicsCollision2DTestScene::UpdateTriangleControls(float /*dt*/) {
    // kept for symmetry; we apply input in ClampAndBouncePlayer(World&) where we have World&
}

void Sandbox::PhysicsCollision2DTestScene::ClampAndBouncePlayer(World& world) {
    if (m_playerId == UINT32_MAX) return;

    // Rebuild a handle on-the-fly from (id, world)
    Entity player(m_playerId, &world);
    auto& tr = player.Transform();
    auto* rb = player.GetComponent<Rigidbody2D>();
    auto* shape = player.GetComponent<ShapeRenderer2D>();
    if (!rb) return;

    // input plus jump
    const float thrust = 1.0f;  // continuous force
    const float jumpImpulse = 20.0f;   // discrete impulse
    const float velDampScale = 0.996f;   // mild numerical damping

    Vector2D F(-1.0f, 0.0f);
    if (Input::WasKeyJustPressed(KEY_A)) Engine::Physics2D::AddForce(*rb, F);
    else if (Input::IsKeyPressed(KEY_D)) {std::cout << F.X; } 
    else if (Input::IsKeyPressed(KEY_W)) F.Y += 0.25f * thrust;
    else if (Input::IsKeyPressed(GLFW_KEY_S)) F.Y -= 0.25f * thrust;

    if (F.X != 0.0f || F.Y != 0.0f) {
        Engine::Physics2D::AddForce(*rb, F);
    }
    if (Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        Engine::Physics2D::AddImpulse(*rb, Vector2D(0.0f, jumpImpulse));
        std::cout << jumpImpulse;
    }
    rb->Velocity *= velDampScale;

    // clamp bounce against window sides
    const float r = m_triHalfHeight; // simple bounding circle for the triangle

    // Left / Right
    if (tr.Position.X - r <= 0.0f) {
        tr.Position.X = r;
        if (rb->Velocity.X < 0.0f) rb->Velocity.X = -rb->Velocity.X;
    }
    else if (tr.Position.X + r >= m_worldWidth) {
        tr.Position.X = m_worldWidth - r;
        if (rb->Velocity.X > 0.0f) rb->Velocity.X = -rb->Velocity.X;
    }

    // Bottom / Top
    if (tr.Position.Y - r <= 0.0f) {
        tr.Position.Y = r;
        if (rb->Velocity.Y < 0.0f) rb->Velocity.Y = -rb->Velocity.Y;
    }
    else if (tr.Position.Y + r >= m_worldHeight) {
        tr.Position.Y = m_worldHeight - r;
        if (rb->Velocity.Y > 0.0f) rb->Velocity.Y = -rb->Velocity.Y;
    }

    std::vector<Vector2D> pts;
    pts.emplace_back(tr.Position.X + 0.0f, tr.Position.Y + m_triHalfHeight);
    pts.emplace_back(tr.Position.X - m_triHalfBase, tr.Position.Y - m_triHalfHeight);
    pts.emplace_back(tr.Position.X + m_triHalfBase, tr.Position.Y - m_triHalfHeight);

    shape->Type = ShapeRenderer2D::ShapeType::Polygon;
    shape->Points = pts;     // overwrite with fresh world-space points
    shape->Closed = true;

}


void Sandbox::PhysicsCollision2DTestScene::OnUpdate(World& world) {
    (void)world;

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
    // Triangle input plus bounce
    ClampAndBouncePlayer(world);
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

void Sandbox::PhysicsCollision2DTestScene::OnShutdown(World& world) {
    m_balls.clear();
    m_boundaryLines.clear();
}