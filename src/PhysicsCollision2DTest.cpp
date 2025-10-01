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

Sandbox::PhysicsCollision2DTestScene::PhysicsCollision2DTestScene(const int width, const int height, const float dampingDelay) : Scene("PhysicsCollision2DTestScene") {
    CREATE_WINDOW("Physics & Collision Test", width, height);
    m_worldWidth = static_cast<float>(width);
    m_worldHeight = static_cast<float>(height);
    m_elapsedTime = 0.0f;
	m_dampingDelay = dampingDelay;
    m_dampingEnabled = false;

    // step-by-step physics mode initialization
    m_stepByStepMode = false;
    m_stepRequested = false;
    m_pausePhysics = false;

    // Disable gravity for this test (balls should fly around freely)
    Engine::Physics2D::SetGravity(Vector2D(0.0f, -15.0f));
}

void Sandbox::PhysicsCollision2DTestScene::OnLoad() {
   // CreateBoundaryLines(world);
    World& world = GetWorld();
    SpawnBalls(world, 10); // Start with fewer balls

    std::cout << "PhysicsCollision2DTestScene initialized with " << m_balls.size() << " balls" << '\n';

    std::cout << "Step-by-step physics controls:" << '\n';
    std::cout << "  P - Toggle step-by-step mode" << '\n';
    std::cout << "  Space - Step physics (when in step mode)" << '\n';

    SpawnCubes();
    //test tri
    if (m_playerId == UINT32_MAX) {
        CreateTriangle();
    }
}

//void Sandbox::PhysicsCollision2DTestScene::CreateBoundaryLines(World& world) {
//	//constexpr float gap = 150.0f;
// //   const float xMid = m_worldWidth * 0.5f;
// //   const float y0 = m_worldHeight * 0.25f;
// //   const float y1 = m_worldHeight * 0.75f;
//
// //   // Create left boundary line
// //   Entity leftLine = world.CreateEntity();
// //   leftLine.AddComponent<Component::LineRenderer>(
// //       Vector2D(xMid - gap * 0.5f, y0),
// //       Vector2D(xMid - gap * 0.5f, y1),
// //       3.f
// //   );
// //   m_boundaryLines.push_back(leftLine);
//
// //   // Create right boundary line  
// //   Entity rightLine = world.CreateEntity();
// //   rightLine.AddComponent<Component::LineRenderer>(
// //       Vector2D(xMid + gap * 0.5f, y0),
// //       Vector2D(xMid + gap * 0.5f, y1),
// //       3.f
// //   );
// //   m_boundaryLines.push_back(rightLine);
//}

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
        Entity ball = world.CreateEntity("Ball");

        // Set random position
        const float minX = radius;
        const float maxX = m_worldWidth - radius;
        const float minY = radius;
        const float maxY = m_worldHeight - radius;

        const float x = (maxX > minX) ? MathHelper::Randomize<float>(minX, maxX, seed)
            : 0.5f * m_worldWidth;
        const float y = (maxY > minY) ? MathHelper::Randomize<float>(minY, maxY, seed)
            : 0.5f * m_worldHeight;

        auto& transform = ball.Transform();
        transform.Position.X = x;
        transform.Position.Y = y;

        // Add Rigidbody2D
        auto& rigidbody = ball.AddComponent<Component::Rigidbody2D>();
        rigidbody.Mass = 1.0f;
        rigidbody.LinearDamping = 0.0f; // No drag initially
        rigidbody.GravityScale = 1.0f; // No gravity for this test

        // Set random velocity
        const float speed = MathHelper::Randomize<float>(200.0f, 300.0f, seed); 
        const float angle = MathHelper::Randomize<float>(0.0f, TWO_PI, seed);
        rigidbody.LinearVelocity.X = std::cos(angle) * speed;
        rigidbody.LinearVelocity.Y = std::sin(angle) * speed;

        // Add visual components
        auto& shapeRenderer = ball.AddComponent<Component::ShapeRenderer2D>();
		shapeRenderer.Type = Component::ShapeRenderer2D::ShapeType::Circle;
		shapeRenderer.Radius = radius;
        shapeRenderer.FillColor = color;

    	ball.AddComponent<Component::CircleCollider2D>(radius);

        m_balls.push_back(ball);
        std::cout << "Created ball (" << i + 1 << ") with ENT ID " << ball.GetId() << " at (" << x << ", " << y << ") with radius " << radius << '\n';
    }
}


void Sandbox::PhysicsCollision2DTestScene::SpawnCubes() {
    if ((int)m_seacubes.size() >= m_maxLeaves) return;

    // random square size and x spawn at top
    const float size = MathHelper::Randomize<float>(14.0f, 28.0f, 0);
    const float half = size * 0.5f;
    //const float minX = half;
    //const float maxX = m_worldWidth - half;
    const float x = MathHelper::Randomize<float>(half, m_worldWidth - half, 0);

    const float y = m_worldHeight - half - 2.0f; 

    Entity cube = CreateEntity("Cube");

    auto& tr = cube.Transform();
    tr.Position.X = x;
    tr.Position.Y = y;

    auto& rb = cube.AddComponent<Rigidbody2D>();
    rb.Mass = 0.8f;
    rb.GravityScale = 1.0f;
    rb.LinearDamping = 0.25f;
    rb.LinearVelocity.X = MathHelper::Randomize<float>(-20.0f, 20.0f, 0);
    rb.LinearVelocity.Y = 0.0f;
        
    // visual: make a square polygon in WORLD-SPACE (renderer polygon path)
    std::vector<Vector2D> pts;
    pts.emplace_back(x - half, y - half);
    pts.emplace_back(x - half, y + half);
    pts.emplace_back(x + half, y + half);
    pts.emplace_back(x + half, y - half);

    auto& shape = cube.AddComponent<ShapeRenderer2D>();
    cube.AddComponent<Component::CircleCollider2D>(half);
    shape.Type = ShapeRenderer2D::ShapeType::Polygon;
    shape.Points = pts;
    shape.Closed = true;
    shape.FillColor = Color(0.15f, 0.65f, 0.95f, 1.0f); // sea-ish

    m_seacubes.push_back(cube);
}

void Sandbox::PhysicsCollision2DTestScene::SpawnCubes_T(float dt) {
    m_spawnAcc += dt;
    while (m_spawnAcc >= m_spawnIntervals) {
        m_spawnAcc -= m_spawnIntervals;
        // spawn 1–2 cubes per tick for variety
        int batch = MathHelper::Randomize<int>(2, 7, 0);
        for (int i = 0; i < batch; ++i) SpawnCubes();
    }
}

void Sandbox::PhysicsCollision2DTestScene::CubeDisintegrate(World& world, size_t i) {
    if (i >= m_seacubes.size()) return;
    world.GetEntityManager().DestroyEntity(m_seacubes[i]);
    m_seacubes[i] = m_seacubes.back();
    m_seacubes.pop_back();
}

void Sandbox::PhysicsCollision2DTestScene::UpdateCubesCollisions(World& world) {
    // hero handle + circle collider
    if (m_playerId == UINT32_MAX) return;
    Entity hero(m_playerId, &world);
    auto* hc = hero.GetComponent<CircleCollider2D>();
    auto& ht = hero.Transform();
    if (!hc) return;

    const float heroR = hc->Radius;
    const float floorY = 0.0f;

    // iterate cubes
    for (size_t i = 0; i < m_seacubes.size(); /* increment in loop */) {
        auto& ct = m_seacubes[i].Transform();
        auto* rb = m_seacubes[i].GetComponent<Rigidbody2D>();

        // rebuild square polygon points (WORLD-SPACE) so it renders at new Transform
        // derive half from current polygon width (or keep a cached size if you add one)
        float half;
        if (auto* sh = m_seacubes[i].GetComponent<ShapeRenderer2D>()) {
            if (sh->Type == ShapeRenderer2D::ShapeType::Polygon && sh->Points.size() >= 4) {
                // compute half from current points bounding box (cheap)
                float minX = sh->Points[0].X, maxX = sh->Points[0].X;
                float minY = sh->Points[0].Y, maxY = sh->Points[0].Y;
                for (auto& p : sh->Points) {
                    if (p.X < minX) minX = p.X; if (p.X > maxX) maxX = p.X;
                    if (p.Y < minY) minY = p.Y; if (p.Y > maxY) maxY = p.Y;
                }
                // original half in model = (max-min)/2
                half = 0.5f * std::max(maxX - minX, maxY - minY);
                // now rebuild the polygon around current transform:
                const float x = ct.Position.X, y = ct.Position.Y;
                sh->Points.clear();
                sh->Points.emplace_back(x - half, y - half);
                sh->Points.emplace_back(x - half, y + half);
                sh->Points.emplace_back(x + half, y + half);
                sh->Points.emplace_back(x + half, y - half);
                sh->Closed = true;
            }
            else {
                // fallback if something changed
                half = 10.0f;
            }
        }
        else {
            half = 10.0f;
        }

        // --- floor evaporation (bottom of screen)
        if (ct.Position.Y - half <= floorY) {
            CubeDisintegrate(world, i);
            continue;
        }

        // --- hero vs cube (DynCol: Circle vs AABB, DISCRETE)
        DynCol::Circle Chero{ Vector2D(ht.Position.X, ht.Position.Y), heroR };
        DynCol::AABB   Bcube{ Vector2D(ct.Position.X - half, ct.Position.Y - half),
                              Vector2D(ct.Position.X + half, ct.Position.Y + half) };

        if (DynCol::Overlap(Chero, Bcube, nullptr)) {
            CubeDisintegrate(world, i);
            continue;
        }

        // still alive
        ++i;
        (void)rb;
    }
}

void Sandbox::PhysicsCollision2DTestScene::CreateTriangle() {
    if (m_playerId != UINT32_MAX) return; // <-- guard
    Entity e = CreateEntity("Triangle");
    m_playerId = e.GetId();

    const float cx = m_worldWidth * 0.25f;
    const float cy = m_worldHeight * 0.50f;

    auto& tr = e.Transform();
    tr.Position.X = cx;
    tr.Position.Y = cy;

    auto& rb = e.AddComponent<Component::Rigidbody2D>();
    rb.Mass = 2.0f;
    rb.GravityScale = 1.0f;     // gravity on (balls have 0)
    rb.LinearDamping = 0.10f;

    e.AddComponent<Component::CircleCollider2D>(m_triHalfHeight);

    // Give it a starting velocity like the balls do:
    rb.LinearVelocity.X = 250.0f;     // NEW: immediate visible motion
    rb.LinearVelocity.Y = 0.0f;

    // Build world-space triangle points around (cx, cy)
    std::vector<Vector2D> pts;
    pts.emplace_back(cx + 0.0f, cy + m_triHalfHeight);
    pts.emplace_back(cx - m_triHalfBase, cy - m_triHalfHeight);
    pts.emplace_back(cx + m_triHalfBase, cy - m_triHalfHeight);

    const Color fill(0.95f, 0.90f, 0.20f, 1.0f);
    Component::ShapeRenderer2D triShape = Component::ShapeRenderer2D::Polygon(pts, fill, true);
    e.AddComponent<Component::ShapeRenderer2D>(triShape);
}

void Sandbox::PhysicsCollision2DTestScene::ClampAndBouncePlayer() {
    if (m_playerId == UINT32_MAX) return;
    World& world = GetWorld();

    // build player
    Entity player(m_playerId, &world);
    auto& tr = player.Transform();
    auto* rb = player.GetComponent<Rigidbody2D>();
    auto* shape = player.GetComponent<ShapeRenderer2D>();
    if (!rb) return;

    // input plus jump
    const float thrust = 1.0f;  // continuous force
    const float jumpImpulse = 20.0f;   // discrete impulse
    const float velDampScale = 0.996f;   // mild numerical damping

    bool leftHeld = Input::IsKeyDown(KEY_A);
    bool rightHeld = Input::IsKeyDown(KEY_D);
    bool upHeld = Input::IsKeyDown(KEY_W);
    bool downHeld = Input::IsKeyDown(KEY_S);

    Vector2D F(0.0f, 0.0f);
    if (leftHeld)  F.X -= thrust;
    if (rightHeld) F.X += thrust;
    if (upHeld)    F.Y += thrust;
    if (downHeld)  F.Y -= thrust;

    // Apply only when non-zero (no force added when keys are up)
    if (F.X != 0.0f || F.Y != 0.0f) {
        Engine::Physics2D::AddForce(*rb, F);
    }

    if (Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        Engine::Physics2D::AddImpulse(*rb, Vector2D(0.0f, jumpImpulse));
        std::cout << jumpImpulse;
    }
    rb->LinearVelocity *= velDampScale;

    // clamp bounce against window sides
    const float r = m_triHalfHeight; // simple bounding circle for the triangle

    // Left / Right
    if (tr.Position.X - r <= 0.0f) {
        tr.Position.X = r;
        if (rb->LinearVelocity.X < 0.0f) rb->LinearVelocity.X = -rb->LinearVelocity.X;
    }
    else if (tr.Position.X + r >= m_worldWidth) {
        tr.Position.X = m_worldWidth - r;
        if (rb->LinearVelocity.X > 0.0f) rb->LinearVelocity.X = -rb->LinearVelocity.X;
    }

    // Bottom / Top
    if (tr.Position.Y - r <= 0.0f) {
        tr.Position.Y = r;
        if (rb->LinearVelocity.Y < 0.0f) rb->LinearVelocity.Y = -rb->LinearVelocity.Y;
    }
    else if (tr.Position.Y + r >= m_worldHeight) {
        tr.Position.Y = m_worldHeight - r;
        if (rb->LinearVelocity.Y > 0.0f) rb->LinearVelocity.Y = -rb->LinearVelocity.Y;
    }

    std::vector<Vector2D> pts;
    pts.emplace_back(tr.Position.X + 0.0f, tr.Position.Y + m_triHalfHeight);
    pts.emplace_back(tr.Position.X - m_triHalfBase, tr.Position.Y - m_triHalfHeight);
    pts.emplace_back(tr.Position.X + m_triHalfBase, tr.Position.Y - m_triHalfHeight);

    shape->Type = ShapeRenderer2D::ShapeType::Polygon;
    shape->Points = pts;     // overwrite with fresh world-space points
    shape->Closed = true;

}

void Sandbox::PhysicsCollision2DTestScene::OnUpdate() {
    m_elapsedTime = static_cast<float>(Time::ElapsedTime());

    // handles step-by-step physics controls
    HandleStepByStepControls();

    // Enable damping after specified delay
    if (!m_dampingEnabled && m_elapsedTime >= m_dampingDelay) {
        m_dampingEnabled = true;

        for (auto& ball : m_balls) {
            auto* rigidbody = ball.GetComponent<Component::Rigidbody2D>();
            if (rigidbody) {
                rigidbody->LinearDamping = 0.5f;
            }
        }

        std::cout << "Damping enabled after " << m_dampingDelay << " seconds!" << '\n';
    }
    // Triangle input plus bounce
    ClampAndBouncePlayer();
}

void Sandbox::PhysicsCollision2DTestScene::HandleStepByStepControls() {
    static bool pWasDown = false;
    static bool spaceWasDown = false;
    static bool wasPaused = false;

    bool pIsDown = Input::IsKeyDown(KEY_P);
    bool spaceIsDown = Input::IsKeyDown(KEY_SPACE);

    // toggle step-by-step mode with P key
    if (pIsDown && !pWasDown) {
        m_stepByStepMode = !m_stepByStepMode;
        m_pausePhysics = m_stepByStepMode;

        Engine::Physics2D::SetEnabled(!m_pausePhysics);

        if (m_pausePhysics) {
            std::cout << "Step-by-step physics mode ENABLED" << '\n';
            // When pausing, store current velocities to restore later
            StoreBallStates();
        }
        else {
            std::cout << "Step-by-step physics mode DISABLED" << '\n';
            // When resuming, restore velocities from before pause
            RestoreBallStates();
        }
    }

    // step physics with SPACE key
    if ((m_stepByStepMode || m_pausePhysics) && spaceIsDown && !spaceWasDown) {
        // for stepping: temporarily enable, step, then disable
        if (m_pausePhysics) {
            Engine::Physics2D::SetEnabled(true);
            m_stepRequested = true;
            std::cout << "Physics step requested" << '\n';
        }
    }

    pWasDown = pIsDown;
    spaceWasDown = spaceIsDown;
    wasPaused = m_pausePhysics;
}

void Sandbox::PhysicsCollision2DTestScene::StoreBallStates() {
    m_storedBallStates.clear();
    for (auto& ball : m_balls) {
        auto* rigidbody = ball.GetComponent<Component::Rigidbody2D>();
        if (rigidbody) {
            m_storedBallStates[ball.GetId()] = {
                rigidbody->LinearVelocity,
                ball.Transform().Position
            };
        }
    }
}

void Sandbox::PhysicsCollision2DTestScene::RestoreBallStates() {
    for (auto& ball : m_balls) {
        auto it = m_storedBallStates.find(ball.GetId());
        if (it != m_storedBallStates.end()) {
            auto* rigidbody = ball.GetComponent<Component::Rigidbody2D>();
            if (rigidbody) {
                rigidbody->LinearVelocity = it->second.velocity;
                ball.Transform().Position = it->second.position;
            }
        }
    }
    m_storedBallStates.clear();
}

void Sandbox::PhysicsCollision2DTestScene::OnFixedUpdate() {
    World& world = GetWorld();
    const float dt = Time::FixedDeltaTime();

    if (!m_pausePhysics || m_stepRequested) {
        UpdateBallCollisions();

        if (m_stepRequested) {
            // step, and pause physics
            Engine::Physics2D::SetEnabled(false);
        }
        m_stepRequested = false;
    }

    //cubess
    SpawnCubes_T(dt);
    UpdateCubesCollisions(world);
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
            rigidbody->LinearVelocity.X = std::abs(rigidbody->LinearVelocity.X);
            bounced = true;
        }
        else if (transform.Position.X + radius >= m_worldWidth) {
            transform.Position.X = m_worldWidth - radius;
            rigidbody->LinearVelocity.X = -std::abs(rigidbody->LinearVelocity.X);
            bounced = true;
        }

        // Top/Bottom walls
        if (transform.Position.Y - radius <= 0.0f) {
            transform.Position.Y = radius;
            rigidbody->LinearVelocity.Y = std::abs(rigidbody->LinearVelocity.Y);
            bounced = true;
        }
        else if (transform.Position.Y + radius >= m_worldHeight) {
            transform.Position.Y = m_worldHeight - radius;
            rigidbody->LinearVelocity.Y = -std::abs(rigidbody->LinearVelocity.Y);
            bounced = true;
        }

        // TODO: Add collision with center lines with the collision system
    }
}

void Sandbox::PhysicsCollision2DTestScene::OnUnload() {
    m_balls.clear();
   // m_boundaryLines.clear();
    m_seacubes.clear();
    m_playerId = UINT32_MAX;
}