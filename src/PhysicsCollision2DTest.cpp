/**
 * @file    PhysicsCollision2DTest.cpp
 * @brief   Sandbox scene for exercising 2D physics & collision features.
 *
 * @details Provides a switchable set of test modes:
 *   • PhysicsMovement — spawns balls, applies damping after a delay, and bounces within bounds. 
 *   • CollisionDetection — hero circle vs static squares (AABB push-out).
 *   • PhysicsHero — triangle "hero" movement & clamping. 
 *   • DynVStatResponse — falling cubes removed on swept hit against static squares.
 *   • DynVDynResponse — ball/ball collision with restitution & friction.
 *   • StepByStepUpdate — togglable (P) pause/step (Space) with state store/restore. 
 *
 * Core helpers:
 *   - SpawnBalls/SpawnCubes and timed batch spawner. 
 *   - UpdateBallCollisions: border resolution with scaled colliders. 
 *   - CubeDisintegrate + UpdateCubesCollisions (hero/cube culling).
 *
 * Controls
 *   - Press **P** to toggle step-by-step mode; **Space** to step while paused. 
 *
 * @dependencies
 *   - World/ECS, Physics2D, Components (Rigidbody2D, ShapeRenderer2D, Colliders), Window/Time/Input. 
 * @notes
 *   - Apply transform scale to collision radii for visual-physics consistency.
 *   - Cubes are kept polygonal; half-size inferred from point bounds each frame. 
 *
 */


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
#include "DynamicCollision.h"

using Component::Rigidbody2D;
using Component::ShapeRenderer2D;
using Component::CircleCollider2D;
using Component::LineRenderer;

constexpr float TWO_PI = 6.28318530718f;

namespace {
    using TestType = Sandbox::PhysicsCollision2DTestScene::TestType;

    // Define the exact order you want to cycle through
    constexpr TestType kCycleOrder[] = {
        TestType::test_PhysicsMovement,
        TestType::test_CollisionDetection,
        TestType::test_PhysicsHero,
        TestType::test_DynvStatResponse,
        TestType::test_DynvDynResponse,
        TestType::test_StepbyStepUpdate, // last
    };

    inline TestType NextTest(TestType cur) {
        const size_t N = sizeof(kCycleOrder) / sizeof(kCycleOrder[0]);
        for (size_t i = 0; i < N; ++i) {
            if (kCycleOrder[i] == cur) {
                return kCycleOrder[(i + 1) % N];  // wrap around
            }
        }
        // Fallback if cur isn't in the list
        return kCycleOrder[0];
    }

    inline EntityId ToId(EntityId id) { return id; }
    inline EntityId ToId(const Entity& e) { return e.GetId(); }

    // Call IsAlive when you only have an id
    inline bool IsAliveById(EntityManager& em, World& world, EntityId id) {
        Entity tmp{ id, &world, "" };                 // matches your ctor (id, world*, name)
        return em.IsAlive(tmp);                     // your IsAlive(const Entity&) overload
    }

}


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

    // set gravity
    Engine::Physics2D::SetGravity(Vector2D(0.0f, -15.0f));
}

void Sandbox::PhysicsCollision2DTestScene::OnLoad() {

}


void Sandbox::PhysicsCollision2DTestScene::OnUpdate() {


    // Input trigger to cycle tests (H key)
    if (Input::IsKeyDown(KEY_C)) {

        if (!test_handler) {
            int current = static_cast<int>(m_currentTest);
            current++;
            if (current > static_cast<int>(TestType::test_StepbyStepUpdate)) {
                current = static_cast<int>(TestType::test_PhysicsMovement);
            }
            m_currentTest = static_cast<TestType>(current);
            std::cout << "Switched to physics test " << current << std::endl;

            OnUnload();
            OnLoad();

            // When switching, you could clear scene and re-run OnLoad if desired
            // For now we keep entities; you can decide to reset if needed.
            test_handler = true;
        }
    }
    else {
        test_handler = false;
    }



    // Dispatch to active test
    switch (m_currentTest) {
    case TestType::test_PhysicsMovement:    Test_PhysicsMovement();    break;
    case TestType::test_CollisionDetection: Test_CollisionDetection(); break;
    case TestType::test_PhysicsHero:        Test_PhysicsHero();        break;
    case TestType::test_DynvStatResponse:   Test_DynVStatResponse();   break;
    case TestType::test_DynvDynResponse:    Test_DynVDynResponse();    break;
    case TestType::test_StepbyStepUpdate:   Test_StepByStepUpdate();   break;
    }
}


void Sandbox::PhysicsCollision2DTestScene::OnFixedUpdate() {
    World& world = GetWorld();
    const float dt = Time::FixedDeltaTime();
    if (m_currentTest != TestType::test_StepbyStepUpdate) {
        return;
    }
    if (!m_pausePhysics || m_stepRequested) {
        UpdateBallCollisions();

        if (m_stepRequested) {
            // step, and pause physics
            Engine::Physics2D::SetEnabled(false);
        }
        m_stepRequested = false;
    }

    // cubes
    SpawnCubes_T(dt);
    UpdateCubesCollisions(world);
}

void Sandbox::PhysicsCollision2DTestScene::OnUnload() {
    World& world = GetWorld();
    auto& em = world.GetEntityManager();

    Engine::Physics2D::SetEnabled(false);

    std::unordered_set<EntityId> ids;
    ids.reserve(m_balls.size() + m_seacubes.size() + m_staticsquares.size() + 4);

    auto add_if_valid = [&](EntityId id) {
        if (id != UINT32_MAX) ids.insert(id);
        };

    // only if valid
    for (const auto& it : m_balls)    add_if_valid(ToId(it));
    for (const auto& it : m_seacubes) add_if_valid(ToId(it));
    for (EntityId id : m_staticsquares) add_if_valid(id);

    add_if_valid(m_playerId);
    add_if_valid(m_midLineId);

    // Destroy each exactly once, only if still alive
    for (EntityId id : ids) {
        if (IsAliveById(em, world, id)) {
            Entity e{ id, &world, "" };
            em.DestroyEntity(e);                // or em.DestroyEntity(id) if you have that overload
        }
    }

    // Clear containers 
    m_balls.clear();
    m_seacubes.clear();
    m_staticsquares.clear();
    m_playerId = UINT32_MAX;
    m_midLineId = UINT32_MAX;

    // Reset flags
    m_stepInit = false;
    m_dampingEnabled = false;
    m_spawnAcc = 0.0f;
    m_stepByStepMode = false;
    m_pausePhysics = false;
    m_stepRequested = false;

    Engine::Physics2D::SetEnabled(true);
}


// running test codes
void Sandbox::PhysicsCollision2DTestScene::Test_PhysicsMovement() {
    World& world = GetWorld();
    if (!m_stepInit) {
        SpawnBalls(world, 10);
        m_stepInit = true;
        m_dampingEnabled = true;
        for (auto& ball : m_balls) {
            if (auto* rb = ball.GetComponent<Component::Rigidbody2D>()) {
                rb->LinearDamping = 0.5f; // physics added
            }
        }
        std::cout << "Damping enabled after " << m_dampingDelay << " seconds!\n";
    }
    UpdateBallCollisions();
}

void Sandbox::PhysicsCollision2DTestScene::Test_CollisionDetection() {
    World& world = GetWorld();

    if (m_playerId == UINT32_MAX) {
        CreateHeroCircle();
    }

    Entity hero(m_playerId, &world);
    auto& htr = hero.Transform();
    const Vector2D oldPos = htr.Position;

    // Circle-specific movement/clamp
    ClampAndBounceCircleHero();

    if (m_staticsquares.empty()) {
        const float sideLen = 200.0f;
        const float half = sideLen * 0.5f;

        auto makeSquare = [&](const char* name, float cx, float cy) {
            Entity sq = CreateEntity(name);

            // Position
            auto& tr = sq.Transform();
            tr.Position = { cx, cy };

            // 4 points (counter-clockwise) around center 
            std::vector<Vector2D> pts;
            pts.emplace_back(cx - half, cy - half); // bottom-left
            pts.emplace_back(cx - half, cy + half); // top-left
            pts.emplace_back(cx + half, cy + half); // top-right
            pts.emplace_back(cx + half, cy - half); // bottom-right

            // Render: Polygon (filled) in red
            Component::ShapeRenderer2D shapePoly =
                Component::ShapeRenderer2D::Polygon(pts, Color(0.95f, 0.20f, 0.20f, 1.0f), /*closed*/true);
            sq.AddComponent<Component::ShapeRenderer2D>(shapePoly);

            // add a static body + box collider 
            auto& rb = sq.AddComponent<Component::Rigidbody2D>();
            rb.BodyType = Component::Rigidbody2D::Static;
            sq.AddComponent<Component::BoxCollider2D>(sideLen, sideLen);

            m_staticsquares.push_back(sq.GetId());
            };

        // Put them mid-screen so they're clearly visible
        makeSquare("StaticSquareL", m_worldWidth * 0.40f, m_worldHeight * 0.50f);
        makeSquare("StaticSquareR", m_worldWidth * 0.70f, m_worldHeight * 0.50f);

    }

    auto* hcc = hero.GetComponent<Component::CircleCollider2D>();
    auto* hrb = hero.GetComponent<Component::Rigidbody2D>();

    if (!hcc) return;

    // Apply transform scale to collision radius
    const float R = hcc->Radius * ((htr.Scale.X + htr.Scale.Y) * 0.5f);

    // Minimal push-out against one AABB. Returns true if we adjusted the hero.
    auto resolveCircleAABB = [&](const Vector2D& C,
        const Vector2D& bmin,
        const Vector2D& bmax) -> bool
        {
            // Closest point on AABB to circle center
            const Vector2D closest(
                std::max(bmin.X, std::min(C.X, bmax.X)),
                std::max(bmin.Y, std::min(C.Y, bmax.Y))
            );

            Vector2D diff = C - closest;
            float d2 = diff.Dot(diff);

            if (d2 >= R * R)
                return false; // no overlap

            // Outside case: push out along edge normal
            if (d2 > 1e-6f) {
                float d = std::sqrt(d2);
                Vector2D n = diff / d;            // outward normal from box toward circle center
                float penetration = R - d;

                htr.Position += n * penetration;  // move just enough to clear

                if (hrb) {                        // kill only inward velocity 
                    float vn = hrb->LinearVelocity.Dot(n);
                    if (vn < 0.0f) hrb->LinearVelocity -= n * vn;
                }
                std::cout << "collision detected sir" << std::endl;
                return true;
            }

            // Inside case (center inside box): push along smallest axis to exit
            float left = C.X - bmin.X;
            float right = bmax.X - C.X;
            float down = C.Y - bmin.Y;
            float up = bmax.Y - C.Y;

            Vector2D n(0, 0);
            float push = 0.0f;
            if (std::min(left, right) < std::min(down, up)) {
                if (left < right) { n = { 1, 0 }; push = left; }  // push right
                else { n = { -1, 0 }; push = right; }  // push left
            }
            else {
                if (down < up) { n = { 0, 1 }; push = down; }  // push up
                else { n = { 0,-1 }; push = up; }  // push down
            }

            htr.Position += n * (push + 0.001f);   // tiny epsilon prevents repenetration
            if (hrb) {
                float vn = hrb->LinearVelocity.Dot(n);
                if (vn < 0.0f) hrb->LinearVelocity -= n * vn;
            }
            return true;
        };

    // Use the sideLen here as in creation above
    const float sideLen = 200.0f;  // to sync with original implementationsss
    const float half = sideLen * 0.5f;

    // Resolve against both squares 
    for (EntityId eid : m_staticsquares) {
        Entity sq = world.GetEntityManager().GetEntity(eid);
        const auto& btr = sq.Transform();

        const Vector2D bmin(btr.Position.X - half, btr.Position.Y - half);
        const Vector2D bmax(btr.Position.X + half, btr.Position.Y + half);

        (void)resolveCircleAABB(htr.Position, bmin, bmax);
    }
    // here after the position correction so visuals match the new position.
}

void Sandbox::PhysicsCollision2DTestScene::Test_PhysicsHero() {
    // Ensure hero exists
    if (m_playerId == UINT32_MAX) {
        CreateTriangle();
        std::cout << "[CharacterMovement] Triangle hero spawned\n";
    }

    // Update hero movement
    ClampAndBouncePlayer();
}

void Sandbox::PhysicsCollision2DTestScene::Test_DynVStatResponse() {
    World& world = GetWorld();
    auto& em = world.GetEntityManager();
    const float dt = Time::FixedDeltaTime();

    //  Ensure two static squares tracked via m_staticsquares
    const float side = 200.0f;
    const float half = side * 0.5f;

    auto ensureSquareAt = [&](size_t slot, const char* name, float cx, float cy) {
        if (m_staticsquares.size() <= slot) m_staticsquares.resize(slot + 1, UINT32_MAX);
        EntityId& id = m_staticsquares[slot];

        auto makeSquare = [&](const char* nm) {
            Entity sq = CreateEntity(nm);
            id = sq.GetId();

            // Transform
            auto& tr = sq.Transform();
            tr.Position = { cx, cy };

            // Visual: filled square polygon (matches CollisionDetection look)
            std::vector<Vector2D> pts{
                {cx - half, cy - half}, {cx - half, cy + half},
                {cx + half, cy + half}, {cx + half, cy - half}
            };
            Component::ShapeRenderer2D shp =
                Component::ShapeRenderer2D::Polygon(pts, Color(0.95f, 0.20f, 0.20f, 1.0f), /*closed*/ true);
            sq.AddComponent<Component::ShapeRenderer2D>(shp);

            // Physics tag (static) + box collider (handy elsewhere)
            auto& rb = sq.AddComponent<Component::Rigidbody2D>();
            rb.BodyType = Component::Rigidbody2D::Static;
            sq.AddComponent<Component::BoxCollider2D>(side, side);
            };

        if (id == UINT32_MAX) {
            makeSquare(name);
            return;
        }

        Entity sq(id, &world, name);
        if (!em.IsAlive(sq)) {
            id = UINT32_MAX;
            makeSquare(name);
        }
        else {
            // keep square pinned (in case something moved it)
            sq.Transform().Position = { cx, cy };
        }
        };

    ensureSquareAt(0, "StaticSquareL", m_worldWidth * 0.40f, m_worldHeight * 0.50f);
    ensureSquareAt(1, "StaticSquareR", m_worldWidth * 0.70f, m_worldHeight * 0.50f);

    // Spawn/update your falling cubes as usual 
    SpawnCubes_T(dt);

    // Build static AABBs for the two squares (targets for sweep)
    DynCol::AABB targets[2];
    int targetCount = 0;
    for (size_t k = 0; k < 2 && k < m_staticsquares.size(); ++k) {
        EntityId id = m_staticsquares[k];
        if (id == UINT32_MAX) continue;
        Entity sq(id, &world, "sq");
        if (!em.IsAlive(sq)) continue;

        const auto& tr = sq.Transform();
        targets[targetCount++] = DynCol::AABB{
            { tr.Position.X - half, tr.Position.Y - half },
            { tr.Position.X + half, tr.Position.Y + half }
        };
    }

    //  Delete cubes whose swept AABB hits either square this frame
    for (size_t i = 0; i < m_seacubes.size(); /* no ++ here its set in loop for case types  */) {
        Entity& cube = m_seacubes[i];
        if (!em.IsAlive(cube)) {                         // drop dead handles
            m_seacubes[i] = m_seacubes.back();
            m_seacubes.pop_back();
            continue;
        }

        auto& ct = cube.Transform();
        auto* rb = cube.GetComponent<Component::Rigidbody2D>();
        auto* sh = cube.GetComponent<Component::ShapeRenderer2D>();
        if (!rb) { ++i; continue; }

        // Derive cube half-size from its polygon points (matches your update logic)
        float halfCube = 10.0f;
        if (sh && sh->Type == Component::ShapeRenderer2D::ShapeType::Polygon && sh->Points.size() >= 4) {
            float minX = sh->Points[0].X, maxX = sh->Points[0].X;
            float minY = sh->Points[0].Y, maxY = sh->Points[0].Y;
            for (const auto& p : sh->Points) {
                if (p.X < minX) minX = p.X; if (p.X > maxX) maxX = p.X;
                if (p.Y < minY) minY = p.Y; if (p.Y > maxY) maxY = p.Y;
            }
            halfCube = 0.5f * std::max(maxX - minX, maxY - minY);
        }

        // Moving AABB for the cube
        DynCol::AABB A = {
            { ct.Position.X - halfCube, ct.Position.Y - halfCube },
            { ct.Position.X + halfCube, ct.Position.Y + halfCube }
        };
        const Vector2D A_endCenter = {
            ct.Position.X + rb->LinearVelocity.X * dt,
            ct.Position.Y + rb->LinearVelocity.Y * dt
        };

        bool hit = false;
        for (int s = 0; s < targetCount && !hit; ++s) {
            // Static target end center is irrelevant to the math; pass its current center
            const Vector2D B_endCenter = {
                (targets[s].min.X + targets[s].max.X) * 0.5f,
                (targets[s].min.Y + targets[s].max.Y) * 0.5f
            };
            DynCol::SweepHit H = DynCol::Sweep(A, A_endCenter, targets[s], B_endCenter);
            if (H.hit && H.toi >= 0.0f && H.toi <= 1.0f) hit = true;
        }

        if (hit) {
            // Erase this cube (swap-with-back + pop is fine; ECS destroy if you want)
            // CubeDisintegrate(world, i);  // if you have this, use it; otherwise:
            em.DestroyEntity(cube);
            m_seacubes[i] = m_seacubes.back();
            m_seacubes.pop_back();
            continue;                     // do NOT ++i after erase
        }
        ++i; // to prevent ++ for continue cases
    }

    // --- 5) Regular maintenance (bounds etc.). Do NOT move cubes in this function.
    UpdateCubesCollisions(world);
}



void Sandbox::PhysicsCollision2DTestScene::Test_DynVDynResponse() {
    World& world = GetWorld();
    if (!m_stepInit) {
        SpawnBalls(world, 50);
        m_stepInit = true;
    }
    UpdateBallCollisions(); // walls / floor / bounds
    BallCollide();          // ball–ball
}

void Sandbox::PhysicsCollision2DTestScene::Test_StepByStepUpdate() {
    World& world = GetWorld();
    if (!m_stepInit) {
        SpawnBalls(world, 10);
        std::cout << "PhysicsCollision2DTestScene initialized with " << m_balls.size() << " balls\n";
        std::cout << "Step-by-step physics controls:\n";
        std::cout << "  P - Toggle step-by-step mode\n";
        std::cout << "  Space - Step physics (when in step mode)\n";

        if (m_playerId == UINT32_MAX) {
            CreateTriangle();
        }
        m_stepInit = true;
    }

    m_elapsedTime = static_cast<float>(Time::ElapsedTime());
    HandleStepByStepControls();

    if (!m_dampingEnabled && m_elapsedTime >= m_dampingDelay) {
        m_dampingEnabled = true;
        for (auto& ball : m_balls) {
            if (auto* rb = ball.GetComponent<Component::Rigidbody2D>()) {
                rb->LinearDamping = 0.5f;
            }
        }
        std::cout << "Damping enabled after " << m_dampingDelay << " seconds!\n";
    }
    ClampAndBouncePlayer();
}

void Sandbox::PhysicsCollision2DTestScene::CreateHeroCircle() {
    if (m_playerId != UINT32_MAX) return;

    Entity e = CreateEntity("HeroCircle");
    m_playerId = e.GetId();

    const float cx = m_worldWidth * 0.25f;
    const float cy = m_worldHeight * 0.50f;

    auto& tr = e.Transform();
    tr.Position.X = cx;
    tr.Position.Y = cy;

    auto& rb = e.AddComponent<Component::Rigidbody2D>();
    rb.Mass = 2.0f;
    rb.GravityScale = 1.0f;
    rb.LinearDamping = 0.10f;

    // Visual: circle
    auto& sh = e.AddComponent<Component::ShapeRenderer2D>();
    sh.Type = Component::ShapeRenderer2D::ShapeType::Circle;
    sh.Radius = Cradius;
    sh.FillColor = Color(0.95f, 0.90f, 0.20f, 1.0f); // same feel as triangle

    // Collider: circle
    e.AddComponent<Component::CircleCollider2D>(Cradius);
}

void Sandbox::PhysicsCollision2DTestScene::ClampAndBounceCircleHero() {
    if (m_playerId == UINT32_MAX) return;
    World& world = GetWorld();

    Entity player(m_playerId, &world);
    auto& tr = player.Transform();
    auto* rb = player.GetComponent<Rigidbody2D>();
    auto* sh = player.GetComponent<ShapeRenderer2D>();
    auto* cc = player.GetComponent<CircleCollider2D>();
    if (!rb || !cc) return;

    const float thrust = 1.0f;
    const float jumpImpulse = 20.0f;      // keep parity with triangle handler
    const float velDampScale = 0.996f;

    bool leftHeld = Input::IsKeyDown(KEY_A);
    bool rightHeld = Input::IsKeyDown(KEY_D);
    bool upHeld = Input::IsKeyDown(KEY_W);
    bool downHeld = Input::IsKeyDown(KEY_S);

    Vector2D F(0.0f, 0.0f);
    if (leftHeld)  F.X -= thrust;
    if (rightHeld) F.X += thrust;
    if (upHeld)    F.Y += thrust;
    if (downHeld)  F.Y -= thrust;

    if (F.X != 0.0f || F.Y != 0.0f) {
        // same semantics as your player handler: treat as force this frame
        Engine::Physics2D::AddForce(*rb, F);
    }

    // SPACE to jump (if you were using it before)
    if (Input::IsKeyPressed(KEY_SPACE)) {
        rb->LinearVelocity.Y += jumpImpulse;
    }

    // Light velocity damp
    rb->LinearVelocity *= velDampScale;

    // Apply transform scale to collision radius
    const float r = cc->Radius * ((tr.Scale.X + tr.Scale.Y) * 0.5f);

    // Clamp within world bounds and kill inward velocity on impact
    if (tr.Position.X - r <= 0.0f) {
        tr.Position.X = r;
        if (rb->LinearVelocity.X < 0.0f) rb->LinearVelocity.X = 0.0f;
    }
    else if (tr.Position.X + r >= m_worldWidth) {
        tr.Position.X = m_worldWidth - r;
        if (rb->LinearVelocity.X > 0.0f) rb->LinearVelocity.X = 0.0f;
    }

    if (tr.Position.Y - r <= 0.0f) {
        tr.Position.Y = r;
        if (rb->LinearVelocity.Y < 0.0f) rb->LinearVelocity.Y = 0.0f;
    }
    else if (tr.Position.Y + r >= m_worldHeight) {
        tr.Position.Y = m_worldHeight - r;
        if (rb->LinearVelocity.Y > 0.0f) rb->LinearVelocity.Y = 0.0f;
    }

    // Keep the visual in sync (in case something else changed it)
    if (sh) {
        sh->Type = Component::ShapeRenderer2D::ShapeType::Circle;
        sh->Radius = cc->Radius; // Keep visual at base radius

    }
}

void Sandbox::PhysicsCollision2DTestScene::SpawnBalls(World& world, const int count, const unsigned seed) {
    m_balls.clear();
    m_balls.reserve(count);

    for (int i = 0; i < count; ++i) {
        float radius = MathHelper::Randomize<float>(18.0f, 34.0f, seed);

        const float hue = MathHelper::Randomize<float>(0.0f, 1.0f, seed);
        const Color color(
            0.5f + 0.5f * std::cos(TWO_PI * hue + 0.0f),
            0.5f + 0.5f * std::cos(TWO_PI * hue + 2.094f),
            0.5f + 0.5f * std::cos(TWO_PI * hue + 4.188f),
            1.0f
        );

        Entity ball = world.CreateEntity("Ball");

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

        auto& rigidbody = ball.AddComponent<Component::Rigidbody2D>();
        rigidbody.Mass = 1.0f;
        rigidbody.LinearDamping = 0.0f;
        rigidbody.GravityScale = 1.0f;

        const float speed = MathHelper::Randomize<float>(200.0f, 300.0f, seed);
        const float angle = MathHelper::Randomize<float>(0.0f, TWO_PI, seed);
        rigidbody.LinearVelocity.X = std::cos(angle) * speed;
        rigidbody.LinearVelocity.Y = std::sin(angle) * speed;

        auto& shapeRenderer = ball.AddComponent<Component::ShapeRenderer2D>();
        shapeRenderer.Type = Component::ShapeRenderer2D::ShapeType::Circle;
        shapeRenderer.Radius = radius;
        shapeRenderer.FillColor = color;

        ball.AddComponent<Component::CircleCollider2D>(radius);

        m_balls.push_back(ball);
        std::cout << "Created ball (" << i + 1 << ") with ENT ID " << ball.GetId()
            << " at (" << x << ", " << y << ") with radius " << radius << '\n';
    }
}

void Sandbox::PhysicsCollision2DTestScene::SpawnCubes() {
    if ((int)m_seacubes.size() >= m_maxLeaves) return;

    const float size = MathHelper::Randomize<float>(14.0f, 28.0f, 0);
    const float half = size * 0.5f;
    const float x = MathHelper::Randomize<float>(half, m_worldWidth - half, 0);
    const float y = m_worldHeight - half - 2.0f;

    Entity cube = CreateEntity("Cube");

    auto& tr = cube.Transform();
    tr.Position.X = x;
    tr.Position.Y = y;

    // Rigidbody (dynamic so gravity applies)
    auto& rb = cube.AddComponent<Rigidbody2D>();
    rb.BodyType = Component::Rigidbody2D::Dynamic;
    rb.Mass = 0.8f;
    rb.GravityScale = 1.0f;
    rb.LinearDamping = 0.25f;
    rb.LinearVelocity.X = MathHelper::Randomize<float>(-20.0f, 20.0f, 0);
    rb.LinearVelocity.Y = -5.0f;

    cube.AddComponent<Component::CircleCollider2D>(size);


    std::vector<Vector2D> pts;
    pts.emplace_back(x - half, y - half);
    pts.emplace_back(x - half, y + half);
    pts.emplace_back(x + half, y + half);
    pts.emplace_back(x + half, y - half);

    auto& shape = cube.AddComponent<ShapeRenderer2D>();
    shape.Type = ShapeRenderer2D::ShapeType::Polygon;
    shape.Points = pts;
    shape.Closed = true;
    shape.FillColor = Color(0.15f, 0.65f, 0.95f, 1.0f);

    m_seacubes.push_back(cube);
}

void Sandbox::PhysicsCollision2DTestScene::SpawnCubes_T(float dt) {
    m_spawnAcc += dt;
    while (m_spawnAcc >= m_spawnIntervals) {
        m_spawnAcc -= m_spawnIntervals;
        int batch = MathHelper::Randomize<int>(1, 7, 0);
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

    Entity hero(m_playerId, &world);
    auto& ht = hero.Transform();

    // Apply transform scale to collision radius
    const float heroR = m_triHalfHeight * ((ht.Scale.X + ht.Scale.Y) * 0.5f);
    for (size_t i = 0; i < m_seacubes.size();) {
        auto& ct = m_seacubes[i].Transform();
        auto* rb = m_seacubes[i].GetComponent<Rigidbody2D>();

        float half;
        if (auto* sh = m_seacubes[i].GetComponent<ShapeRenderer2D>()) {
            if (sh->Type == ShapeRenderer2D::ShapeType::Polygon && sh->Points.size() >= 4) {
                float minX = sh->Points[0].X, maxX = sh->Points[0].X;
                float minY = sh->Points[0].Y, maxY = sh->Points[0].Y;
                for (auto& p : sh->Points) {
                    if (p.X < minX) minX = p.X; if (p.X > maxX) maxX = p.X;
                    if (p.Y < minY) minY = p.Y; if (p.Y > maxY) maxY = p.Y;
                }
                half = 0.5f * std::max(maxX - minX, maxY - minY);
                const float x = ct.Position.X, y = ct.Position.Y;
                sh->Points.clear();
                sh->Points.emplace_back(x - half, y - half);
                sh->Points.emplace_back(x - half, y + half);
                sh->Points.emplace_back(x + half, y + half);
                sh->Points.emplace_back(x + half, y - half);
                sh->Closed = true;
            }
            else {
                half = 10.0f;
            }
        }
        else {
            half = 10.0f;
        }


        if (m_currentTest == TestType::test_DynvStatResponse) {
            if (ct.Position.Y - half <= m_worldHeight / 3.0f) {
                CubeDisintegrate(world, i);
                continue;
            }
        }
        else if (m_currentTest == TestType::test_StepbyStepUpdate) {
            if (ct.Position.Y - half <= 0) {
                CubeDisintegrate(world, i);
                continue;
            }
        }
        else {
            CubeDisintegrate(world, i);
            continue;
        }
        DynCol::Circle Chero{ Vector2D(ht.Position.X, ht.Position.Y), heroR };
        DynCol::AABB   Bcube{ Vector2D(ct.Position.X - half, ct.Position.Y - half),
                              Vector2D(ct.Position.X + half, ct.Position.Y + half) };

        if (DynCol::Overlap(Chero, Bcube, nullptr)) {
            CubeDisintegrate(world, i);
            continue;
        }

        ++i;
        (void)rb;
    }
}

void Sandbox::PhysicsCollision2DTestScene::CreateTriangle() {
    if (m_playerId != UINT32_MAX) return;
    Entity e = CreateEntity("Triangle");
    m_playerId = e.GetId();

    const float cx = m_worldWidth * 0.25f;
    const float cy = m_worldHeight * 0.50f;

    auto& tr = e.Transform();
    tr.Position.X = cx;
    tr.Position.Y = cy;

    auto& rb = e.AddComponent<Component::Rigidbody2D>();
    rb.Mass = 2.0f;
    rb.GravityScale = 1.0f;
    rb.LinearDamping = 0.10f;

    e.AddComponent<Component::CircleCollider2D>(m_triHalfHeight);

    rb.LinearVelocity.X = 50.0f;
    rb.LinearVelocity.Y = 0.0f;

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

    Entity player(m_playerId, &world);
    auto& tr = player.Transform();
    auto* rb = player.GetComponent<Rigidbody2D>();
    auto* shape = player.GetComponent<ShapeRenderer2D>();
    if (!rb) return;

    const float thrust = 1.0f;
    const float jumpImpulse = 20.0f;
    const float velDampScale = 0.996f;

    bool leftHeld = Input::IsKeyDown(KEY_A);
    bool rightHeld = Input::IsKeyDown(KEY_D);
    bool upHeld = Input::IsKeyDown(KEY_W);
    bool downHeld = Input::IsKeyDown(KEY_S);

    Vector2D F(0.0f, 0.0f);
    if (leftHeld)  F.X -= thrust;
    if (rightHeld) F.X += thrust;
    if (upHeld)    F.Y += thrust;
    if (downHeld)  F.Y -= thrust;

    if (F.X != 0.0f || F.Y != 0.0f) {
        Engine::Physics2D::AddForce(*rb, F);
    }

    if (Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
        Engine::Physics2D::AddImpulse(*rb, Vector2D(0.0f, jumpImpulse));
        std::cout << jumpImpulse;
    }
    rb->LinearVelocity *= velDampScale;

    // Apply transform scale to collision radius
    const float r = m_triHalfHeight * ((tr.Scale.X + tr.Scale.Y) * 0.5f);

    if (tr.Position.X - r <= 0.0f) {
        tr.Position.X = r;
        if (rb->LinearVelocity.X < 0.0f) rb->LinearVelocity.X = 0.0f;
    }
    else if (tr.Position.X + r >= m_worldWidth) {
        tr.Position.X = m_worldWidth - r;
        if (rb->LinearVelocity.X > 0.0f) rb->LinearVelocity.X = 0.0f;
    }

    if (tr.Position.Y - r <= 0.0f) {
        tr.Position.Y = r;
        if (rb->LinearVelocity.Y < 0.0f) rb->LinearVelocity.Y = 0.0f;
    }
    else if (tr.Position.Y + r >= m_worldHeight) {
        tr.Position.Y = m_worldHeight - r;
        if (rb->LinearVelocity.Y > 0.0f) rb->LinearVelocity.Y = 0.0f;
    }

    std::vector<Vector2D> pts;
    pts.emplace_back(tr.Position.X + 0.0f, tr.Position.Y + m_triHalfHeight);
    pts.emplace_back(tr.Position.X - m_triHalfBase, tr.Position.Y - m_triHalfHeight);
    pts.emplace_back(tr.Position.X + m_triHalfBase, tr.Position.Y - m_triHalfHeight);

    shape->Type = ShapeRenderer2D::ShapeType::Polygon;
    shape->Points = pts;
    shape->Closed = true;
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
            StoreBallStates();
        }
        else {
            std::cout << "Step-by-step physics mode DISABLED" << '\n';
            RestoreBallStates();
        }
    }

    // step physics with SPACE key
    if ((m_stepByStepMode || m_pausePhysics) && spaceIsDown && !spaceWasDown) {
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

void Sandbox::PhysicsCollision2DTestScene::BallCollide() {
    World& world = GetWorld();
    auto& em = world.GetEntityManager();

  

    const size_t n = m_balls.size();
    for (size_t i = 0; i < n; ++i) {
        Entity& a = m_balls[i];
        if (!em.IsAlive(a)) continue;

        auto* rba = a.GetComponent<Component::Rigidbody2D>();
        auto* cca = a.GetComponent<Component::CircleCollider2D>();
        if (!rba || !cca) continue;

        auto& ta = a.Transform();
        // Apply transform scale to collision radius
        const float ra = cca->Radius * ((ta.Scale.X + ta.Scale.Y) * 0.5f);
        const float invMa = (rba->Mass > 0.0f) ? 1.0f / rba->Mass : 0.0f;

        for (size_t j = i + 1; j < n; ++j) {
            Entity& b = m_balls[j];
            if (!em.IsAlive(b)) continue;

            auto* rbb = b.GetComponent<Component::Rigidbody2D>();
            auto* ccb = b.GetComponent<Component::CircleCollider2D>();
            if (!rbb || !ccb) continue;

            auto& tb = b.Transform();
            // Apply transform scale to collision radius
            const float rb = ccb->Radius * ((tb.Scale.X + tb.Scale.Y) * 0.5f);
            const float invMb = (rbb->Mass > 0.0f) ? 1.0f / rbb->Mass : 0.0f;

            // Skip if both are immovable
            const float invMassSum = invMa + invMb;
            if (invMassSum == 0.0f) continue;

            DynCol::Circle A{ ta.Position, ra };
            DynCol::Circle B{ tb.Position, rb };
            DynCol::Manifold m{};

            if (!DynCol::Overlap(A, B, &m) || !m.valid) continue;
            {
                const Vector2D va = rba->LinearVelocity;
                const Vector2D vb = rbb->LinearVelocity;
                const Vector2D rv = vb - va;

                const float velN = rv.X * m.normal.X + rv.Y * m.normal.Y;
                if (velN > 0.0f) continue; // already separating along normal

                const float e = std::clamp(restitution, 0.0f, 1.0f);
                const float z = -(1.0f + e) * velN / invMassSum;
                const Vector2D J = m.normal * z;

                rba->LinearVelocity -= J * invMa;
                rbb->LinearVelocity += J * invMb;
            }

            // friction
            if (friction > 0.0f) {
                // Recompute relative velocity after normal impulse
                const Vector2D rv2 = rbb->LinearVelocity - rba->LinearVelocity;

                const float vn2 = rv2.X * m.normal.X + rv2.Y * m.normal.Y;
                Vector2D t = { rv2.X - m.normal.X * vn2, rv2.Y - m.normal.Y * vn2 };
                const float tlen2 = t.X * t.X + t.Y * t.Y;

                if (tlen2 > 1e-12f) {
                    t = t / std::sqrt(tlen2);

                    const float jt = -(rv2.X * t.X + rv2.Y * t.Y) / invMassSum;
                    const float mu = std::max(0.0f, friction);

                    // Use normal impulse magnitude from above as scale; recompute cheapest way:
                    // normal impulse magnitude equals |(vb-va)·n|*(1+e)/invMassSum for this contact.
                    const Vector2D va = rba->LinearVelocity;
                    const Vector2D vb = rbb->LinearVelocity;
                    const float velN_after = (vb.X - va.X) * m.normal.X + (vb.Y - va.Y) * m.normal.Y;
                    const float jn_mag = std::abs(velN_after) / invMassSum; // sufficient for Coulomb clamp

                    Vector2D Jt;
                    if (std::abs(jt) < jn_mag * mu) {
                        // static friction
                        Jt = t * jt;
                    }
                    else {
                        // dynamic friction
                        Jt = t * (-jn_mag * mu * (jt < 0 ? -1.0f : 1.0f));
                    }

                    rba->LinearVelocity -= Jt * invMa;
                    rbb->LinearVelocity += Jt * invMb;
                }
            }
        }
    }
}

void Sandbox::PhysicsCollision2DTestScene::UpdateBallCollisions() {
    World& world = GetWorld();
    std::vector<EntityId> allEntities = world.GetEntityManager().GetAllEntities();

    for (EntityId id : allEntities) {
        Entity ball = world.GetEntityManager().GetEntity(id);
        auto* rigidbody = ball.GetComponent<Component::Rigidbody2D>();
        const auto* circleCollider = ball.GetComponent<Component::CircleCollider2D>();
        auto& transform = ball.Transform();

        if (!rigidbody || !circleCollider)
            continue;

        // Apply transform scale to collision radius
        const float radius = circleCollider->Radius * ((transform.Scale.X + transform.Scale.Y) * 0.5f);

        bool bounced = false;

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

        (void)bounced;
    }
}
