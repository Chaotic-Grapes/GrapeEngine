#include "ecs\systems\PhysicsSystem.h"
#include "ecs\systems\RendererSystem.h"
#include "PhysicsTest.h"
#include "services/Input.h"
#include "core/Application.h"
#include "ecs/Components.h"
#include "ecs/Entity.h"
#include "services/Time.h"
#include "services/Window.h"
#include "services/WindowManager.h"
#include "services/ResourceManager.h"
#include "helpers/EntityUtils.h"

using namespace ECS;
using namespace Sandbox;

void PhysicsTestScene::OnLoad() {
    // Seed random number generator
    srand(static_cast<unsigned int>(time(nullptr)));

    const auto& config = Engine::CORE->GetConfig();
    const int windowWidth = config.WindowConfig.Width;
    const int windowHeight = config.WindowConfig.Height;

    CREATE_WINDOW("Physics Test Scene", windowWidth, windowHeight);
    worldWidth = static_cast<float>(windowWidth);
    worldHeight = static_cast<float>(windowHeight);

    // Create a test layer 
    m_testLayer = GetLayers().CreateOrGetLayer("physics_test");

    // Initialize renderer system
    m_rendererSystem = std::make_shared<ECS::RendererSystem>();
    m_rendererSystem->Initialize(GetWorld());
    AddSystem([this](Scenes::Scene& s, const float dt) {
        m_rendererSystem->Update(s.GetWorld(), dt);
        }, "Renderer System");

    // Initialize physics system
    AddSystem([](Scenes::Scene& s, const float dt) {
        ECS::PhysicsSystem::Update(s.GetWorld(), dt);
        }, "Physics System");

    //set current test to ->
    currentTest = Tests::PhysicsCollisionResponse;

    LOG_DEBUG("=== Physics Test Scene Loaded ===");
    LOG_DEBUG("Press 'C' to cycle through tests");
}

void PhysicsTestScene::OnUpdate() {
    if (Input::IsKeyDown(KEY_T)) {
        if (!testHandler) {

            //test cycling logic
            int current = static_cast<int>(currentTest);
            current++;
            // if current overflows over test 3 it sets back to test 1 
            if (current > static_cast<int>(Tests::BroadNarrowPhaseCollision)) {
                current = static_cast<int>(Tests::PhysicsCollisionResponse);
            }

            // clear entities before next tests
            DestroyEntities();
            //set currentTest to current
            currentTest = static_cast<Tests>(current);
            // set to true to prevent rerunning of this loop 
            testHandler = true;

            // Log test switch
            switch (currentTest) {
            case Tests::PhysicsCollisionResponse:
                LOG_DEBUG("Switched to: Physics Collision Response Test");
                break;
            case Tests::PhysicsForces:
                LOG_DEBUG("Switched to: Physics Forces Test");
                break;
            case Tests::BroadNarrowPhaseCollision:
                LOG_DEBUG("Switched to: Broad & Narrow Phase Collision Test");
                break;
            }
        }
    }
    // when key is not down reset handler
    else {
        testHandler = false;
    }

    //switch case to switch around test cases
    switch (currentTest) {
    case Tests::PhysicsCollisionResponse:
        PhysicsCollisionResponse();
        break;
    case Tests::PhysicsForces:
        PhysicsForces();
        break;
    case Tests::BroadNarrowPhaseCollision:
        BroadNarrowPhaseCollision();
        break;
    }
}

void PhysicsTestScene::OnUnload() {
    //exit destroy entities
    DestroyEntities();
    LOG_DEBUG("=== Physics Test Scene Unloaded ===");
}

// Helper function to generate random float in range
inline float RandomFloat(float min, float max) {
    float random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return min + random * (max - min);
}

//helper function during unloading
void PhysicsTestScene::DestroyEntities() {
    ECS::World& world = GetWorld();
    for (const uint64_t id : testEntities) {
        const ECS::Entity e = EntityUtils::Unpack(id);
        if (world.IsAlive(e)) {
            world.Destroy(e);
        }
    }
    //clear vector
    testEntities.clear();
}

//helper function to create static walls
void PhysicsTestScene::CreateStaticWall(float x, float y, float width, float height) {
    Entity wall = CreateOnLayer(m_testLayer,
        Components::LocalTransform{
            Vector3D(x, y, 0.0f),
            Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
            Vector3D(1.0f, 1.0f, 1.0f)
        },
        Components::Rigidbody2D{
            0.0f,       // Mass = 0 (static)
            0.0f,       // InverseMass
            0.0f,       // LinearDamping
            0.0f,       // AngularDamping
            0.0f,       // GravityScale
            0           // Flags
        },
        Components::LinearVelocity2D{ 
            Vector2D(0.0f, 0.0f)
        },
        Components::PhysicsMaterial2D{  
            0.2f,       // Friction
            0.8f,       // Restitution (bounciness)
            0.5f        // PositionCorrectPercent
        },
        Components::BoxCollider2D{
            Vector2D(width / 2.0f, height / 2.0f),
            Vector2D(0.0f, 0.0f), 0.0f, 0xFFFFFFFF, 0
        },
        Components::ShapeBox2D{
            Vector2D(width / 2.0f, height / 2.0f),
            Vector2D(0.0f, 0.0f),
            Color(0.2f, 0.2f, 0.2f, 1.0f),
            1.0f, true
        },
        Components::Active{ true }
    );

    testEntities.push_back(EntityUtils::Pack(wall));
}

// test 2 -  Forces, Gravity, and Friction
void PhysicsTestScene::PhysicsCollisionResponse() {
    if (testEntities.empty()) {
        LOG_DEBUG("=== TEST 1: Physics Collision Response ===");
        LOG_DEBUG("Testing elastic and inelastic collisions");
        LOG_DEBUG("Colors: Red = low bounce, Green = high bounce");

        // Create world boundaries
        CreateStaticWall(worldWidth / 2.0f, 10.0f, worldWidth, 20.0f);
        CreateStaticWall(worldWidth / 2.0f, worldHeight - 10.0f, worldWidth, 20.0f);
        CreateStaticWall(10.0f, worldHeight / 2.0f, 20.0f, worldHeight);
        CreateStaticWall(worldWidth - 10.0f, worldHeight / 2.0f, 20.0f, worldHeight);

        // Create grid of bouncing balls with varying restitution
        const int rows = 5;
        const int cols = 10;
        const float spacing = 100.0f;
        const float startX = 300.0f;
        const float startY = 200.0f;

        for (int row = 0; row < rows; ++row) {
            float restitution = 0.1f + (row * 0.2f);

            for (int col = 0; col < 10; ++col) {
                float xPos = startX + col * spacing;
                float yPos = startY + row * spacing;

                Entity e = CreateOnLayer(m_testLayer,
                    Components::LocalTransform{
                        Vector3D(xPos, yPos, 0.0f),
                        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                        Vector3D(1.0f, 1.0f, 1.0f)
                    },
                    Components::Rigidbody2D{
                        1.5f, 1.0f / 1.5f, 0.1f, 0.1f, 1.0f, 0
                    },
                    Components::LinearVelocity2D{
                        Vector2D(RandomFloat(-700.0f, 700.0f), RandomFloat(-700.0f, 700.0f))
                    },
                    Components::AngularVelocity2D{ RandomFloat(-500.0f, 500.0f) },
                    Components::PhysicsMaterial2D{  // ADD THIS!
                        0.2f,           // Friction
                        restitution,    // Restitution (varies by row)
                        0.5f            // PositionCorrectPercent
                    },
                    Components::CircleCollider2D{
                        15.0f, Vector2D(0.0f, 0.0f), 0xFFFFFFFF, 0
                    },
                    Components::ShapeCircle2D{
                        25.0f, Vector2D(0.0f, 0.0f),
                        Color(1.0f - restitution, restitution, 0.5f, 1.0f),
                        1.0f, true
                    },
                    Components::Active{ true }
                );

                testEntities.push_back(EntityUtils::Pack(e));
            }
        }

        LOG_DEBUG("Spawned " << (rows * cols) << " entities with varying restitution");
    }
}

// test 2 -  Forces, Gravity, and Friction
void PhysicsTestScene::PhysicsForces() {
    if (testEntities.empty()) {
        LOG_DEBUG("=== TEST 2: Physics Forces ===");
        LOG_DEBUG("Testing gravity, friction, and mass interactions");

        // Create ground platform
        Entity ground = CreateOnLayer(m_testLayer,
            Components::LocalTransform{
                Vector3D(worldWidth / 2.0f, 100.0f, 0.0f),
                Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                Vector3D(1.0f, 1.0f, 1.0f)
            },
            Components::Rigidbody2D{
                0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0
            },
            Components::BoxCollider2D{
                Vector2D((worldWidth - 100.0f) / 2.0f, 20.0f),
                Vector2D(0.0f, 0.0f), 0.0f, 0xFFFFFFFF, 0
            },
            Components::PhysicsMaterial2D{  // ADD THIS!
                        0.2f,           // Friction
                        0.0f,    // Restitution (varies by row)
                        0.5f            // PositionCorrectPercent
            },
            Components::ShapeBox2D{
                Vector2D((worldWidth - 100.0f) / 2.0f, 20.0f),
                Vector2D(0.0f, 0.0f),
                Color(0.3f, 0.3f, 0.3f, 1.0f), 1.0f, true
            },
            Components::Active{ true }
        );
        testEntities.push_back(EntityUtils::Pack(ground));

        // Create boxes with varying friction
        const int boxCount = 15;
        for (int i = 0; i < boxCount; ++i) {
            float friction = static_cast<float>(i) / static_cast<float>(boxCount - 1);
            float xPos = 200.0f + (i * 80.0f);

            Entity box = CreateOnLayer(m_testLayer,
                Components::LocalTransform{
                    Vector3D(xPos, 500.0f, 0.0f),
                    Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                    Vector3D(1.0f, 1.0f, 1.0f)
                },
                Components::Rigidbody2D{
                    2.0f, 0.5f, friction, 0.3f, 1.0f, 2
                },
                Components::LinearVelocity2D{ Vector2D(0.0f, 0.0f) },
                Components::AngularVelocity2D{ 0.0f },
                Components::BoxCollider2D{
                    Vector2D(15.0f, 15.0f), Vector2D(0.0f, 0.0f), 0.0f, 0xFFFFFFFF, 0
                },
                Components::ShapeBox2D{
                    Vector2D(15.0f, 15.0f), Vector2D(0.0f, 0.0f),
                    Color(friction, 0.5f, 1.0f - friction, 1.0f), 1.0f, true
                },
                Components::PhysicsMaterial2D{  // ADD THIS!
                        0.2f,           // Friction
                        0.5f,    // Restitution (varies by row)
                        0.5f            // PositionCorrectPercent
                },
                Components::Active{ true }
            );
            testEntities.push_back(EntityUtils::Pack(box));
        }

        // Create falling balls with different masses
        for (int i = 0; i < 23; ++i) {
            float mass = 1.5f + (i * 0.5f);
            float radius = 20.0f;

            Entity ball = CreateOnLayer(m_testLayer,
                Components::LocalTransform{
                    Vector3D(RandomFloat(200.0f, worldWidth - 200.0f), 700.0f, 0.0f),
                    Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                    Vector3D(1.0f, 1.0f, 1.0f)
                },
                Components::Rigidbody2D{
                    mass, 1.0f / mass, 0.1f, 0.1f, 1.0f, 2
                },
                Components::LinearVelocity2D{ Vector2D(0.0f, 0.0f) },
                Components::AngularVelocity2D{ 0.0f },
                Components::CircleCollider2D{
                    radius, Vector2D(0.0f, 0.0f), 0xFFFFFFFF, 0
                },
                Components::PhysicsMaterial2D{  // ADD THIS!
                        0.2f,           // Friction
                        1.2f,    // Restitution (varies by row)
                        1.5f            // PositionCorrectPercent
                },
                Components::ShapeCircle2D{
                    radius, Vector2D(0.0f, 0.0f),
                    Color(0.8f, 0.3f, 0.3f, 1.0f), 1.0f, true
                },
                Components::Active{ true }
            );
            testEntities.push_back(EntityUtils::Pack(ball));
        }

        LOG_DEBUG("Spawned " << (boxCount + 10) << " entities testing forces and friction");
    }
}

// test 3 - broad & narrow Phase Collision Detection Stress Test
void PhysicsTestScene::BroadNarrowPhaseCollision() {
    if (testEntities.empty()) {
        LOG_DEBUG("=== TEST 3: Broad & Narrow Phase Collision ===");
        LOG_DEBUG("Stress testing collision detection performance");

        // Create boundary walls
        CreateStaticWall(worldWidth / 2.0f, 10.0f, worldWidth, 20.0f);
        CreateStaticWall(worldWidth / 2.0f, worldHeight - 10.0f, worldWidth, 20.0f);
        CreateStaticWall(10.0f, worldHeight / 2.0f, 20.0f, worldHeight);
        CreateStaticWall(worldWidth - 10.0f, worldHeight / 2.0f, 20.0f, worldHeight);

        // Spawn many moving entities
        const int count = 300;
        for (int i = 0; i < count; ++i) {
            Entity e = CreateOnLayer(m_testLayer,
                Components::LocalTransform{
                    Vector3D(
                        RandomFloat(100.0f, worldWidth - 100.0f),
                        RandomFloat(100.0f, worldHeight - 100.0f),
                        0.0f
                    ),
                    Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                    Vector3D(1.0f, 1.0f, 1.0f)
                },
                Components::Rigidbody2D{
                    1.5f, 1.0f / 1.5f, 0.1f, 0.1f, 0.5f, 0
                },
                Components::LinearVelocity2D{
                    Vector2D(RandomFloat(-200.0f, 200.0f), RandomFloat(-200.0f, 200.0f))
                },
                Components::AngularVelocity2D{ RandomFloat(-180.0f, 180.0f) },
                Components::CircleCollider2D{
                    12.0f, Vector2D(0.0f, 0.0f), 0xFFFFFFFF, 0
                },
                Components::PhysicsMaterial2D{  // ADD THIS!
                        0.2f,           // Friction
                        0.3f,    // Restitution (varies by row)
                        0.5f            // PositionCorrectPercent
                },
                Components::ShapeCircle2D{
                    12.0f, Vector2D(0.0f, 0.0f),
                    Color(RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), RandomFloat(0.0f, 1.0f), 0.8f),
                    1.0f, true
                },
                Components::Active{ true }
            );
            testEntities.push_back(EntityUtils::Pack(e));
        }

        // Add static obstacles
        for (int i = 0; i < 10; ++i) {
            Entity obstacle = CreateOnLayer(m_testLayer,
                Components::LocalTransform{
                    Vector3D(
                        RandomFloat(200.0f, worldWidth - 200.0f),
                        RandomFloat(200.0f, worldHeight - 200.0f),
                        0.0f
                    ),
                    Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                    Vector3D(1.0f, 1.0f, 1.0f)
                },
                Components::Rigidbody2D{ 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0 },

                Components::CircleCollider2D{
                    50.0f, Vector2D(0.0f, 0.0f), 0xFFFFFFFF, 0
                },
                Components::PhysicsMaterial2D{  // ADD THIS!
                        0.2f,           // Friction
                        0.2f,    // Restitution (varies by row)
                        0.5f            // PositionCorrectPercent
                },
                Components::ShapeCircle2D{
                    50.0f, Vector2D(0.0f, 0.0f),
                    Color(0.5f, 0.5f, 0.5f, 1.0f), 1.0f, true
                },
                Components::Active{ true }
            );
            testEntities.push_back(EntityUtils::Pack(obstacle));
        }

        LOG_DEBUG("Spawned " << (count + 10) << " entities for collision stress test");
    }
}