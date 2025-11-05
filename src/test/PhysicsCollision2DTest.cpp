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

//#include "PhysicsCollision2DTest.h"
//#include "core/Application.h"
//#include "core/Logger.h"
//#include "ecs/Components.h"
//#include "ecs/Entity.h"
//#include "ecs/World.h"
//#include "ecs/systems/LifetimeSystem.h"
//#include "ecs/systems/PhysicsSystem.h"
//#include "helpers/MathUtils.h"
//#include "helpers/EntityUtils.h"
//#include "physics/DynamicCollision.h"
//#include "physics/Physics.h"
//#include "services/Input.h"
//#include "services/Time.h"
//#include "services/Window.h"
//#include "services/WindowManager.h"
//#include <algorithm>
//#include <cmath>
//#include <iostream>
//
//using namespace ECS;
//namespace {
//    constexpr float TWO_PI = 6.28318530718f;
//
//    // Helper to check if entity is alive
//    bool IsEntityAlive(const World& world, const uint64_t packedEntityId) {
//        if (packedEntityId == UINT64_MAX)
//            return false;
//        const Entity entity = EntityUtils::Unpack(packedEntityId);
//
//        return world.IsAlive(entity);
//    }
//
//    // Helper to find bounding box of points
//    void GetPointBounds(const std::vector<Vector2D>& points, float& minX, float& maxX, float& minY, float& maxY) {
//        if (points.empty())
//            return;
//
//        minX = maxX = points[0].X;
//        minY = maxY = points[0].Y;
//
//        for (const auto& point : points) {
//            if (point.X < minX) minX = point.X;
//            if (point.X > maxX) maxX = point.X;
//            if (point.Y < minY) minY = point.Y;
//            if (point.Y > maxY) maxY = point.Y;
//        }
//    }
//
//    // Helper to create square points around center
//    std::vector<Vector2D> CreateSquarePoints(const float centerX, const float centerY, const float halfSize) {
//        return {
//            { centerX - halfSize, centerY - halfSize },  // bottom-left
//            { centerX - halfSize, centerY + halfSize },  // top-left
//            { centerX + halfSize, centerY + halfSize },  // top-right
//            { centerX + halfSize, centerY - halfSize }   // bottom-right
//        };
//    }
//}
//
//// ============================================================================
//// Constructor & Lifecycle
//// ============================================================================
//
//void Sandbox::PhysicsCollision2DTestScene::OnLoad() {
//    // Load config here, although it should be loaded in Game.cpp to create the window
//    // But we put it here as user input is needed first
//    const auto& config = Engine::CORE->GetConfig();
//    const int windowWidth = config.WindowConfig.Width;
//    const int windowHeight = config.WindowConfig.Height;
//
//    CREATE_WINDOW("Physics & Collision Test", windowWidth, windowHeight);
//    m_worldWidth = static_cast<float>(windowWidth);
//    m_worldHeight = static_cast<float>(windowHeight);
//    m_elapsedTime = 0.0f;
//    m_dampingDelay = 7.f;
//    m_dampingEnabled = false;
//
//    // step-by-step physics mode initialization
//    m_stepByStepMode = false;
//    m_stepRequested = false;
//    m_pausePhysics = false;
//
//    // set gravity
//    Engine::Physics::SetGravity(Vector2D(0.0f, -15.0f));
//
//    // setup gameplay layer
//    m_gameplayLayer = GetLayers().CreateOrGetLayer("gameplay");
//
//    AddSystem([](Scenes::Scene& s, const float dt) {
//        LifetimeSystem::Update(s.GetWorld(), dt);
//        }, "Lifetime System");
//
//    AddSystem([](Scenes::Scene& s, const float dt) {
//        PhysicsSystem::Update(s.GetWorld(), dt);
//        }, "Physics System");
//
//    // TODO: Add renderer system
//}
//
//void Sandbox::PhysicsCollision2DTestScene::OnUpdate() {
//    // Handle test cycling with C key
//    if (Input::IsKeyDown(KEY_C)) {
//        if (!m_testChangeRequested) {
//            int currentIndex = static_cast<int>(m_currentTest);
//            currentIndex = (currentIndex >= static_cast<int>(TestType::StepByStepUpdate))
//                ? static_cast<int>(TestType::PhysicsMovement)
//                : currentIndex + 1;
//            m_currentTest = static_cast<TestType>(currentIndex);
//
//            LOG_DEBUG("Switched to physics test " << currentIndex);
//            _clearEntities();
//            _resetFlagsAndVariables();
//            m_testChangeRequested = true;
//        }
//    }
//    else {
//        m_testChangeRequested = false;
//    }
//
//    // Dispatch to active test
//    switch (m_currentTest) {
//    case TestType::PhysicsMovement:           _testPhysicsMovement();           break;
//    case TestType::CollisionDetection:        _testCollisionDetection();        break;
//    case TestType::PhysicsHero:               _testPhysicsHero();               break;
//    case TestType::DynamicVsStaticResponse:   _testDynamicVsStaticResponse();   break;
//    case TestType::DynamicVsDynamicResponse:  _testDynamicVsDynamicResponse();  break;
//    case TestType::StepByStepUpdate:          _testStepByStepUpdate();          break;
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::OnFixedUpdate() {
//    if (m_currentTest != TestType::StepByStepUpdate) return;
//
//    const float deltaTime = Time::FixedDeltaTime();
//
//    if (!m_pausePhysics || m_stepRequested) {
//        _updateBallBoundaryCollisions();
//        if (m_stepRequested) {
//            Engine::Physics::SetEnabled(false);
//        }
//        m_stepRequested = false;
//    }
//
//    _spawnCubesBatch(deltaTime);
//    _updateCubeCollisions(GetWorld());
//}
//
//void Sandbox::PhysicsCollision2DTestScene::OnUnload() {
//    _clearEntities();
//    _resetFlagsAndVariables();
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_clearEntities() {
//    World& world = GetWorld();
//
//    for (const auto packedEntity : m_balls) {
//        if (IsEntityAlive(world, packedEntity)) {
//            const Entity entity = EntityUtils::Unpack(packedEntity);
//            world.Destroy(entity);
//        }
//    }
//    m_balls.clear();
//
//    for (const auto packedEntity : m_cubes) {
//        if (IsEntityAlive(world, packedEntity)) {
//            const Entity entity = EntityUtils::Unpack(packedEntity);
//            world.Destroy(entity);
//        }
//    }
//    m_cubes.clear();
//
//    for (const auto packedEntity : m_staticSquares) {
//        if (IsEntityAlive(world, packedEntity)) {
//            const Entity entity = EntityUtils::Unpack(packedEntity);
//            world.Destroy(entity);
//        }
//    }
//    m_staticSquares.clear();
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_resetFlagsAndVariables() {
//    m_heroId = UINT64_MAX;
//    m_midLineId = UINT64_MAX;
//
//    m_testInitialized = false;
//    m_dampingEnabled = false;
//    m_cubeSpawnAccumulator = 0.0f;
//    m_stepByStepMode = false;
//    m_pausePhysics = false;
//    m_stepRequested = false;
//
//    m_elapsedTime = 0.0f;
//    m_dampingDelay = 7.f;
//
//    // set gravity
//    Engine::Physics::SetGravity(Vector2D(0.0f, -15.0f));
//
//    Engine::Physics::SetEnabled(true);
//}
//
//// ============================================================================
//// Test Implementations
//// ============================================================================
//
//void Sandbox::PhysicsCollision2DTestScene::_testPhysicsMovement() {
//    if (!m_testInitialized) {
//        _spawnBalls(GetWorld(), 10);
//        m_testInitialized = true;
//    }
//
//    m_elapsedTime = static_cast<float>(Time::ElapsedTime());
//    if (!m_dampingEnabled && m_elapsedTime >= m_dampingDelay) {
//        m_dampingEnabled = true;
//        World& world = GetWorld();
//        for (const auto packedBall : m_balls) {
//            if (IsEntityAlive(world, packedBall)) {
//                const Entity ball = EntityUtils::Unpack(packedBall);
//
//                if (!world.Has<Components::Rigidbody2D>(ball))
//                    return;
//
//                auto& rigidbody = world.Get<Components::Rigidbody2D>(ball);
//                rigidbody.LinearDamping = BALL_DAMPING_VALUE;
//            }
//        }
//        LOG_DEBUG("Damping enabled after " << m_dampingDelay << " seconds");
//    }
//
//    _updateBallBoundaryCollisions();
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_testCollisionDetection() {
//    World& world = GetWorld();
//
//    if (m_heroId == UINT64_MAX)
//        _createHeroCircle();
//    _ensureStaticSquares();
//
//    if (!IsEntityAlive(world, m_heroId))
//        return;
//
//    const Entity hero = EntityUtils::Unpack(m_heroId);
//
//    if (!world.Has<Components::LocalTransform>(hero) ||
//        !world.Has<Components::LinearVelocity2D>(hero) ||
//        !world.Has<Components::CircleCollider2D>(hero))
//        return;
//
//    auto& heroTransform = world.Get<Components::LocalTransform>(hero);
//    auto& heroLinearVelocity = world.Get<Components::LinearVelocity2D>(hero);
//    const auto& heroCollider = world.Get<Components::CircleCollider2D>(hero);
//
//    _updateHeroMovement(HERO_CIRCLE_RADIUS);
//
//    const float heroRadius = _getScaledRadius(heroCollider.Radius, Vector2D(heroTransform.Scale.X, heroTransform.Scale.Y));
//    constexpr float halfSquare = STATIC_SQUARE_SIDE_LENGTH * 0.5f;
//
//    for (const uint64_t packedSquare : m_staticSquares) {
//        if (!IsEntityAlive(world, packedSquare))
//            continue;
//
//        const Entity square = EntityUtils::Unpack(packedSquare);
//        if (!world.Has<Components::LocalTransform>(square))
//			continue;
//
//        auto& squareTransform = world.Get<Components::LocalTransform>(square);
//
//
//        _resolveCircleAABBCollision(
//            heroTransform.Position,
//            heroLinearVelocity.Value,
//            Vector2D(squareTransform.Position.X - halfSquare, squareTransform.Position.Y - halfSquare),
//            Vector2D(squareTransform.Position.X + halfSquare, squareTransform.Position.Y + halfSquare),
//            heroRadius
//        );
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_testPhysicsHero() {
//    if (m_heroId == UINT64_MAX) {
//        _createHeroTriangle();
//        std::cout << "[PhysicsHero] Triangle hero spawned\n";
//    }
//    _updateHeroMovement(HERO_TRIANGLE_HALF_HEIGHT);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_testDynamicVsStaticResponse() {
//    World& world = GetWorld();
//    const float deltaTime = Time::FixedDeltaTime();
//
//    _ensureStaticSquares();
//    _spawnCubesBatch(deltaTime);
//
//    // Build AABB targets for sweep tests
//    DynCol::AABB targets[2];
//    const int targetCount = _buildStaticSquareAABBs(world, targets, 2);
//
//    // Test cubes for collision with static squares
//    for (size_t i = 0; i < m_cubes.size(); ) {
//        const uint64_t packedCube = m_cubes[i];
//
//        if (!IsEntityAlive(world, packedCube)) {
//            _removeCubeAtIndex(i);
//            continue;
//        }
//
//        const Entity cube = EntityUtils::Unpack(packedCube);
//
//        if (!world.Has<Components::LocalTransform>(cube) ||
//            !world.Has<Components::Rigidbody2D>(cube) ||
//            !world.Has<Components::BoxCollider2D>(cube)) {
//            ++i;
//            continue;
//        }
//
//        auto& cubeTransform = world.Get<Components::LocalTransform>(cube);
//        auto& cubeLinearVelocity = world.Get<Components::LinearVelocity2D>(cube);
//        auto& cubeShape = world.Get<Components::ShapeBox2D>(cube);
//
//        const float cubeHalfSize = _calculateCubeHalfSize(cubeShape);
//        const Vector2D cubeEndCenter = MathUtils::ToVector2D(cubeTransform.Position) + cubeLinearVelocity.Value * deltaTime;
//
//        if (_checkSweptAABBCollision(cubeTransform.Position, cubeEndCenter, cubeHalfSize, targets, targetCount, deltaTime)) {
//            world.Destroy(cube);
//            _removeCubeAtIndex(i);
//            continue;
//        }
//
//        ++i;
//    }
//
//    _updateCubeCollisions(world);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_testDynamicVsDynamicResponse() {
//    if (!m_testInitialized) {
//        _spawnBalls(GetWorld(), 50);
//        m_testInitialized = true;
//    }
//    _updateBallBoundaryCollisions();
//    _updateBallToBallCollisions();
//}
//
//// ============================================================================
//// Entity Creation
//// ============================================================================
//
//void Sandbox::PhysicsCollision2DTestScene::_createHero(const bool isCircle) {
//    if (m_heroId != UINT64_MAX) return;
//
//    World& world = GetWorld();
//    const char* name = isCircle ? "HeroCircle" : "HeroTriangle";
//    const Vector2D startPos(m_worldWidth * 0.25f, m_worldHeight * 0.50f);
//    const float radius = isCircle ? HERO_CIRCLE_RADIUS : HERO_TRIANGLE_HALF_HEIGHT;
//    const Color heroColor(0.95f, 0.90f, 0.20f, 1.0f);
//
//    Entity hero = CreateOnLayer(
//        m_gameplayLayer,
//        Components::LocalTransform{
//        	Vector3D{
//                startPos.X,
//                startPos.Y,
//                0
//        	},
//            Quaternion{},
//            Vector3D{ 1.f, 1.f, 1.f }
//        },
//        Components::WorldTransform{},
//        Components::Name{ *name },
//        Components::LinearVelocity2D{},
//        Components::Rigidbody2D{}
//    )
//
//    // Create rigidbody component
//    Rigidbody2D rigidbody;
//    rigidbody.Mass = 2.0f;
//    rigidbody.GravityScale = 1.0f;
//    rigidbody.LinearDamping = 0.10f;
//    if (!isCircle) rigidbody.LinearVelocity.X = 50.0f;
//
//    // Create collider component
//    CircleCollider2D collider;
//    collider.Radius = radius;
//
//    // Create shape renderer component
//    ShapeRenderer2D shapeRenderer;
//    if (isCircle) {
//        shapeRenderer.Type = ShapeRenderer2D::ShapeType::Circle;
//        shapeRenderer.Radius = radius;
//        shapeRenderer.FillColor = heroColor;
//    }
//    else {
//        std::vector<Vector2D> trianglePoints{
//            {startPos.X, startPos.Y + HERO_TRIANGLE_HALF_HEIGHT},
//            {startPos.X - HERO_TRIANGLE_HALF_BASE, startPos.Y - HERO_TRIANGLE_HALF_HEIGHT},
//            {startPos.X + HERO_TRIANGLE_HALF_BASE, startPos.Y - HERO_TRIANGLE_HALF_HEIGHT}
//        };
//        shapeRenderer = ShapeRenderer2D::Polygon(trianglePoints, heroColor, true);
//    }
//
//    // Create entity with all components
//    Entity hero = world.Create(
//        LocalTransform{ Vector3D{startPos.X, startPos.Y, 0.0f}, Quaternion{0.0f, 0.0f, 0.0f, 1.0f}, Vector3D{1.0f, 1.0f, 1.0f} },
//        Components::Name{ name },
//        rigidbody,
//        collider,
//        shapeRenderer
//    );
//
//    m_heroId = EntityUtils::Pack(hero);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_createHeroCircle() {
//    _createHero(true);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_createHeroTriangle() {
//    _createHero(false);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_createStaticSquare(const char* name, const float centerX, const float centerY) {
//    constexpr float halfSize = STATIC_SQUARE_SIDE_LENGTH * 0.5f;
//
//    const Entity square = CreateOnLayer(
//        m_gameplayLayer,
//        Components::LocalTransform{
//            Vector3D{
//                centerX,
//                centerY,
//                0
//            },
//            Quaternion{},
//            Vector3D{ 1.f, 1.f, 1.f }
//        },
//        Components::WorldTransform{},
//        Components::Rigidbody2D{},
//        Components::LinearVelocity2D{},
//        Components::BoxCollider2D{{ halfSize, halfSize }},
//        Components::ShapeBox2D{
//			Vector2D{ halfSize, halfSize },
//            Vector2D{},
//            Color{0.95f, 0.2f, 0.2f, 1.f },
//            1.f,
//            true
//        },
//        Components::Name{ *name }
//    );
//
//    m_staticSquares.push_back(EntityUtils::Pack(square));
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_ensureStaticSquares() {
//    World& world = GetWorld();
//
//    const std::pair<const char*, Vector2D> squareConfigs[] = {
//        {"StaticSquareL", {m_worldWidth * 0.40f, m_worldHeight * 0.50f}},
//        {"StaticSquareR", {m_worldWidth * 0.70f, m_worldHeight * 0.50f}}
//    };
//
//    for (size_t i = 0; i < 2; ++i) {
//        if (m_staticSquares.size() <= i) m_staticSquares.resize(i + 1, UINT64_MAX);
//
//        uint64_t& packedSquareId = m_staticSquares[i];
//        const auto& [name, pos] = squareConfigs[i];
//
//        if (!IsEntityAlive(world, packedSquareId)) {
//            _createStaticSquare(name, pos.X, pos.Y);
//        }
//        else {
//            Entity square = EntityUtils::Unpack(packedSquareId);
//            auto* transform = world.Get<LocalTransform>(square);
//            if (transform) {
//                transform->Position = Vector3D{ pos.X, pos.Y, 0.0f };
//            }
//        }
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_spawnBalls(World& world, const int count, const unsigned seed) {
//    m_balls.clear();
//    m_balls.reserve(count);
//
//    for (int i = 0; i < count; ++i) {
//        const float radius = MathHelper::Randomize<float>(18.0f, 34.0f, seed);
//        const float hue = MathHelper::Randomize<float>(0.0f, 1.0f, seed);
//
//        const Color ballColor(
//            0.5f + 0.5f * std::cos(TWO_PI * hue),
//            0.5f + 0.5f * std::cos(TWO_PI * hue + 2.094f),
//            0.5f + 0.5f * std::cos(TWO_PI * hue + 4.188f),
//            1.0f
//        );
//
//        const float posX = MathHelper::Randomize<float>(radius, m_worldWidth - radius, seed);
//        const float posY = MathHelper::Randomize<float>(radius, m_worldHeight - radius, seed);
//
//        const float speed = MathHelper::Randomize<float>(200.0f, 300.0f, seed);
//        const float angle = MathHelper::Randomize<float>(0.0f, TWO_PI, seed);
//
//        // Create rigidbody component
//        Rigidbody2D rigidbody;
//        rigidbody.Mass = 1.0f;
//        rigidbody.LinearDamping = 0.0f;
//        rigidbody.GravityScale = 1.0f;
//        rigidbody.LinearVelocity = { std::cos(angle) * speed, std::sin(angle) * speed };
//
//        // Create shape renderer component
//        ShapeRenderer2D shapeRenderer;
//        shapeRenderer.Type = ShapeRenderer2D::ShapeType::Circle;
//        shapeRenderer.Radius = radius;
//        shapeRenderer.FillColor = ballColor;
//
//        // Create collider component
//        CircleCollider2D collider;
//        collider.Radius = radius;
//
//        // Create entity with all components
//        Entity ball = world.Create(
//            LocalTransform{ Vector3D{posX, posY, 0.0f}, Quaternion{0.0f, 0.0f, 0.0f, 1.0f}, Vector3D{1.0f, 1.0f, 1.0f} },
//            Components::Name{ "Ball" },
//            rigidbody,
//            shapeRenderer,
//            collider
//        );
//
//        m_balls.push_back(EntityUtils::Pack(ball));
//
//        std::cout << "Created ball (" << i + 1 << ") at (" << posX << ", " << posY << ") with radius " << radius << '\n';
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_spawnCube() {
//    if (static_cast<int>(m_cubes.size()) >= MAX_CUBES) return;
//
//    World& world = GetWorld();
//    const float size = MathHelper::Randomize<float>(14.0f, 28.0f, 0);
//    const float halfSize = size * 0.5f;
//    const float posX = MathHelper::Randomize<float>(halfSize, m_worldWidth - halfSize, 0);
//    const float posY = m_worldHeight - halfSize - 2.0f;
//
//    // Create rigidbody component
//    Rigidbody2D rigidbody;
//    rigidbody.BodyType = Rigidbody2D::Dynamic;
//    rigidbody.Mass = 0.8f;
//    rigidbody.GravityScale = 1.0f;
//    rigidbody.LinearDamping = 0.25f;
//    rigidbody.LinearVelocity = { MathHelper::Randomize<float>(-20.0f, 20.0f, 0), -5.0f };
//
//    // Create collider component
//    CircleCollider2D collider;
//    collider.Radius = size;
//
//    // Create shape renderer component
//    ShapeRenderer2D shapeRenderer;
//    shapeRenderer.Type = ShapeRenderer2D::ShapeType::Polygon;
//    shapeRenderer.Points = CreateSquarePoints(posX, posY, halfSize);
//    shapeRenderer.Closed = true;
//    shapeRenderer.FillColor = Color(0.15f, 0.65f, 0.95f, 1.0f);
//
//    // Create entity with all components
//    Entity cube = world.Create(
//        LocalTransform{ Vector3D{posX, posY, 0.0f}, Quaternion{0.0f, 0.0f, 0.0f, 1.0f}, Vector3D{1.0f, 1.0f, 1.0f} },
//        Components::Name{ "Cube" },
//        rigidbody,
//        collider,
//        shapeRenderer
//    );
//
//    m_cubes.push_back(EntityUtils::Pack(cube));
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_spawnCubesBatch(float deltaTime) {
//    m_cubeSpawnAccumulator += deltaTime;
//
//    while (m_cubeSpawnAccumulator >= CUBE_SPAWN_INTERVAL) {
//        m_cubeSpawnAccumulator -= CUBE_SPAWN_INTERVAL;
//        const int batchSize = MathUtils::Randomize<int>(1, 7, 0);
//        for (int i = 0; i < batchSize; ++i) _spawnCube();
//    }
//}
//
//// ============================================================================
//// Collision & Physics Updates
//// ============================================================================
//
//void Sandbox::PhysicsCollision2DTestScene::_updateBallBoundaryCollisions() {
//    World& world = GetWorld();
//
//    for (uint64_t packedBall : m_balls) {
//        if (!IsEntityAlive(world, packedBall)) continue;
//
//        Entity ball = EntityUtils::Unpack(packedBall);
//        auto* rigidbody = world.Get<Rigidbody2D>(ball);
//        const auto* circleCollider = world.Get<CircleCollider2D>(ball);
//        auto* transform = world.Get<LocalTransform>(ball);
//
//        if (!rigidbody || !circleCollider || !transform) continue;
//
//        const float radius = _getScaledRadius(circleCollider->Radius, Vector2D(transform->Scale.X, transform->Scale.Y));
//
//        Vector2D position(transform->Position.X, transform->Position.Y);
//        Engine::Physics::BoundaryConstraint bounds{ 0.0f, m_worldWidth, 0.0f, m_worldHeight, true };
//        Engine::Physics::ApplyBoundaryConstraint(position, rigidbody->LinearVelocity, radius, bounds);
//        transform->Position.X = position.X;
//        transform->Position.Y = position.Y;
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_updateBallToBallCollisions() {
//    World& world = GetWorld();
//    const size_t ballCount = m_balls.size();
//
//    Engine::Physics::CollisionParams collisionParams{
//        m_restitution,
//        m_friction,
//        0.5f  // position correction percent
//    };
//
//    for (size_t i = 0; i < ballCount; ++i) {
//        uint64_t packedBallA = m_balls[i];
//        if (!IsEntityAlive(world, packedBallA)) continue;
//
//        Entity ballA = EntityUtils::Unpack(packedBallA);
//        auto* rigidbodyA = world.Get<Rigidbody2D>(ballA);
//        auto* colliderA = world.Get<CircleCollider2D>(ballA);
//        auto* transformA = world.Get<LocalTransform>(ballA);
//        if (!rigidbodyA || !colliderA || !transformA) continue;
//
//        const float radiusA = _getScaledRadius(colliderA->Radius, Vector2D(transformA->Scale.X, transformA->Scale.Y));
//
//        for (size_t j = i + 1; j < ballCount; ++j) {
//            uint64_t packedBallB = m_balls[j];
//            if (!IsEntityAlive(world, packedBallB)) continue;
//
//            Entity ballB = EntityUtils::Unpack(packedBallB);
//            auto* rigidbodyB = world.Get<Rigidbody2D>(ballB);
//            auto* colliderB = world.Get<CircleCollider2D>(ballB);
//            auto* transformB = world.Get<LocalTransform>(ballB);
//            if (!rigidbodyB || !colliderB || !transformB) continue;
//
//            const float radiusB = _getScaledRadius(colliderB->Radius, Vector2D(transformB->Scale.X, transformB->Scale.Y));
//
//            Vector2D posA(transformA->Position.X, transformA->Position.Y);
//            Vector2D posB(transformB->Position.X, transformB->Position.Y);
//
//            // Delegate collision resolution to Physics class
//            Engine::Physics::ResolveCircleCircleCollision(
//                *rigidbodyA, *rigidbodyB,
//                posA, posB,
//                radiusA, radiusB,
//                collisionParams
//            );
//
//            // Update transform positions after collision resolution
//            transformA->Position.X = posA.X;
//            transformA->Position.Y = posA.Y;
//            transformB->Position.X = posB.X;
//            transformB->Position.Y = posB.Y;
//        }
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_updateCubeCollisions(World& world) {
//    if (m_heroId == UINT64_MAX) return;
//    if (!IsEntityAlive(world, m_heroId)) return;
//
//    Entity hero = EntityUtils::Unpack(m_heroId);
//    auto* heroTransform = world.Get<LocalTransform>(hero);
//    if (!heroTransform) return;
//
//    const float heroRadius = _getScaledRadius(HERO_TRIANGLE_HALF_HEIGHT, Vector2D(heroTransform->Scale.X, heroTransform->Scale.Y));
//    const Vector2D heroPos(heroTransform->Position.X, heroTransform->Position.Y);
//
//    for (size_t i = 0; i < m_cubes.size(); /* incremented in loop */) {
//        uint64_t packedCube = m_cubes[i];
//        if (!IsEntityAlive(world, packedCube)) {
//            _removeCubeAtIndex(i);
//            continue;
//        }
//
//        Entity cube = EntityUtils::Unpack(packedCube);
//        auto* cubeTransform = world.Get<LocalTransform>(cube);
//        auto* cubeShape = world.Get<ShapeRenderer2D>(cube);
//
//        if (!cubeTransform) {
//            ++i;
//            continue;
//        }
//
//        const float cubeHalfSize = _calculateCubeHalfSize(cubeShape);
//        const Vector2D cubePos(cubeTransform->Position.X, cubeTransform->Position.Y);
//
//        // Check if cube should be destroyed
//        bool shouldDestroy = _shouldDestroyCube(cubePos.Y, cubeHalfSize);
//
//        if (!shouldDestroy) {
//            DynCol::Circle heroCircle{ heroPos, heroRadius };
//            DynCol::AABB cubeAABB{
//                {cubePos.X - cubeHalfSize, cubePos.Y - cubeHalfSize},
//                {cubePos.X + cubeHalfSize, cubePos.Y + cubeHalfSize}
//            };
//            shouldDestroy = DynCol::Overlap(heroCircle, cubeAABB, nullptr);
//        }
//
//        if (shouldDestroy) {
//            world.Destroy(cube);
//            _removeCubeAtIndex(i);
//            continue;
//        }
//
//        ++i;
//    }
//}
//
//// ============================================================================
//// Hero Movement & Controls
//// ============================================================================
//
//void Sandbox::PhysicsCollision2DTestScene::_updateHeroMovement(const float heroRadius) {
//    if (m_heroId == UINT64_MAX) return;
//
//    World& world = GetWorld();
//    if (!IsEntityAlive(world, m_heroId)) return;
//
//    const Entity hero = EntityUtils::Unpack(m_heroId);
//
//    if (!world.Has<Components::LocalTransform>(hero) ||
//        !world.Has<Components::Rigidbody2D>(hero) || 
//        !world.Has<Components::LinearVelocity2D>(hero))
//        return;
//
//    auto& transform = world.Get<Components::LocalTransform>(hero);
//    auto& rigidbody = world.Get<Components::Rigidbody2D>(hero);
//    auto& linearVelocity = world.Get<Components::LinearVelocity2D>(hero);
//
//    // Input handling
//    Vector2D inputForce(0.0f, 0.0f);
//    if (Input::IsKeyDown(KEY_A)) inputForce.X -= HERO_THRUST;
//    if (Input::IsKeyDown(KEY_D)) inputForce.X += HERO_THRUST;
//    if (Input::IsKeyDown(KEY_W)) inputForce.Y += HERO_THRUST;
//    if (Input::IsKeyDown(KEY_S)) inputForce.Y -= HERO_THRUST;
//
//    if (inputForce.X != 0.0f || inputForce.Y != 0.0f) {
//        Engine::Physics::ApplyForce(rigidbody, inputForce);
//    }
//
//    // Jump input
//    if (Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT) || Input::IsKeyPressed(KEY_SPACE)) {
//        Engine::Physics::ApplyImpulse(rigidbody, Vector2D(0.0f, HERO_JUMP_IMPULSE));
//    }
//
//    // Apply velocity damping and boundary constraints
//    Engine::Physics::ApplyVelocityDamping(rigidbody, HERO_VELOCITY_DAMPING);
//    _applyBoundaryConstraints(transform.Position, linearVelocity.Value, heroRadius);
//
//    // Update visual representation
//    if (world.Has<Components::ShapePolygon2D<16>>(hero)) {
//        auto& polyLine = world.Get<Components::ShapePolygon2D<16>>(hero);
//        polyLine.Points[0] = { transform.Position.X, transform.Position.Y + HERO_TRIANGLE_HALF_HEIGHT };
//        polyLine.Points[1] = { transform.Position.X - HERO_TRIANGLE_HALF_BASE, transform.Position.Y - HERO_TRIANGLE_HALF_HEIGHT };
//        polyLine.Points[2] = { transform.Position.X + HERO_TRIANGLE_HALF_BASE, transform.Position.Y - HERO_TRIANGLE_HALF_HEIGHT };
//
//        polyLine.Count = 3;
//    }
//    else if (world.Has<Components::ShapeCircle2D>(hero)) {
//        auto& circleRenderer = world.Get<Components::ShapeCircle2D>(hero);
//        circleRenderer.Radius = heroRadius;
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_applyBoundaryConstraints(
//    Vector3D& position, Vector2D& velocity, const float radius) {
//    const float scaledRadius = _getScaledRadius(radius, Vector2D(1.0f, 1.0f));
//    Engine::Physics::BoundaryConstraint bounds{ 0.0f, m_worldWidth, 0.0f, m_worldHeight, true };
//    Engine::Physics::ApplyBoundaryConstraint(position, velocity, scaledRadius, bounds);
//}
//
//bool Sandbox::PhysicsCollision2DTestScene::_resolveCircleAABBCollision(
//    Vector3D& circlePosition, Vector2D& circleVelocity,
//    const Vector3D& boxMin, const Vector3D& boxMax, float circleRadius) {
//
//    // Delegate collision resolution to Physics class
//    Engine::Physics::CircleAABBResult result = Engine::Physics::ResolveCircleAABBCollision(
//        circlePosition, circleVelocity,
//        boxMin, boxMax,
//        circleRadius,
//        0.001f  // epsilon
//    );
//
//    if (result.collided) {
//        LOG_DEBUG("Collision detected and resolved");
//    }
//
//    return result.collided;
//}
//
//// ============================================================================
//// Step-by-Step Debugging
//// ============================================================================
//
//void Sandbox::PhysicsCollision2DTestScene::_handleStepByStepControls() {
//    static bool pKeyWasPressed = false;
//    static bool spaceKeyWasPressed = false;
//
//    bool pKeyPressed = Input::IsKeyDown(KEY_P);
//    bool spaceKeyPressed = Input::IsKeyDown(KEY_SPACE);
//
//    if (pKeyPressed && !pKeyWasPressed) {
//        m_stepByStepMode = !m_stepByStepMode;
//        m_pausePhysics = m_stepByStepMode;
//        Engine::PhysicsSystem::SetEnabled(!m_pausePhysics);
//
//        if (m_pausePhysics) {
//            std::cout << "Step-by-step physics mode ENABLED\n";
//            _storeBallStates();
//        }
//        else {
//            std::cout << "Step-by-step physics mode DISABLED\n";
//            _restoreBallStates();
//        }
//    }
//
//    if ((m_stepByStepMode || m_pausePhysics) && spaceKeyPressed && !spaceKeyWasPressed && m_pausePhysics) {
//        Engine::PhysicsSystem::SetEnabled(true);
//        m_stepRequested = true;
//        std::cout << "Physics step requested\n";
//    }
//
//    pKeyWasPressed = pKeyPressed;
//    spaceKeyWasPressed = spaceKeyPressed;
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_storeBallStates() {
//    World& world = GetWorld();
//    m_storedBallStates.clear();
//
//    for (uint64_t packedBall : m_balls) {
//        if (!IsEntityAlive(world, packedBall)) continue;
//
//        Entity ball = EntityUtils::Unpack(packedBall);
//        auto* rigidbody = world.Get<Rigidbody2D>(ball);
//        auto* transform = world.Get<LocalTransform>(ball);
//
//        if (rigidbody && transform) {
//            m_storedBallStates[packedBall] = {
//                rigidbody->LinearVelocity,
//                Vector2D(transform->Position.X, transform->Position.Y)
//            };
//        }
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_restoreBallStates() {
//    World& world = GetWorld();
//
//    for (uint64_t packedBall : m_balls) {
//        auto it = m_storedBallStates.find(packedBall);
//        if (it != m_storedBallStates.end() && IsEntityAlive(world, packedBall)) {
//            Entity ball = EntityUtils::Unpack(packedBall);
//            auto* rigidbody = world.Get<Rigidbody2D>(ball);
//            auto* transform = world.Get<LocalTransform>(ball);
//
//            if (rigidbody && transform) {
//                rigidbody->LinearVelocity = it->second.Velocity;
//                transform->Position.X = it->second.Position.X;
//                transform->Position.Y = it->second.Position.Y;
//            }
//        }
//    }
//    m_storedBallStates.clear();
//}
//
//// ============================================================================
//// Utility Functions
//// ============================================================================
//
//void Sandbox::PhysicsCollision2DTestScene::_destroyAllEntities(World& world) {
//    // Destroy all balls
//    for (uint64_t packedBall : m_balls) {
//        if (IsEntityAlive(world, packedBall)) {
//            Entity ball = EntityUtils::Unpack(packedBall);
//            world.Destroy(ball);
//        }
//    }
//
//    // Destroy all cubes
//    for (uint64_t packedCube : m_cubes) {
//        if (IsEntityAlive(world, packedCube)) {
//            Entity cube = EntityUtils::Unpack(packedCube);
//            world.Destroy(cube);
//        }
//    }
//
//    // Destroy all static squares
//    for (uint64_t packedSquare : m_staticSquares) {
//        if (IsEntityAlive(world, packedSquare)) {
//            Entity square = EntityUtils::Unpack(packedSquare);
//            world.Destroy(square);
//        }
//    }
//
//    // Destroy hero
//    if (IsEntityAlive(world, m_heroId)) {
//        Entity hero = EntityUtils::Unpack(m_heroId);
//        world.Destroy(hero);
//    }
//
//    // Destroy mid line
//    if (IsEntityAlive(world, m_midLineId)) {
//        Entity midLine = EntityUtils::Unpack(m_midLineId);
//        world.Destroy(midLine);
//    }
//
//    m_balls.clear();
//    m_cubes.clear();
//    m_staticSquares.clear();
//    m_heroId = UINT64_MAX;
//    m_midLineId = UINT64_MAX;
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_destroyCube(World& world, size_t index) {
//    if (index >= m_cubes.size()) return;
//
//    uint64_t packedCube = m_cubes[index];
//    if (IsEntityAlive(world, packedCube)) {
//        Entity cube = EntityUtils::Unpack(packedCube);
//        world.Destroy(cube);
//    }
//
//    _removeCubeAtIndex(index);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_removeCubeAtIndex(size_t index) {
//    m_cubes[index] = m_cubes.back();
//    m_cubes.pop_back();
//}
//
//float Sandbox::PhysicsCollision2DTestScene::_calculateCubeHalfSize(const ShapeRenderer2D* shapeRenderer) const {
//    if (!shapeRenderer || shapeRenderer->Type != ShapeRenderer2D::ShapeType::Polygon
//        || shapeRenderer->Points.size() < 4) {
//        return DEFAULT_CUBE_HALF_SIZE;
//    }
//
//    float minX, maxX, minY, maxY;
//    GetPointBounds(shapeRenderer->Points, minX, maxX, minY, maxY);
//    return 0.5f * std::max(maxX - minX, maxY - minY);
//}
//
//float Sandbox::PhysicsCollision2DTestScene::_getScaledRadius(float baseRadius, const Vector2D& scale) const {
//    return baseRadius * ((scale.X + scale.Y) * 0.5f);
//}
//
//int Sandbox::PhysicsCollision2DTestScene::_buildStaticSquareAABBs(
//    World& world, DynCol::AABB* targets, int maxCount) const {
//
//    const float halfSquare = STATIC_SQUARE_SIDE_LENGTH * 0.5f;
//    int count = 0;
//
//    for (size_t k = 0; k < std::min(static_cast<size_t>(maxCount), m_staticSquares.size()); ++k) {
//        uint64_t packedSquare = m_staticSquares[k];
//        if (!IsEntityAlive(world, packedSquare)) continue;
//
//        Entity square = EntityUtils::Unpack(packedSquare);
//        const auto* transform = world.Get<LocalTransform>(square);
//        if (!transform) continue;
//
//        targets[count++] = DynCol::AABB{
//            {transform->Position.X - halfSquare, transform->Position.Y - halfSquare},
//            {transform->Position.X + halfSquare, transform->Position.Y + halfSquare}
//        };
//    }
//    return count;
//}
//
//bool Sandbox::PhysicsCollision2DTestScene::_checkSweptAABBCollision(
//    const Vector2D& startPos, const Vector2D& endPos, float halfSize,
//    const DynCol::AABB* targets, int targetCount, float deltaTime) const {
//
//    DynCol::AABB movingAABB{
//        {startPos.X - halfSize, startPos.Y - halfSize},
//        {startPos.X + halfSize, startPos.Y + halfSize}
//    };
//
//    for (int i = 0; i < targetCount; ++i) {
//        const Vector2D targetCenter(
//            (targets[i].min.X + targets[i].max.X) * 0.5f,
//            (targets[i].min.Y + targets[i].max.Y) * 0.5f
//        );
//
//        DynCol::SweepHit sweepResult = DynCol::Sweep(movingAABB, endPos, targets[i], targetCenter);
//        if (sweepResult.hit && sweepResult.toi >= 0.0f && sweepResult.toi <= 1.0f) {
//            return true;
//        }
//    }
//    return false;
//}
//
//bool Sandbox::PhysicsCollision2DTestScene::_shouldDestroyCube(float cubeY, float cubeHalfSize) const {
//    if (m_currentTest == TestType::DynamicVsStaticResponse) {
//        return cubeY - cubeHalfSize <= m_worldHeight / 3.0f;
//    }
//    else if (m_currentTest == TestType::StepByStepUpdate) {
//        return cubeY - cubeHalfSize <= 0.0f;
//    }
//    return true;
//}
//
//
//
//
//
//#include "PhysicsCollision2DTest.h"
//#include "core/Application.h"
//#include "ecs/Components.h"
//#include "ecs/Entity.h"
//#include "ecs/World.h"
//#include "ecs/systems/LifetimeSystem.h"
//#include "ecs/systems/PhysicsSystem.h"
//#include "helpers/EntityUtils.h"
//#include "helpers/MathUtils.h"
//#include "physics/DynamicCollision.h"
//#include "services/Input.h"
//#include "services/Time.h"
//#include "services/Window.h"
//#include "services/WindowManager.h"
//#include <cmath>
//#include <iostream>
//
//constexpr float TWO_PI = 6.28318530718f;
//
//namespace {
//    using TestType = Sandbox::PhysicsCollision2DTestScene::TestType;
//
//    // Define the exact order you want to cycle through
//    constexpr TestType kCycleOrder[] = {
//        TestType::test_PhysicsMovement,
//        TestType::test_CollisionDetection,
//        TestType::test_PhysicsHero,
//        TestType::test_DynvStatResponse,
//        TestType::test_DynvDynResponse,
//        TestType::test_StepbyStepUpdate, // last
//    };
//
//    inline TestType NextTest(TestType cur) {
//        const size_t N = sizeof(kCycleOrder) / sizeof(kCycleOrder[0]);
//        for (size_t i = 0; i < N; ++i) {
//            if (kCycleOrder[i] == cur) {
//                return kCycleOrder[(i + 1) % N];  // wrap around
//            }
//        }
//        // Fallback if cur isn't in the list
//        return kCycleOrder[0];
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::OnLoad() {
//    // Load config here, although it should be loaded in Game.cpp to create the window
//    // But we put it here as user input is needed first
//    const auto& config = Engine::CORE->GetConfig();
//    const int windowWidth = config.WindowConfig.Width;
//    const int windowHeight = config.WindowConfig.Height;
//
//    CREATE_WINDOW("Physics & Collision Test", windowWidth, windowHeight);
//    m_worldWidth = static_cast<float>(windowWidth);
//    m_worldHeight = static_cast<float>(windowHeight);
//    m_elapsedTime = 0.0f;
//    m_dampingDelay = 7.f;
//    m_dampingEnabled = false;
//
//    // step-by-step physics mode initialization
//    m_stepByStepMode = false;
//    m_stepRequested = false;
//    m_pausePhysics = false;
//
//    // set gravity
//    Engine::PhysicsSystem::SetGravity(Vector2D(0.0f, -15.0f));
//
//    // setup gameplay layer
//    m_gameplayLayer = GetLayers().CreateOrGetLayer("gameplay");
//
//    AddSystem([](Scenes::Scene& s, float dt) {
//        ECS::LifetimeSystem::Update(s.GetWorld(), dt);
//    }, "Lifetime System");
//}
//
//void Sandbox::PhysicsCollision2DTestScene::OnUpdate() {
//    // Input trigger to cycle tests (H key)
//    if (Input::IsKeyDown(KEY_C)) {
//
//        if (!test_handler) {
//            int current = static_cast<int>(m_currentTest);
//            current++;
//            if (current > static_cast<int>(TestType::test_StepbyStepUpdate)) {
//                current = static_cast<int>(TestType::test_PhysicsMovement);
//            }
//            m_currentTest = static_cast<TestType>(current);
//            std::cout << "Switched to physics test " << current << std::endl;
//
//            _clearEntities();
//            _resetFlagsAndVariables();
//
//            // When switching, you could clear scene and re-run OnLoad if desired
//            // For now we keep entities; you can decide to reset if needed.
//            test_handler = true;
//        }
//    }
//    else {
//        test_handler = false;
//    }
//
//    // Dispatch to active test
//    switch (m_currentTest) {
//    case TestType::test_PhysicsMovement:    Test_PhysicsMovement();    break;
//    case TestType::test_CollisionDetection: Test_CollisionDetection(); break;
//    case TestType::test_PhysicsHero:        Test_PhysicsHero();        break;
//    case TestType::test_DynvStatResponse:   Test_DynVStatResponse();   break;
//    case TestType::test_DynvDynResponse:    Test_DynVDynResponse();    break;
//    case TestType::test_StepbyStepUpdate:   Test_StepByStepUpdate();   break;
//    }
//}
//
//
//void Sandbox::PhysicsCollision2DTestScene::OnFixedUpdate() {
//    ECS::World& world = GetWorld();
//    const float dt = Time::FixedDeltaTime();
//    if (m_currentTest != TestType::test_StepbyStepUpdate) {
//        return;
//    }
//    if (!m_pausePhysics || m_stepRequested) {
//        _updateBallCollisions();
//
//        if (m_stepRequested) {
//            // step, and pause physics
//            Engine::PhysicsSystem::SetEnabled(false);
//        }
//        m_stepRequested = false;
//    }
//
//    // cubes
//    _spawnCubes_T(dt);
//    _updateCubesCollisions(world);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::OnUnload() {
//    _clearEntities();
//    _resetFlagsAndVariables();
//}
//
//
//// running test codes
//void Sandbox::PhysicsCollision2DTestScene::Test_PhysicsMovement() {
//    ECS::World& world = GetWorld();
//
//    if (!m_stepInit) {
//        _spawnBalls(world, 10);
//        m_stepInit = true;
//        m_dampingEnabled = true;
//        for (auto& ball : m_balls) {
//            if (world.Has<Components::Rigidbody2D>(ball)) {
//                auto& rb = world.Get<Components::Rigidbody2D>(ball);
//                rb.LinearDamping = 0.5f; // physics added
//            }
//            std::cout << "Damping enabled after " << m_dampingDelay << " seconds!\n";
//        }
//    }
//    _updateBallCollisions();
//}
//
//void Sandbox::PhysicsCollision2DTestScene::Test_CollisionDetection() {
//    ECS::World& world = GetWorld();
//
//    if (m_playerId == UINT64_MAX) {
//        _createHeroCircle();
//    }
//
//    ECS::Entity hero = CreateOnLayer(
//        m_gameplayLayer,
//        Components::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
//        Components::WorldTransform{ },
//        Components::LinearVelocity2D{ Vector2D{0,0} },
//        Components::Rigidbody2D{
//            1.0f, // Mass
//            1.0f, // InverseMass
//            0.0f, // LinearDamping
//            0.0f, // AngularDamping
//            0.0f, // GravityScale
//            0u    // Flags
//        },
//        Components::CircleCollider2D{ m_radius },
//        Components::ShapeCircle2D{ 
//            m_radius,                           // Radius
//            Vector2D{0,0},                      // Offset
//            Color(0.95f, 0.20f, 0.20f, 1.0f),   // Color
//            2.0f,                               // Thickness
//            false                               // Filled
//        }
//    );
//
//    m_playerId = EntityUtils::Pack(hero);
//
//    auto& htr = world.Get<Components::LocalTransform>(hero);
//    const Vector3D oldPos = htr.Position;
//
//    // Circle-specific movement/clamp
//    _clampAndBounceCircleHero();
//
//    if (m_staticsquares.empty()) {
//        const float sideLen = 200.0f;
//        const float half = sideLen * 0.5f;
//
//        auto makeSquare = [&](const char* name, float cx, float cy) {
//            // 4 points (counter-clockwise) around center 
//            std::vector<Vector2D> pts;
//            pts.emplace_back(cx - half, cy - half); // bottom-left
//            pts.emplace_back(cx - half, cy + half); // top-left
//            pts.emplace_back(cx + half, cy + half); // top-right
//            pts.emplace_back(cx + half, cy - half); // bottom-right
//
//            ECS::Entity sq = CreateOnLayer(
//                m_gameplayLayer,
//                Components::LocalTransform{ Vector3D{cx, cy, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
//                Components::WorldTransform{ },
//                Components::LinearVelocity2D{ Vector2D{0,0} },
//                Components::BoxCollider2D{ 
//                    Vector2D{half, half}, // HalfExtents
//                    Vector2D{0,0},        // Offset
//                    0.0f,                 // Rotation
//                    0xFFFFFFFFu,          // LayerMask
//                    0u                    // Flags
//                },
//                Components::ShapeBox2D{ 
//                    Vector2D{half, half},   // HalfExtents
//                    Vector2D{0,0},          // Offset
//                    Color{1.f,1.f,1.f,1.f}, // Color
//                    1.0f,                   // Thickness
//                    true                    // Filled
//                }
//            );
//
//            m_staticsquares.push_back(EntityUtils::Pack(sq));
//        };
//
//        // Put them mid-screen so they're clearly visible
//        makeSquare("StaticSquareL", m_worldWidth * 0.40f, m_worldHeight * 0.50f);
//        makeSquare("StaticSquareR", m_worldWidth * 0.70f, m_worldHeight * 0.50f);
//
//    }
//
//    if (!world.Has<Components::Rigidbody2D>(hero) || !world.Has<Components::CircleCollider2D>(hero)) return;
//
//    auto hcc = world.Get<Components::CircleCollider2D>(hero);
//    auto hrb = world.Get<Components::LinearVelocity2D>(hero);
//
//    // Apply transform scale to collision radius
//    const float R = hcc.Radius * ((htr.Scale.X + htr.Scale.Y) * 0.5f);
//
//    // Minimal push-out against one AABB. Returns true if we adjusted the hero.
//    auto resolveCircleAABB = [&](const Vector2D& C,
//        const Vector2D& bmin,
//        const Vector2D& bmax) -> bool
//        {
//            // Closest point on AABB to circle center
//            const Vector2D closest(
//                (std::max)(bmin.X, (std::min)(C.X, bmax.X)),
//                (std::max)(bmin.Y, (std::min)(C.Y, bmax.Y))
//            );
//
//            Vector2D diff = C - closest;
//            float d2 = diff.Dot(diff);
//
//            if (d2 >= R * R)
//                return false; // no overlap
//
//            // Outside case: push out along edge normal
//            if (d2 > 1e-6f) {
//                float d = std::sqrt(d2);
//                Vector2D n = diff / d;            // outward normal from box toward circle center
//                float penetration = R - d;
//
//                Vector2D push = Vector2D(n * penetration);
//                htr.Position += Vector3D{push.X, push.Y, 0.0f};  // move just enough to clear
//
//                // kill only inward velocity 
//                float vn = hrb.Value.Dot(n);
//                if (vn < 0.0f) 
//                    hrb.Value -= n * vn;
//
//                LOG_DEBUG("Collision was detected");
//                return true;
//            }
//
//            // Inside case (center inside box): push along smallest axis to exit
//            float left  = C.X - bmin.X;
//            float right = bmax.X - C.X;
//            float down  = C.Y - bmin.Y;
//            float up    = bmax.Y - C.Y;
//
//            Vector2D n(0, 0);
//            float push = 0.0f;
//            if ((std::min)(left, right) < (std::min)(down, up)) {
//                if (left < right) { n = { 1, 0 }; push = left; }  // push right
//                else { n = { -1, 0 }; push = right; }  // push left
//            }
//            else {
//                if (down < up) {
//                    n = { 0, 1 };
//                    push = down;
//                }  // push up
//                else {
//                    n = { 0, -1 };
//                    push = up;
//                }  // push down
//            }
//
//            Vector2D pushVec = n * (push + 0.001f);   // tiny epsilon prevents repenetration
//            htr.Position += Vector3D{pushVec.X, pushVec.Y, 0.0f};  // move just enough to clear
//            float vn = hrb.Value.Dot(n);
//            if (vn < 0.0f)
//                hrb.Value -= n * vn;
//            return true;
//        };
//
//    // Use the sideLen here as in creation above
//    const float sideLen = 200.0f;  // to sync with original implementationsss
//    const float half = sideLen * 0.5f;
//
//    // Resolve against both squares 
//    for (size_t eid : m_staticsquares) {
//        ECS::Entity sq = EntityUtils::Unpack(eid);
//        const auto& btr = world.Get<Components::LocalTransform>(sq);
//
//        const Vector2D bmin(btr.Position.X - half, btr.Position.Y - half);
//        const Vector2D bmax(btr.Position.X + half, btr.Position.Y + half);
//
//        (void)resolveCircleAABB(Vector2D{htr.Position.X, htr.Position.Y}, bmin, bmax);
//    }
//    // here after the position correction so visuals match the new position.
//}
//
//void Sandbox::PhysicsCollision2DTestScene::Test_PhysicsHero() {
//    // Ensure hero exists
//    if (m_playerId == UINT64_MAX) {
//        _createTriangle();
//        LOG_DEBUG("[CharacterMovement] Triangle hero spawned\n");
//    }
//
//    // Update hero movement
//    _clampAndBouncePlayer();
//}
//
//void Sandbox::PhysicsCollision2DTestScene::Test_DynVStatResponse() {
//    ECS::World& world = GetWorld();
//    const float dt = Time::FixedDeltaTime();
//
//    //  Ensure two static squares tracked via m_staticsquares
//    const float side = 200.0f;
//    const float half = side * 0.5f;
//
//    auto ensureSquareAt = [&](size_t slot, const char* name, float cx, float cy) {
//        if (m_staticsquares.size() <= slot)
//            m_staticsquares.resize(slot + 1, UINT32_MAX);
//        uint64_t& id = m_staticsquares[slot];
//
//        auto makeSquare = [&](const char* nm) {
//            ECS::Entity sq = CreateOnLayer(
//                m_gameplayLayer,
//                Components::LocalTransform{ Vector3D{cx,cy,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
//                Components::WorldTransform{ },
//                Components::LinearVelocity2D{},
//                Components::Name{ *nm },
//                Components::BoxCollider2D{ 
//                    Vector2D{half, half}, // HalfExtents
//                    Vector2D{0,0},        // Offset
//                    0.0f,                 // Rotation
//                    0xFFFFFFFFu,          // LayerMask
//                    0u                    // Flags
//                },
//                Components::ShapeBox2D{ 
//                    Vector2D{half, half},               // HalfExtents
//                    Vector2D{0,0},                      // Offset
//                    Color(0.95f, 0.20f, 0.20f, 1.0f),   // Color
//                    1.0f,                               // Thickness
//                    true                                // Filled
//                }
//            );
//            id = EntityUtils::Pack(sq);
//        };
//
//        if (id == UINT64_MAX) {
//            makeSquare(name);
//            return;
//        }
//
//        ECS::Entity sq = EntityUtils::Unpack(id);
//        if (!world.IsAlive(sq)) {
//            id = UINT64_MAX;
//            makeSquare(name);
//        }
//        else {
//            // keep square pinned (in case something moved it)
//            world.Get<Components::LocalTransform>(sq).Position = { cx, cy, 0 };
//        }
//    };
//
//    ensureSquareAt(0, "StaticSquareL", m_worldWidth * 0.40f, m_worldHeight * 0.50f);
//    ensureSquareAt(1, "StaticSquareR", m_worldWidth * 0.70f, m_worldHeight * 0.50f);
//
//    // Spawn/update your falling cubes as usual 
//    _spawnCubes_T(dt);
//
//    // Build static AABBs for the two squares (targets for sweep)
//    DynCol::AABB targets[2];
//    int targetCount = 0;
//    for (size_t k = 0; k < 2 && k < m_staticsquares.size(); ++k) {
//        uint64_t id = m_staticsquares[k];
//        if (id == UINT64_MAX)
//            continue;
//
//        ECS::Entity sq = EntityUtils::Unpack(id);
//        if (!world.IsAlive(sq))
//            continue;
//
//        const auto& tr = world.Get<Components::LocalTransform>(sq);
//        targets[targetCount++] = DynCol::AABB{
//            { tr.Position.X - half, tr.Position.Y - half },
//            { tr.Position.X + half, tr.Position.Y + half },
//        };
//    }
//
//    //  Delete cubes whose swept AABB hits either square this frame
//    for (size_t i = 0; i < m_seacubes.size(); /* no ++ here its set in loop for case types  */) {
//        ECS::Entity& cube = EntityUtils::Unpack(m_seacubes[i]);
//        if (!world.IsAlive(cube)) {                        // drop dead handles
//            m_seacubes[i] = m_seacubes.back();
//            m_seacubes.pop_back();
//            continue;
//        }
//
//        const auto& ct = world.Get<Components::LocalTransform>(cube);
//        if (!world.Has<Components::Rigidbody2D>(cube) || !world.Has<Components::ShapeBox2D>(cube)) {
//            ++i;
//            continue;
//        }
//
//        auto rb = world.Get<Components::Rigidbody2D>(cube);
//        auto sh = world.Get<Components::ShapeBox2D>(cube);
//
//        // Derive cube half-size from its polygon points (matches your update logic)
//        float halfCube = 10.0f;
//        if (sh.Points.size() >= 4) {
//            float minX = sh.HalfExtents.X, maxX = sh.HalfExtents.X;
//            float minY = sh.HalfExtents.Y, maxY = sh.HalfExtents.Y;
//            for (const auto& p : sh.Points) {
//                if (p.X < minX) minX = p.X; if (p.X > maxX) maxX = p.X;
//                if (p.Y < minY) minY = p.Y; if (p.Y > maxY) maxY = p.Y;
//            }
//            halfCube = 0.5f * (std::max)(maxX - minX, maxY - minY);
//        }
//
//        // Moving AABB for the cube
//        DynCol::AABB A = {
//            { ct.Position.X - halfCube, ct.Position.Y - halfCube },
//            { ct.Position.X + halfCube, ct.Position.Y + halfCube }
//        };
//        const Vector2D A_endCenter = {
//            ct.Position.X + rb->LinearVelocity.X * dt,
//            ct.Position.Y + rb->LinearVelocity.Y * dt
//        };
//
//        bool hit = false;
//        for (int s = 0; s < targetCount && !hit; ++s) {
//            // Static target end center is irrelevant to the math; pass its current center
//            const Vector2D B_endCenter = {
//                (targets[s].min.X + targets[s].max.X) * 0.5f,
//                (targets[s].min.Y + targets[s].max.Y) * 0.5f
//            };
//            DynCol::SweepHit H = DynCol::Sweep(A, A_endCenter, targets[s], B_endCenter);
//            if (H.hit && H.toi >= 0.0f && H.toi <= 1.0f) hit = true;
//        }
//
//        if (hit) {
//            // Erase this cube (swap-with-back + pop is fine; ECS destroy if you want)
//            // CubeDisintegrate(world, i);  // if you have this, use it; otherwise:
//            DestroyEntity(cube);
//            m_seacubes[i] = m_seacubes.back();
//            m_seacubes.pop_back();
//            continue;                     // do NOT ++i after erase
//        }
//        ++i; // to prevent ++ for continue cases
//    }
//
//    // --- 5) Regular maintenance (bounds etc.). Do NOT move cubes in this function.
//    _updateCubesCollisions(world);
//}
//
//
//
//void Sandbox::PhysicsCollision2DTestScene::Test_DynVDynResponse() {
//    ECS::World& world = GetWorld();
//    if (!m_stepInit) {
//        _spawnBalls(world, 50);
//        m_stepInit = true;
//    }
//    _updateBallCollisions(); // walls / floor / bounds
//    _ballCollide();          // ball–ball
//}
//
//// TODO: Shift this to a global function rather than specific to this scene
//void Sandbox::PhysicsCollision2DTestScene::Test_StepByStepUpdate() {
//    ECS::World& world = GetWorld();
//    if (!m_stepInit) {
//        _spawnBalls(world, 10);
//        std::cout << "PhysicsCollision2DTestScene initialized with " << m_balls.size() << " balls\n";
//        std::cout << "Step-by-step physics controls:\n";
//        std::cout << "  P - Toggle step-by-step mode\n";
//        std::cout << "  Space - Step physics (when in step mode)\n";
//
//        if (m_playerId == UINT64_MAX) {
//            _createTriangle();
//        }
//        m_stepInit = true;
//    }
//
//    m_elapsedTime = static_cast<float>(Time::ElapsedTime());
//    _handleStepByStepControls();
//
//    if (!m_dampingEnabled && m_elapsedTime >= m_dampingDelay) {
//        m_dampingEnabled = true;
//        for (auto& ball : m_balls) {
//            if (auto* rb = ball.GetComponent<Component::Rigidbody2D>()) {
//                rb->LinearDamping = 0.5f;
//            }
//        }
//        std::cout << "Damping enabled after " << m_dampingDelay << " seconds!\n";
//    }
//    _clampAndBouncePlayer();
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_clearEntities() {
//    ECS::World& world = GetWorld();
//
//    for (auto e : m_balls) {
//        if (world.IsAlive(e))
//            world.Destroy(e);
//    }
//    m_balls.clear();
//
//    for (auto e : m_seacubes) {
//        if (world.IsAlive(e))
//            world.Destroy(e);
//    }
//    m_seacubes.clear();
//
//    for (auto e : m_staticsquares) {
//        ECS::Entity ent = EntityUtils::Unpack(e);
//        if (world.IsAlive(ent))
//            world.Destroy(ent);
//    }
//    m_staticsquares.clear();
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_resetFlagsAndVariables() {
//    m_playerId = UINT64_MAX;
//    m_midLineId = UINT64_MAX;
//
//    m_stepInit = false;
//    m_dampingEnabled = false;
//    m_spawnAcc = 0.0f;
//    m_stepByStepMode = false;
//    m_pausePhysics = false;
//    m_stepRequested = false;
//
//    m_elapsedTime = 0.0f;
//    m_dampingDelay = 7.f;
//
//    // set gravity
//    Engine::PhysicsSystem::SetGravity(Vector2D(0.0f, -15.0f));
//
//    Engine::PhysicsSystem::SetEnabled(true);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_createHeroCircle() {
//    if (m_playerId != UINT64_MAX) return;
//    ECS::World& world = GetWorld();
//
//    ECS::Entity e = world.Create( 
//        ECS::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
//        Components::Name{ "HeroCircle" }
//    );
//    m_playerId = EntityUtils::Pack(e);
//
//    const float cx = m_worldWidth * 0.25f;
//    const float cy = m_worldHeight * 0.50f;
//
//    world.Get<ECS::LocalTransform>(e).Position = { cx, cy, 0.f };
//
//    auto& rb = e.AddComponent<Component::Rigidbody2D>();
//    rb.Mass = 2.0f;
//    rb.GravityScale = 1.0f;
//    rb.LinearDamping = 0.10f;
//
//    // Visual: circle
//    auto& sh = e.AddComponent<ECS::Components::ShapeRenderer2D>();
//    sh.Type = ECS::Components::ShapeRenderer2D::ShapeType::Circle;
//    sh.Radius = m_radius;
//    sh.FillColor = Color(0.95f, 0.90f, 0.20f, 1.0f); // same feel as triangle
//
//    // Collider: circle
//    e.AddComponent<ECS::Components::CircleCollider2D>(m_radius);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_clampAndBounceCircleHero() {
//    if (m_playerId == UINT64_MAX) return;
//    ECS::World& world = GetWorld();
//
//    ECS::Entity player(m_playerId, &world);
//    auto& tr = world.Get<ECS::LocalTransform>(player);
//    auto* rb = world.Get<ECS::Components::Rigidbody2D>(player);
//    auto* sh = world.Get<ECS::Components::ShapeRenderer2D>(player);
//    auto* cc = world.Get<ECS::Components::CircleCollider2D>(player);
//    if (!rb || !cc) return;
//
//    const float thrust = 1.0f;
//    const float jumpImpulse = 20.0f;      // keep parity with triangle handler
//    const float velDampScale = 0.996f;
//
//    bool leftHeld = Input::IsKeyDown(KEY_A);
//    bool rightHeld = Input::IsKeyDown(KEY_D);
//    bool upHeld = Input::IsKeyDown(KEY_W);
//    bool downHeld = Input::IsKeyDown(KEY_S);
//
//    Vector2D F(0.0f, 0.0f);
//    if (leftHeld)  F.X -= thrust;
//    if (rightHeld) F.X += thrust;
//    if (upHeld)    F.Y += thrust;
//    if (downHeld)  F.Y -= thrust;
//
//    if (F.X != 0.0f || F.Y != 0.0f) {
//        // same semantics as your player handler: treat as force this frame
//        Engine::PhysicsSystem::AddForce(*rb, F);
//    }
//
//    // SPACE to jump (if you were using it before)
//    if (Input::IsKeyPressed(KEY_SPACE)) {
//        rb->LinearVelocity.Y += jumpImpulse;
//    }
//
//    // Light velocity damp
//    rb->LinearVelocity *= velDampScale;
//
//    // Apply transform scale to collision radius
//    const float r = cc->Radius * ((tr.Scale.X + tr.Scale.Y) * 0.5f);
//
//    // Clamp within world bounds and kill inward velocity on impact
//    if (tr.Position.X - r <= 0.0f) {
//        tr.Position.X = r;
//        if (rb->LinearVelocity.X < 0.0f) rb->LinearVelocity.X = 0.0f;
//    }
//    else if (tr.Position.X + r >= m_worldWidth) {
//        tr.Position.X = m_worldWidth - r;
//        if (rb->LinearVelocity.X > 0.0f) rb->LinearVelocity.X = 0.0f;
//    }
//
//    if (tr.Position.Y - r <= 0.0f) {
//        tr.Position.Y = r;
//        if (rb->LinearVelocity.Y < 0.0f) rb->LinearVelocity.Y = 0.0f;
//    }
//    else if (tr.Position.Y + r >= m_worldHeight) {
//        tr.Position.Y = m_worldHeight - r;
//        if (rb->LinearVelocity.Y > 0.0f) rb->LinearVelocity.Y = 0.0f;
//    }
//
//    // Keep the visual in sync (in case something else changed it)
//    if (sh) {
//        sh->Type = Component::ShapeRenderer2D::ShapeType::Circle;
//        sh->Radius = cc->Radius; // Keep visual at base radius
//
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_spawnBalls(ECS::World& world, const int count, const unsigned seed) {
//    m_balls.clear();
//    m_balls.reserve(count);
//
//    for (int i = 0; i < count; ++i) {
//        float radius = MathUtils::Randomize<float>(18.0f, 34.0f, seed);
//
//        const float hue = MathUtils::Randomize<float>(0.0f, 1.0f, seed);
//        const Color color(
//            0.5f + 0.5f * std::cos(TWO_PI * hue + 0.0f),
//            0.5f + 0.5f * std::cos(TWO_PI * hue + 2.094f),
//            0.5f + 0.5f * std::cos(TWO_PI * hue + 4.188f),
//            1.0f
//        );
//
//        ECS::Entity ball = world.Create( 
//            ECS::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
//            ECS::Components::Rigidbody2D{},
//            ECS::Components::CircleCollider2D{ radius },
//            ECS::Components::Name{ "Ball" }
//        );
//
//        const float minX = radius;
//        const float maxX = m_worldWidth - radius;
//        const float minY = radius;
//        const float maxY = m_worldHeight - radius;
//
//        const float x = (maxX > minX) ? MathUtils::Randomize<float>(minX, maxX, seed)
//            : 0.5f * m_worldWidth;
//        const float y = (maxY > minY) ? MathUtils::Randomize<float>(minY, maxY, seed)
//            : 0.5f * m_worldHeight;
//
//        auto& transform = world.Get<ECS::LocalTransform>(ball);
//        transform.Position.X = x;
//        transform.Position.Y = y;
//
//        auto& rigidbody = world.Get<ECS::Components::Rigidbody2D>(ball);
//        rigidbody.Mass = 1.0f;
//        rigidbody.LinearDamping = 0.0f;
//        rigidbody.GravityScale = 1.0f;
//
//        const float speed = MathUtils::Randomize<float>(200.0f, 300.0f, seed);
//        const float angle = MathUtils::Randomize<float>(0.0f, TWO_PI, seed);
//        rigidbody.LinearVelocity.X = std::cos(angle) * speed;
//        rigidbody.LinearVelocity.Y = std::sin(angle) * speed;
//
//        auto& shapeRenderer = ball.AddComponent<Component::ShapeRenderer2D>();
//        shapeRenderer.Type = Component::ShapeRenderer2D::ShapeType::Circle;
//        shapeRenderer.Radius = radius;
//        shapeRenderer.FillColor = color;
//
//        ball.AddComponent<Component::CircleCollider2D>(radius);
//
//        m_balls.push_back(ball);
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_spawnCubes() {
//    if ((int)m_seacubes.size() >= m_maxLeaves) return;
//
//    const float size = MathUtils::Randomize<float>(14.0f, 28.0f, 0);
//    const float half = size * 0.5f;
//    const float x = MathUtils::Randomize<float>(half, m_worldWidth - half, 0);
//    const float y = m_worldHeight - half - 2.0f;
//
//    ECS::Entity cube = world.Create( 
//        ECS::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
//        ECS::Components::Rigidbody2D{},
//        ECS::Components::Name{ "SeaCube" }
//    );
//
//    auto& tr = world.Get<ECS::LocalTransform>(cube);
//    tr.Position.X = x;
//    tr.Position.Y = y;
//
//    // Rigidbody (dynamic so gravity applies)
//    auto& rb = world.Get<ECS::Components::Rigidbody2D>(cube);
//    rb.BodyType = ECS::Components::Rigidbody2D::Dynamic;
//    rb.Mass = 0.8f;
//    rb.GravityScale = 1.0f;
//    rb.LinearDamping = 0.25f;
//    rb.LinearVelocity.X = MathUtils::Randomize<float>(-20.0f, 20.0f, 0);
//    rb.LinearVelocity.Y = -5.0f;
//
//    world.AddComponent<ECS::Components::CircleCollider2D>(cube, size);
//
//
//    std::vector<Vector2D> pts;
//    pts.emplace_back(x - half, y - half);
//    pts.emplace_back(x - half, y + half);
//    pts.emplace_back(x + half, y + half);
//    pts.emplace_back(x + half, y - half);
//
//    auto& shape = cube.AddComponent<ShapeRenderer2D>();
//    shape.Type = ShapeRenderer2D::ShapeType::Polygon;
//    shape.Points = pts;
//    shape.Closed = true;
//    shape.FillColor = Color(0.15f, 0.65f, 0.95f, 1.0f);
//
//    m_seacubes.push_back(cube);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_spawnCubes_T(float dt) {
//    m_spawnAcc += dt;
//    while (m_spawnAcc >= m_spawnIntervals) {
//        m_spawnAcc -= m_spawnIntervals;
//        int batch = MathUtils::Randomize<int>(1, 7, 0);
//        for (int i = 0; i < batch; ++i) _spawnCubes();
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_cubeDisintegrate(ECS::World& world, size_t i) {
//    if (i >= m_seacubes.size()) return;
//    DestroyEntity(m_seacubes[i]);
//    m_seacubes[i] = m_seacubes.back();
//    m_seacubes.pop_back();
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_updateCubesCollisions(ECS::World& world) {
//
//    ECS::Entity hero = world.Create( 
//        ECS::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
//        Components::Name{ "HeroTriangle" }
//    );
//    m_playerId = EntityUtils::Pack(hero);
//    auto& ht = world.Get<ECS::LocalTransform>(hero);
//
//    // Apply transform scale to collision radius
//    const float heroR = m_triHalfHeight * ((ht.Scale.X + ht.Scale.Y) * 0.5f);
//    for (size_t i = 0; i < m_seacubes.size();) {
//        auto& ct = world.Get<ECS::LocalTransform>(m_seacubes[i]);
//        auto* rb = world.Get<ECS::Components::Rigidbody2D>(m_seacubes[i]);
//
//        float half;
//        if (auto* sh = world.Get<ECS::Components::ShapeRenderer2D>(m_seacubes[i])) {
//            if (sh->Type == ECS::Components::ShapeRenderer2D::ShapeType::Polygon && sh->Points.size() >= 4) {
//                float minX = sh->Points[0].X, maxX = sh->Points[0].X;
//                float minY = sh->Points[0].Y, maxY = sh->Points[0].Y;
//                for (auto& p : sh->Points) {
//                    if (p.X < minX) minX = p.X; if (p.X > maxX) maxX = p.X;
//                    if (p.Y < minY) minY = p.Y; if (p.Y > maxY) maxY = p.Y;
//                }
//                half = 0.5f * (std::max)(maxX - minX, maxY - minY);
//                const float x = ct.Position.X, y = ct.Position.Y;
//                sh->Points.clear();
//                sh->Points.emplace_back(x - half, y - half);
//                sh->Points.emplace_back(x - half, y + half);
//                sh->Points.emplace_back(x + half, y + half);
//                sh->Points.emplace_back(x + half, y - half);
//                sh->Closed = true;
//            }
//            else {
//                half = 10.0f;
//            }
//        }
//        else {
//            half = 10.0f;
//        }
//
//
//        if (m_currentTest == TestType::test_DynvStatResponse) {
//            if (ct.Position.Y - half <= m_worldHeight / 3.0f) {
//                _cubeDisintegrate(world, i);
//                continue;
//            }
//        }
//        else if (m_currentTest == TestType::test_StepbyStepUpdate) {
//            if (ct.Position.Y - half <= 0) {
//                _cubeDisintegrate(world, i);
//                continue;
//            }
//        }
//        else {
//            _cubeDisintegrate(world, i);
//            continue;
//        }
//        DynCol::Circle Chero{ Vector2D(ht.Position.X, ht.Position.Y), heroR };
//        DynCol::AABB   Bcube{ Vector2D(ct.Position.X - half, ct.Position.Y - half),
//                              Vector2D(ct.Position.X + half, ct.Position.Y + half) };
//
//        if (DynCol::Overlap(Chero, Bcube, nullptr)) {
//            _cubeDisintegrate(world, i);
//            continue;
//        }
//
//        ++i;
//        (void)rb;
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_createTriangle() {
//    if (m_playerId != UINT64_MAX) return;
//    ECS::World& world = GetWorld();
//
//    ECS::Entity e = world.Create( 
//        ECS::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
//        Components::Name{ "Triangle" }
//    );
//    m_playerId = EntityUtils::Pack(e);
//
//    const float cx = m_worldWidth * 0.25f;
//    const float cy = m_worldHeight * 0.50f;
//
//    auto& tr = world.Get<ECS::LocalTransform>(e);
//    tr.Position.X = cx;
//    tr.Position.Y = cy;
//
//    auto& rb = world.Get<ECS::Components::Rigidbody2D>(e);
//    rb.Mass = 2.0f;
//    rb.GravityScale = 1.0f;
//    rb.LinearDamping = 0.10f;
//
//    e.AddComponent<ECS::Components::CircleCollider2D>(m_triHalfHeight);
//
//    rb.LinearVelocity.X = 50.0f;
//    rb.LinearVelocity.Y = 0.0f;
//
//    std::vector<Vector2D> pts;
//    pts.emplace_back(cx + 0.0f, cy + m_triHalfHeight);
//    pts.emplace_back(cx - m_triHalfBase, cy - m_triHalfHeight);
//    pts.emplace_back(cx + m_triHalfBase, cy - m_triHalfHeight);
//
//    const Color fill(0.95f, 0.90f, 0.20f, 1.0f);
//    Component::ShapeRenderer2D triShape = Component::ShapeRenderer2D::Polygon(pts, fill, true);
//    e.AddComponent<Component::ShapeRenderer2D>(triShape);
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_clampAndBouncePlayer() {
//    if (m_playerId == UINT32_MAX) return;
//    World& world = GetWorld();
//
//    Entity player(m_playerId, &world);
//    auto& tr = player.Transform();
//    auto* rb = player.GetComponent<Rigidbody2D>();
//    auto* shape = player.GetComponent<ShapeRenderer2D>();
//    if (!rb) return;
//
//    const float thrust = 1.0f;
//    const float jumpImpulse = 20.0f;
//    const float velDampScale = 0.996f;
//
//    bool leftHeld = Input::IsKeyDown(KEY_A);
//    bool rightHeld = Input::IsKeyDown(KEY_D);
//    bool upHeld = Input::IsKeyDown(KEY_W);
//    bool downHeld = Input::IsKeyDown(KEY_S);
//
//    Vector2D F(0.0f, 0.0f);
//    if (leftHeld)  F.X -= thrust;
//    if (rightHeld) F.X += thrust;
//    if (upHeld)    F.Y += thrust;
//    if (downHeld)  F.Y -= thrust;
//
//    if (F.X != 0.0f || F.Y != 0.0f) {
//        Engine::PhysicsSystem::AddForce(*rb, F);
//    }
//
//    if (Input::IsMousePressed(GLFW_MOUSE_BUTTON_LEFT)) {
//        Engine::PhysicsSystem::AddImpulse(*rb, Vector2D(0.0f, jumpImpulse));
//        std::cout << jumpImpulse;
//    }
//    rb->LinearVelocity *= velDampScale;
//
//    // Apply transform scale to collision radius
//    const float r = m_triHalfHeight * ((tr.Scale.X + tr.Scale.Y) * 0.5f);
//
//    if (tr.Position.X - r <= 0.0f) {
//        tr.Position.X = r;
//        if (rb->LinearVelocity.X < 0.0f) rb->LinearVelocity.X = 0.0f;
//    }
//    else if (tr.Position.X + r >= m_worldWidth) {
//        tr.Position.X = m_worldWidth - r;
//        if (rb->LinearVelocity.X > 0.0f) rb->LinearVelocity.X = 0.0f;
//    }
//
//    if (tr.Position.Y - r <= 0.0f) {
//        tr.Position.Y = r;
//        if (rb->LinearVelocity.Y < 0.0f) rb->LinearVelocity.Y = 0.0f;
//    }
//    else if (tr.Position.Y + r >= m_worldHeight) {
//        tr.Position.Y = m_worldHeight - r;
//        if (rb->LinearVelocity.Y > 0.0f) rb->LinearVelocity.Y = 0.0f;
//    }
//
//    std::vector<Vector2D> pts;
//    pts.emplace_back(tr.Position.X + 0.0f, tr.Position.Y + m_triHalfHeight);
//    pts.emplace_back(tr.Position.X - m_triHalfBase, tr.Position.Y - m_triHalfHeight);
//    pts.emplace_back(tr.Position.X + m_triHalfBase, tr.Position.Y - m_triHalfHeight);
//
//    shape->Type = ShapeRenderer2D::ShapeType::Polygon;
//    shape->Points = pts;
//    shape->Closed = true;
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_handleStepByStepControls() {
//    static bool pWasDown = false;
//    static bool spaceWasDown = false;
//    static bool wasPaused = false;
//
//    bool pIsDown = Input::IsKeyDown(KEY_P);
//    bool spaceIsDown = Input::IsKeyDown(KEY_SPACE);
//
//    // toggle step-by-step mode with P key
//    if (pIsDown && !pWasDown) {
//        m_stepByStepMode = !m_stepByStepMode;
//        m_pausePhysics = m_stepByStepMode;
//
//        Engine::PhysicsSystem::SetEnabled(!m_pausePhysics);
//
//        if (m_pausePhysics) {
//            std::cout << "Step-by-step physics mode ENABLED" << '\n';
//            _storeBallStates();
//        }
//        else {
//            std::cout << "Step-by-step physics mode DISABLED" << '\n';
//            _restoreBallStates();
//        }
//    }
//
//    // step physics with SPACE key
//    if ((m_stepByStepMode || m_pausePhysics) && spaceIsDown && !spaceWasDown) {
//        if (m_pausePhysics) {
//            Engine::PhysicsSystem::SetEnabled(true);
//            m_stepRequested = true;
//            std::cout << "Physics step requested" << '\n';
//        }
//    }
//
//    pWasDown = pIsDown;
//    spaceWasDown = spaceIsDown;
//    wasPaused = m_pausePhysics;
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_storeBallStates() {
//    m_storedBallStates.clear();
//    for (auto& ball : m_balls) {
//        auto* rigidbody = ball.GetComponent<Component::Rigidbody2D>();
//        if (rigidbody) {
//            m_storedBallStates[ball.GetId()] = {
//                rigidbody->LinearVelocity,
//                ball.Transform().Position
//            };
//        }
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_restoreBallStates() {
//    for (auto& ball : m_balls) {
//        auto it = m_storedBallStates.find(ball.GetId());
//        if (it != m_storedBallStates.end()) {
//            auto* rigidbody = ball.GetComponent<Component::Rigidbody2D>();
//            if (rigidbody) {
//                rigidbody->LinearVelocity = it->second.velocity;
//                ball.Transform().Position = it->second.position;
//            }
//        }
//    }
//    m_storedBallStates.clear();
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_ballCollide() {
//    World& world = GetWorld();
//    auto& em = world.GetEntityManager();
//
//  
//
//    const size_t n = m_balls.size();
//    for (size_t i = 0; i < n; ++i) {
//        Entity& a = m_balls[i];
//        if (!em.IsAlive(a)) continue;
//
//        auto* rba = a.GetComponent<Component::Rigidbody2D>();
//        auto* cca = a.GetComponent<Component::CircleCollider2D>();
//        if (!rba || !cca) continue;
//
//        auto& ta = a.Transform();
//        // Apply transform scale to collision radius
//        const float ra = cca->Radius * ((ta.Scale.X + ta.Scale.Y) * 0.5f);
//        const float invMa = (rba->Mass > 0.0f) ? 1.0f / rba->Mass : 0.0f;
//
//        for (size_t j = i + 1; j < n; ++j) {
//            Entity& b = m_balls[j];
//            if (!em.IsAlive(b)) continue;
//
//            auto* rbb = b.GetComponent<Component::Rigidbody2D>();
//            auto* ccb = b.GetComponent<Component::CircleCollider2D>();
//            if (!rbb || !ccb) continue;
//
//            auto& tb = b.Transform();
//            // Apply transform scale to collision radius
//            const float rb = ccb->Radius * ((tb.Scale.X + tb.Scale.Y) * 0.5f);
//            const float invMb = (rbb->Mass > 0.0f) ? 1.0f / rbb->Mass : 0.0f;
//
//            // Skip if both are immovable
//            const float invMassSum = invMa + invMb;
//            if (invMassSum == 0.0f) continue;
//
//            DynCol::Circle A{ ta.Position, ra };
//            DynCol::Circle B{ tb.Position, rb };
//            DynCol::Manifold m{};
//
//            if (!DynCol::Overlap(A, B, &m) || !m.valid) continue;
//            {
//                const Vector2D va = rba->LinearVelocity;
//                const Vector2D vb = rbb->LinearVelocity;
//                const Vector2D rv = vb - va;
//
//                const float velN = rv.X * m.normal.X + rv.Y * m.normal.Y;
//                if (velN > 0.0f) continue; // already separating along normal
//
//                const float e = std::clamp(m_restitution, 0.0f, 1.0f);
//                const float z = -(1.0f + e) * velN / invMassSum;
//                const Vector2D J = m.normal * z;
//
//                rba->LinearVelocity -= J * invMa;
//                rbb->LinearVelocity += J * invMb;
//            }
//
//            // friction
//            if (m_friction > 0.0f) {
//                // Recompute relative velocity after normal impulse
//                const Vector2D rv2 = rbb->LinearVelocity - rba->LinearVelocity;
//
//                const float vn2 = rv2.X * m.normal.X + rv2.Y * m.normal.Y;
//                Vector2D t = { rv2.X - m.normal.X * vn2, rv2.Y - m.normal.Y * vn2 };
//                const float tlen2 = t.X * t.X + t.Y * t.Y;
//
//                if (tlen2 > 1e-12f) {
//                    t = t / std::sqrt(tlen2);
//
//                    const float jt = -(rv2.X * t.X + rv2.Y * t.Y) / invMassSum;
//                    const float mu = std::max(0.0f, m_friction);
//
//                    // Use normal impulse magnitude from above as scale; recompute cheapest way:
//                    // normal impulse magnitude equals |(vb-va)·n|*(1+e)/invMassSum for this contact.
//                    const Vector2D va = rba->LinearVelocity;
//                    const Vector2D vb = rbb->LinearVelocity;
//                    const float velN_after = (vb.X - va.X) * m.normal.X + (vb.Y - va.Y) * m.normal.Y;
//                    const float jn_mag = std::abs(velN_after) / invMassSum; // sufficient for Coulomb clamp
//
//                    Vector2D Jt;
//                    if (std::abs(jt) < jn_mag * mu) {
//                        // static friction
//                        Jt = t * jt;
//                    }
//                    else {
//                        // dynamic friction
//                        Jt = t * (-jn_mag * mu * (jt < 0 ? -1.0f : 1.0f));
//                    }
//
//                    rba->LinearVelocity -= Jt * invMa;
//                    rbb->LinearVelocity += Jt * invMb;
//                }
//            }
//        }
//    }
//}
//
//void Sandbox::PhysicsCollision2DTestScene::_updateBallCollisions() {
//    World& world = GetWorld();
//    std::vector<EntityId> allEntities = world.GetEntityManager().GetAllEntities();
//
//    for (EntityId id : allEntities) {
//        Entity ball = world.GetEntityManager().GetEntity(id);
//        auto* rigidbody = ball.GetComponent<Component::Rigidbody2D>();
//        const auto* circleCollider = ball.GetComponent<Component::CircleCollider2D>();
//        auto& transform = ball.Transform();
//
//        if (!rigidbody || !circleCollider)
//            continue;
//
//        // Apply transform scale to collision radius
//        const float radius = circleCollider->Radius * ((transform.Scale.X + transform.Scale.Y) * 0.5f);
//
//        bool bounced = false;
//
//        if (transform.Position.X - radius <= 0.0f) {
//            transform.Position.X = radius;
//            rigidbody->LinearVelocity.X = std::abs(rigidbody->LinearVelocity.X);
//            bounced = true;
//        }
//        else if (transform.Position.X + radius >= m_worldWidth) {
//            transform.Position.X = m_worldWidth - radius;
//            rigidbody->LinearVelocity.X = -std::abs(rigidbody->LinearVelocity.X);
//            bounced = true;
//        }
//
//        if (transform.Position.Y - radius <= 0.0f) {
//            transform.Position.Y = radius;
//            rigidbody->LinearVelocity.Y = std::abs(rigidbody->LinearVelocity.Y);
//            bounced = true;
//        }
//        else if (transform.Position.Y + radius >= m_worldHeight) {
//            transform.Position.Y = m_worldHeight - radius;
//            rigidbody->LinearVelocity.Y = -std::abs(rigidbody->LinearVelocity.Y);
//            bounced = true;
//        }
//
//        (void)bounced;
//    }
//}
