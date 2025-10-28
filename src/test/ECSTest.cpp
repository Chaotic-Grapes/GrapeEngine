/* Start Header *****************************************************************/
/*!
\file   ECSTest.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th October 2025
\brief
Implements the ECSTestScene, a comprehensive testbed for validating all ECS
systems, components, and their interactions. This test scene covers:

- PhysicsSystem: 2D rigidbody physics, collisions, velocity, acceleration
- RendererSystem: Shape rendering, sprites, layers, z-ordering
- LifetimeSystem: Entity destruction after timeout
- Transform hierarchy and WorldTransform updates
- Component interactions (Active, Layer, TagMask, etc.)
- Performance and stress testing for ECS operations

The scene provides structured test cases that can be cycled through using
keyboard input (T key), with each test validating specific ECS functionality.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ECSTest.hpp"
#include "core/Application.h"
#include "core/Logger.h"
#include "ecs/Components.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "ecs/systems/LifetimeSystem.h"
#include "ecs/systems/TransformSystem.h"
#include "physics/Physics.h"
#include "helpers/EntityUtils.h"
#include "helpers/MathUtils.h"
#include "services/Input.h"
#include "services/Time.h"
#include "services/Window.h"
#include "services/WindowManager.h"
#include "services/ResourceManager.h"
#include <glm/glm.hpp>
#include <iostream>

using namespace Sandbox;
using namespace ECS;

extern ResourceManager RM;

// ================================================================================================
// SCENE LIFECYCLE
// ================================================================================================

void ECSTestScene::OnLoad() {
    const auto& config = Engine::CORE->GetConfig();
    const int windowWidth = config.WindowConfig.Width;
    const int windowHeight = config.WindowConfig.Height;

    CREATE_WINDOW("ECS Test Scene", windowWidth, windowHeight);
    m_worldWidth = static_cast<float>(windowWidth);
    m_worldHeight = static_cast<float>(windowHeight);

    // Create test layers
    m_boundaryLayer = GetLayers().CreateOrGetLayer("boundary");
    m_testLayer = GetLayers().CreateOrGetLayer("test");
    m_physicsLayer = GetLayers().CreateOrGetLayer("physics");
    m_renderLayer = GetLayers().CreateOrGetLayer("render");

    // Initialize renderer system
    m_rendererSystem = std::make_shared<ECS::RendererSystem>();
    m_rendererSystem->Initialize();
    AddSystem([this](Scenes::Scene& s, const float dt) {
        m_rendererSystem->Update(s.GetWorld(), dt);
    }, "Renderer System");

    // Initialize physics system
    AddSystem([](Scenes::Scene& s, const float dt) {
        ECS::PhysicsSystem::Update(s.GetWorld(), dt);
    }, "Physics System");

    // Initialize lifetime system
    AddSystem([](Scenes::Scene& s, const float dt) {
        ECS::LifetimeSystem::Update(s.GetWorld(), dt);
    }, "Lifetime System");

    // Configure physics world boundaries (account for wall thickness)
    const float wallThickness = 20.0f;
    Engine::Physics::SetWorldBounds(wallThickness, m_worldWidth - wallThickness, wallThickness, m_worldHeight - wallThickness, false, 0.8f);
    Engine::Physics::EnableWorldBounds(true);

    // Create visual boundary walls
    _createWorldBoundaries();

    m_currentTest = TestType::BasicEntityCreation;
    LOG_INFO("ECSTestScene initialized with " << GetSystemCount() << " systems");
    _logTestInfo("BasicEntityCreation");
}

void ECSTestScene::OnUpdate() {
    // Cycle through test types with T key
    if (Input::IsKeyDown(KEY_T)) {
        if (!m_tHandled) {
            _cycleTests();
            m_tHandled = true;
        }
    }
    else {
        m_tHandled = false;
    }

    // Update test timer
    m_testTimer += Time::DeltaTime();

    // Run the current test
    switch (m_currentTest) {
        // Core Component Tests
        case TestType::BasicEntityCreation:     _testBasicEntityCreation(); break;
        case TestType::ComponentModification:   _testComponentModification(); break;
        case TestType::LayerSystem:             _testLayerSystem(); break;
        case TestType::ActiveAndTags:           _testActiveAndTags(); break;
        
        // Transform Tests
        case TestType::LocalTransformTest:      _testLocalTransform(); break;
        case TestType::WorldTransformTest:      _testWorldTransform(); break;
        case TestType::TransformInterpolation:  _testTransformInterpolation(); break;
        
        // Physics System Tests
        case TestType::PhysicsBasic:            _testPhysicsBasic(); break;
        case TestType::PhysicsGravity:          _testPhysicsGravity(); break;
        case TestType::PhysicsCollision:        _testPhysicsCollision(); break;
        case TestType::PhysicsMaterial:         _testPhysicsMaterial(); break;
        case TestType::PhysicsAngular:          _testPhysicsAngular(); break;
        case TestType::PhysicsComplex:          _testPhysicsComplex(); break;
        
        // Renderer System Tests
        case TestType::RenderShapes:            _testRenderShapes(); break;
        case TestType::RenderSprites:           _testRenderSprites(); break;
        case TestType::RenderLayers:            _testRenderLayers(); break;
        case TestType::RenderStressTest:        _testRenderStressTest(); break;
        
        // Lifetime System Tests
        case TestType::LifetimeBasic:           _testLifetimeBasic(); break;
        case TestType::LifetimeWithPhysics:     _testLifetimeWithPhysics(); break;
        case TestType::LifetimeSpawner:         _testLifetimeSpawner(); break;
        
        // Integration Tests
        case TestType::PhysicsRenderCombo:      _testPhysicsRenderCombo(); break;
        case TestType::AllSystemsTest:          _testAllSystems(); break;
        case TestType::StressTestAll:           _testStressTestAll(); break;
        
        // Advanced Tests
        case TestType::EntityPooling:           _testEntityPooling(); break;
        case TestType::ComponentIteration:      _testComponentIteration(); break;
        case TestType::ArchetypeChanges:        _testArchetypeChanges(); break;
    }

    if (Time::FrameCount() % 120 == 0) {
        // Log renderer flush count every 120 frames and FPS
        LOG_INFO("Renderer flushes this frame: " << m_rendererSystem->GetFlushCount()
                 << " | FPS: " << static_cast<int>(1.0f / Time::DeltaTime()));
    }
}

void ECSTestScene::OnUnload() {
    _clearTestEntities();
    LOG_INFO("ECSTestScene shutting down");
}

// ================================================================================================
// HELPER METHODS
// ================================================================================================

void ECSTestScene::_clearTestEntities() {
    const ECS::World& world = GetWorld();
    for (const uint64_t id : m_testEntities) {
        const ECS::Entity e = EntityUtils::Unpack(id);
        if (world.IsAlive(e)) {
            DestroyEntity(e);
        }
    }
    m_testEntities.clear();
    m_testTimer = 0.f;
    m_spawnCount = 0;
}

void ECSTestScene::_logTestInfo(const char* testName) {
    LOG_INFO("========================================");
    LOG_INFO("Running ECS Test: " << testName);
    LOG_INFO("Press T to cycle to the next test");
    LOG_INFO("========================================");
}

void ECSTestScene::_cycleTests() {
    int current = static_cast<int>(m_currentTest);
    current++;

    // Cycle through all test types
    if (current > static_cast<int>(TestType::ArchetypeChanges)) {
        current = static_cast<int>(TestType::BasicEntityCreation);
    }

    _clearTestEntities();
    m_currentTest = static_cast<TestType>(current);

    // Log the test name
    const char* testNames[] = {
        "BasicEntityCreation", "ComponentModification", "LayerSystem", "ActiveAndTags",
        "LocalTransformTest", "WorldTransformTest", "TransformInterpolation",
        "PhysicsBasic", "PhysicsGravity", "PhysicsCollision", "PhysicsMaterial", 
        "PhysicsAngular", "PhysicsComplex",
        "RenderShapes", "RenderSprites", "RenderLayers", "RenderStressTest",
        "LifetimeBasic", "LifetimeWithPhysics", "LifetimeSpawner",
        "PhysicsRenderCombo", "AllSystemsTest", "StressTestAll",
        "EntityPooling", "ComponentIteration", "ArchetypeChanges"
    };
    
    int index = current - 1001;
    if (index >= 0 && index < 26) {
        _logTestInfo(testNames[index]);
    }
}

void ECSTestScene::_createWorldBoundaries() {
    const float wallThickness = 20.0f;
    const Color wallColor{0.3f, 0.3f, 0.3f, 1.0f};

    // Bottom wall
    CreateOnLayer(m_boundaryLayer,
        Components::LocalTransform{ 
            Vector3D{m_worldWidth * 0.5f, wallThickness * 0.5f, 0}, 
            Quaternion{0,0,0,1}, 
            Vector3D{1,1,1} 
        },
        Components::ShapeBox2D{ 
            Vector2D{m_worldWidth * 0.5f, wallThickness * 0.5f}, 
            Vector2D{0,0}, 
            wallColor, 
            0.f, 
            true 
        },
        Components::Name{"Boundary_Bottom"}
    );

    // Top wall
    CreateOnLayer(m_boundaryLayer,
        Components::LocalTransform{ 
            Vector3D{m_worldWidth * 0.5f, m_worldHeight - wallThickness * 0.5f, 0}, 
            Quaternion{0,0,0,1}, 
            Vector3D{1,1,1} 
        },
        Components::ShapeBox2D{ 
            Vector2D{m_worldWidth * 0.5f, wallThickness * 0.5f}, 
            Vector2D{0,0}, 
            wallColor, 
            0.f, 
            true 
        },
        Components::Name{"Boundary_Top"}
    );

    // Left wall
    CreateOnLayer(m_boundaryLayer,
        Components::LocalTransform{ 
            Vector3D{wallThickness * 0.5f, m_worldHeight * 0.5f, 0}, 
            Quaternion{0,0,0,1}, 
            Vector3D{1,1,1} 
        },
        Components::ShapeBox2D{ 
            Vector2D{wallThickness * 0.5f, m_worldHeight * 0.5f}, 
            Vector2D{0,0}, 
            wallColor, 
            0.f, 
            true 
        },
        Components::Name{"Boundary_Left"}
    );

    // Right wall
    CreateOnLayer(m_boundaryLayer,
        Components::LocalTransform{ 
            Vector3D{m_worldWidth - wallThickness * 0.5f, m_worldHeight * 0.5f, 0}, 
            Quaternion{0,0,0,1}, 
            Vector3D{1,1,1} 
        },
        Components::ShapeBox2D{ 
            Vector2D{wallThickness * 0.5f, m_worldHeight * 0.5f}, 
            Vector2D{0,0}, 
            wallColor, 
            0.f, 
            true 
        },
        Components::Name{"Boundary_Right"}
    );

    LOG_DEBUG("Created world boundaries at edges of screen (" << m_worldWidth << "x" << m_worldHeight << ")");
}

// ================================================================================================
// CORE COMPONENT TESTS
// ================================================================================================

void ECSTestScene::_testBasicEntityCreation() {
    if (m_testEntities.empty()) {
        // Create 5 entities with different component configurations
        // Centered horizontally on screen
        const float spacing = 100.f;
        const float totalWidth = 3 * spacing;
        const float startX = (m_worldWidth - totalWidth) * 0.5f;
        const float yPos = m_worldHeight * 0.5f;
        
        const Entity e1 = CreateOnLayer(m_testLayer, 
            Components::LocalTransform{ Vector3D{startX, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::Name{"Entity_1"}
        );
        
        const Entity e2 = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ Vector3D{startX + spacing, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::Active{true},
            Components::Name{"Entity_2"}
        );
        
        const Entity e3 = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ Vector3D{startX + 2 * spacing, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::TagMask{0x01},
            Components::Name{"Entity_3"}
        );

        m_testEntities.push_back(EntityUtils::Pack(e1));
        m_testEntities.push_back(EntityUtils::Pack(e2));
        m_testEntities.push_back(EntityUtils::Pack(e3));

        // Create and immediately destroy an entity to test pooling
        const Entity temp = CreateEntity();
        DestroyEntity(temp);

        // Create another entity - should reuse the destroyed entity's slot
        const Entity e4 = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ Vector3D{startX + 3 * spacing, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::Name{"Entity_4_Reused"}
        );
        m_testEntities.push_back(EntityUtils::Pack(e4));

        // Draw boxes for visualization
        for (const uint64_t id : m_testEntities) {
            const Entity e = EntityUtils::Unpack(id);
            if (GetWorld().IsAlive(e)) {
                GetWorld().Add<Components::ShapeBox2D>(e, Components::ShapeBox2D{
                    Vector2D{25.f, 25.f}, Vector2D{0,0},
                    Color{0.f, 1.f, 0.f, 1.f}, 2.f, false
                });
            }
        }

        LOG_DEBUG("Created " << m_testEntities.size() << " entities with various components");
    }
}

void ECSTestScene::_testComponentModification() {
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        const Entity e = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ Vector3D{m_worldWidth * 0.5f, m_worldHeight * 0.5f, 0}, 
                                       Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeCircle2D{ 50.f, Vector2D{0,0}, Color{1.f,0.f,0.f,1.f}, 0.f, true },
            Components::Name{"ModifiableEntity"}
        );
        m_testEntities.push_back(EntityUtils::Pack(e));
    }

    // Modify components dynamically
    const Entity e = EntityUtils::Unpack(m_testEntities[0]);
    if (world.IsAlive(e) && world.Has<Components::ShapeCircle2D>(e)) {
        auto& circle = world.Get<Components::ShapeCircle2D>(e);
        
        // Pulse the circle radius and color
        const float pulse = 0.5f + 0.5f * std::sin(m_testTimer * 2.0f);
        circle.Radius = 30.f + 40.f * pulse;
        circle.Color.R = static_cast<HexValue>(pulse * 255.f);
        circle.Color.G = static_cast<HexValue>((1.f - pulse) * 255.f);
        circle.Color.B = 200u;
        circle.Color.A = static_cast<HexValue>(pulse * 255.f);
    }
}

void ECSTestScene::_testLayerSystem() {
    if (m_testEntities.empty()) {
        // Create entities on different layers, centered horizontally
        const float spacing = 150.f;
        const float totalWidth = 2 * spacing;
        const float startX = (m_worldWidth - totalWidth) * 0.5f;
        const float yPos = m_worldHeight * 0.5f;
        
        const Entity e1 = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ Vector3D{startX, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeBox2D{ Vector2D{40, 40}, Vector2D{0,0}, Color{1.f,0.f,0.f,1.f}, 0.f, true },
            Components::Name{"Layer_Test"}
        );

        const Entity e2 = CreateOnLayer(m_physicsLayer,
            Components::LocalTransform{ Vector3D{startX + spacing, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeBox2D{ Vector2D{40, 40}, Vector2D{0,0}, Color{0.f,1.f,0.f,1.f}, 0.f, true },
            Components::Name{"Layer_Physics"}
        );

        const Entity e3 = CreateOnLayer(m_renderLayer,
            Components::LocalTransform{ Vector3D{startX + 2 * spacing, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeBox2D{ Vector2D{40, 40}, Vector2D{0,0}, Color{0.f,0.f,1.f,1.f}, 0.f, true },
            Components::Name{"Layer_Render"}
        );

        m_testEntities.push_back(EntityUtils::Pack(e1));
        m_testEntities.push_back(EntityUtils::Pack(e2));
        m_testEntities.push_back(EntityUtils::Pack(e3));

        LOG_DEBUG("Created entities on 3 different layers");
    }

    // Count entities per layer
    static float logTimer = 0.f;
    logTimer += Time::DeltaTime();
    if (logTimer >= 2.0f) {
        int testCount = 0, physicsCount = 0, renderCount = 0;
        GetWorld().Each<Components::Layer>([&](Entity, const Components::Layer& layer) {
            if (layer.Id == m_testLayer) testCount++;
            else if (layer.Id == m_physicsLayer) physicsCount++;
            else if (layer.Id == m_renderLayer) renderCount++;
        });
        LOG_DEBUG("Layers -> Test: " << testCount << ", Physics: " << physicsCount << ", Render: " << renderCount);
        logTimer = 0.f;
    }
}

void ECSTestScene::_testActiveAndTags() {
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        // Create entities with Active and TagMask components, centered horizontally
        const float spacing = 100.f;
        const float totalWidth = 4 * spacing;
        const float startX = (m_worldWidth - totalWidth) * 0.5f;
        const float yPos = m_worldHeight * 0.5f;
        
        for (int i = 0; i < 5; ++i) {
            const Entity e = CreateOnLayer(m_testLayer,
                Components::LocalTransform{ 
                    Vector3D{startX + i * spacing, yPos, 0}, 
                    Quaternion{0,0,0,1}, 
                    Vector3D{1,1,1} 
                },
                Components::Active{ i % 2 == 0 }, // Alternate enabled/disabled
                Components::TagMask{ static_cast<uint32_t>(1 << i) }, // Each gets unique tag
                Components::ShapeCircle2D{ 30.f, Vector2D{0,0}, 
                    Color{i % 2 == 0 ? 1.f : 0.5f, 1.f, 0.f, 1.f}, 0.f, true },
                Components::Name{"Tagged_Entity"}
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
        
        LOG_DEBUG("Created 5 entities with Active and TagMask components");
        LOG_DEBUG("TagMask is a 32-bit bitmask for filtering entities by tags (bits 0-31)");
        LOG_DEBUG("Each entity has a unique tag bit: Entity 0=bit 0, Entity 1=bit 1, etc.");
        LOG_DEBUG("But as of now, it is not fully implemented so there is no use for TagMask.");
    }

    // Toggle active states periodically
	// Conversion from (float to int to float) to get whole number for seconds
    if (static_cast<int>(m_testTimer) % 2 == 0 && m_testTimer - static_cast<float>(static_cast<int>(m_testTimer)) < Time::DeltaTime()) {
        int toggleCount = 0;
        for (const uint64_t id : m_testEntities) {
            const Entity e = EntityUtils::Unpack(id);
            if (world.IsAlive(e) && world.Has<Components::Active>(e)) {
                auto& active = world.Get<Components::Active>(e);
                active.Enabled = !active.Enabled;
                toggleCount++;
            }
        }
        if (toggleCount > 0) {
            LOG_DEBUG("Toggled Active.Enabled for " << toggleCount << " entities");
        }
    }
}

// ================================================================================================
// TRANSFORM TESTS
// ================================================================================================

void ECSTestScene::_testLocalTransform() {
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        // Center entity that rotates and scales
        const Entity e = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ 
                Vector3D{m_worldWidth * 0.5f, m_worldHeight * 0.5f, 0}, 
                Quaternion{0,0,0,1}, 
                Vector3D{1,1,1} 
            },
            Components::ShapeBox2D{ Vector2D{50, 50}, Vector2D{0,0}, Color{1.f,1.f,0.f,1.f}, 2.f, false },
            Components::Name{"LocalTransform_Center"}
        );
        m_testEntities.push_back(EntityUtils::Pack(e));
        
        // Entity moving along X axis from left
        const Entity eLeft = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ 
                Vector3D{m_worldWidth * 0.15f, m_worldHeight * 0.25f, 0}, 
                Quaternion{0,0,0,1}, 
                Vector3D{1,1,1} 
            },
            Components::ShapeBox2D{ Vector2D{50, 50}, Vector2D{0,0}, Color{0.f,1.f,1.f,1.f}, 2.f, false },
            Components::Name{"LocalTransform_Left"}
        );
        m_testEntities.push_back(EntityUtils::Pack(eLeft));
        
        // Entity moving along X axis from right
        const Entity eRight = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ 
                Vector3D{m_worldWidth * 0.85f, m_worldHeight * 0.75f, 0}, 
                Quaternion{0,0,0,1}, 
                Vector3D{1,1,1} 
            },
            Components::ShapeBox2D{ Vector2D{50, 50}, Vector2D{0,0}, Color{1.f,0.f,1.f,1.f}, 2.f, false },
            Components::Name{"LocalTransform_Right"}
        );
        m_testEntities.push_back(EntityUtils::Pack(eRight));
    }

    // Animate center transform - rotate and scale
    for (int i = 0; i < m_testEntities.size(); ++i) {
        const Entity e = EntityUtils::Unpack(m_testEntities[i]);
        if (!world.IsAlive(e)) continue;
        auto& tr = world.Get<Components::LocalTransform>(e);

        // Rotate
        const auto deltaRotation = Quaternion::FromAxisAngle(Vector3D::Forward, 90.f * Time::DeltaTime());
        tr.Rotation = deltaRotation * tr.Rotation;
        tr.Rotation.Normalize();

        // Scale pulse
        const float scale = 0.8f + 0.4f * std::sin(m_testTimer * 3.0f);
        tr.Scale = Vector3D{ scale, scale, 1.f };

        if (i == 1) {
            // Move left entity from left to right
            const float t = (std::sin(m_testTimer * 1.5f) + 1.0f) * 0.5f; // 0 to 1
            tr.Position.X = m_worldWidth * 0.15f + t * (m_worldWidth * 0.75f);
        }
        else if (i == 2) {
            // Move right entity from right to left
            const float t = (std::sin(m_testTimer * 1.5f) + 1.0f) * 0.5f; // 0 to 1
            tr.Position.X = m_worldWidth * 0.85f - t * (m_worldWidth * 0.75f);
        }
    }
}

void ECSTestScene::_testWorldTransform() {
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        // Create parent at center
        const Entity parent = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ 
                Vector3D{m_worldWidth * 0.5f, m_worldHeight * 0.5f, 0}, 
                Quaternion{0,0,0,1}, 
                Vector3D{1,1,1} 
            },
            Components::WorldTransform{},
            Components::ShapeBox2D{ Vector2D{60, 60}, Vector2D{0,0}, Color{1.f,0.f,0.f,1.f}, 2.f, false },
            Components::Name{"Parent"}
        );

        // Create children
        const Entity child1 = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ 
                Vector3D{100, 0, 0}, // Offset from parent
                Quaternion{0,0,0,1}, 
                Vector3D{0.5f, 0.5f, 1.f} 
            },
            Components::WorldTransform{},
            Components::ShapeCircle2D{ 20.f, Vector2D{0,0}, Color{0.f,1.f,0.f,1.f}, 0.f, true },
            Components::Name{"Child_1"}
        );

        const Entity child2 = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ 
                Vector3D{-100, 0, 0}, // Offset from parent
                Quaternion{0,0,0,1}, 
                Vector3D{0.5f, 0.5f, 1.f} 
            },
            Components::WorldTransform{},
            Components::ShapeCircle2D{ 20.f, Vector2D{0,0}, Color{0.f,0.f,1.f,1.f}, 0.f, true },
            Components::Name{"Child_2"}
        );

        const Entity child3 = CreateOnLayer(m_testLayer,
            Components::LocalTransform{
                Vector3D{-350, 0, 0}, // Offset from parent
                Quaternion{0,0,0,1},
                Vector3D{0.25f, 0.25f, 1.f}
            },
            Components::WorldTransform{},
            Components::ShapeCircle2D{ 20.f, Vector2D{0,0}, Color{1.f,1.f,1.f,1.f}, 0.f, true },
            Components::Name{ "Child_3" }
        );

        // Establish hierarchy
        world.Attach(child1, parent);
        world.Attach(child2, parent);
        world.Attach(child3, parent);

        m_testEntities.push_back(EntityUtils::Pack(parent));
        m_testEntities.push_back(EntityUtils::Pack(child1));
        m_testEntities.push_back(EntityUtils::Pack(child2));
        m_testEntities.push_back(EntityUtils::Pack(child3));

        LOG_DEBUG("Created parent-child hierarchy");
    }

    // Rotate parent - children should follow
    const Entity parent = EntityUtils::Unpack(m_testEntities[0]);
    if (world.IsAlive(parent)) {
        auto& tr = world.Get<Components::LocalTransform>(parent);
        const auto deltaRotation = Quaternion::FromAxisAngle(Vector3D::Forward, 45.f * Time::DeltaTime());
        tr.Rotation = deltaRotation * tr.Rotation;
        tr.Rotation.Normalize();
    }
}

void ECSTestScene::_testTransformInterpolation() {
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        const Entity e = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ 
                Vector3D{100, m_worldHeight * 0.5f, 0}, 
                Quaternion{0,0,0,1}, 
                Vector3D{1,1,1} 
            },
            Components::ShapeBox2D{ Vector2D{30, 30}, Vector2D{0,0}, Color{1.f,0.f,1.f,1.f}, 0.f, true },
            Components::Name{"Interpolated"}
        );
        m_testEntities.push_back(EntityUtils::Pack(e));
    }

    // Smoothly move entity back and forth
    const Entity e = EntityUtils::Unpack(m_testEntities[0]);
    if (world.IsAlive(e)) {
        auto& tr = world.Get<Components::LocalTransform>(e);
        const float t = (std::sin(m_testTimer * 2.0f) + 1.0f) * 0.5f; // 0 to 1
        tr.Position.X = 100.f + t * (m_worldWidth - 200.f);
    }
}

// ================================================================================================
// PHYSICS SYSTEM TESTS
// ================================================================================================

void ECSTestScene::_testPhysicsBasic() {
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        const Entity e = CreateOnLayer(m_physicsLayer,
            Components::LocalTransform{ Vector3D{100, m_worldHeight * 0.5f, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::Rigidbody2D{ 1.f, 1.f, 0.1f, 0.0f, 1.0f, 0 },
            Components::LinearVelocity2D{ Vector2D{1000.f, 0.f} },
            Components::AngularVelocity2D{ 0.f },
            Components::PhysicsMaterial2D{},
            Components::CircleCollider2D{ 25.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
            Components::ShapeCircle2D{ 25.f, Vector2D{0,0}, Color{1.f,1.f,0.f,1.f}, 0.f, true },
            Components::Name{"Physics_Basic"}
        );
        m_testEntities.push_back(EntityUtils::Pack(e));
    }

    // Bounce off walls
    const Entity e = EntityUtils::Unpack(m_testEntities[0]);
    if (world.IsAlive(e)) {
        const auto& tr = world.Get<Components::LocalTransform>(e);
        auto& vel = world.Get<Components::LinearVelocity2D>(e);
        
        if (tr.Position.X > m_worldWidth - 50.f || tr.Position.X < 50.f) {
            vel.Value.X *= -1.f;
        }
    }
}

void ECSTestScene::_testPhysicsGravity() {
    if (m_testEntities.empty()) {
        // Create falling objects
        for (int i = 0; i < 3; ++i) {
            const Entity e = CreateOnLayer(m_physicsLayer,
                Components::LocalTransform{ 
                    Vector3D{200.f + static_cast<float>(i) * 150.f, m_worldHeight - 100.f, 0},
                    Quaternion{0,0,0,1}, 
                    Vector3D{1,1,1} 
                },
                Components::Rigidbody2D{ 1.f, 1.f, 0.0f, 0.0f, 1.0f + i * 0.5f, 0x02 }, // UseGravity flag
                Components::LinearVelocity2D{ Vector2D{500.f, 0.f} },
                Components::AngularVelocity2D{ 0.f },
                Components::CircleCollider2D{ 20.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
                Components::ShapeCircle2D{ 20.f, Vector2D{0,0}, 
                    Color{1.f, 0.5f * static_cast<float>(i), 0.f, 1.f}, 0.f, true },
                Components::Name{"Falling_Object"}
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
    }
}

void ECSTestScene::_testPhysicsCollision() {
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        // Create two moving circles that will collide
        const Entity e1 = CreateOnLayer(m_physicsLayer,
            Components::LocalTransform{ Vector3D{200, m_worldHeight * 0.5f, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::Rigidbody2D{ 1.f, 1.f, 0.0f, 0.0f, 0.0f, 0 },
            Components::LinearVelocity2D{ Vector2D{150.f, 0.f} },
            Components::AngularVelocity2D{ 0.f },
            Components::CircleCollider2D{ 30.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
            Components::PhysicsMaterial2D{ 0.2f, 0.8f, 0.2f },
            Components::ShapeCircle2D{ 30.f, Vector2D{0,0}, Color{1.f,0.f,0.f,1.f}, 0.f, true },
            Components::Name{"Collider_1"}
        );

        const Entity e2 = CreateOnLayer(m_physicsLayer,
            Components::LocalTransform{ Vector3D{m_worldWidth - 200, m_worldHeight * 0.5f, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::Rigidbody2D{ 1.f, 1.f, 0.0f, 0.0f, 0.0f, 0 },
            Components::LinearVelocity2D{ Vector2D{-150.f, 0.f} },
            Components::AngularVelocity2D{ 0.f },
            Components::CircleCollider2D{ 30.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
            Components::PhysicsMaterial2D{ 0.2f, 0.8f, 0.2f },
            Components::ShapeCircle2D{ 30.f, Vector2D{0,0}, Color{0.f,0.f,1.f,1.f}, 0.f, true },
            Components::Name{"Collider_2"}
        );

        m_testEntities.push_back(EntityUtils::Pack(e1));
        m_testEntities.push_back(EntityUtils::Pack(e2));
    }
}

void ECSTestScene::_testPhysicsMaterial() {
    if (m_testEntities.empty()) {
        // Create bouncy balls with different restitution, centered horizontally
        const float spacing = 120.f;
        const float totalWidth = 3 * spacing;
        const float startX = (m_worldWidth - totalWidth) * 0.5f;

        for (int i = 0; i < 4; ++i) {
            const float restitution = static_cast<float>(i) * 0.25f; // 0, 0.25, 0.5, 0.75
            const Entity e = CreateOnLayer(m_physicsLayer,
                Components::LocalTransform{
                    Vector3D{startX + static_cast<float>(i) * spacing, m_worldHeight - 150.f, 0},
                    Quaternion{0,0,0,1},
                    Vector3D{1,1,1}
                },
                Components::Rigidbody2D{ 1.f, 1.f, 0.0f, 0.0f, 1.0f, 0x02 },
                Components::LinearVelocity2D{ Vector2D{0.f, 0.f} },
                Components::AngularVelocity2D{ 0.f },
                Components::CircleCollider2D{ 20.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
                Components::PhysicsMaterial2D{ 1.f - restitution, restitution, 0.2f },
                Components::ShapeCircle2D{ 20.f, Vector2D{0,0},
                    Color{restitution, 1.f - restitution, 0.5f, 1.f}, 0.f, true },
                Components::Name{ "Bouncy_Ball" }
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
    }
}

void ECSTestScene::_testPhysicsAngular() {
    if (m_testEntities.empty()) {
        // Create spinning boxes, centered horizontally
        const float spacing = 200.f;
        const float totalWidth = 2 * spacing;
        const float startX = (m_worldWidth - totalWidth) * 0.5f;

        for (int i = 0; i < 3; ++i) {
            const Entity e = CreateOnLayer(m_physicsLayer,
                Components::LocalTransform{
                    Vector3D{startX + static_cast<float>(i) * spacing, m_worldHeight * 0.5f, 0},
                    Quaternion{0,0,0,1},
                    Vector3D{1,1,1}
                },
                Components::Rigidbody2D{ 1.f, 1.f, 0.0f, 0.1f, 0.0f, 0 },
                Components::LinearVelocity2D{ Vector2D{0.f, 0.f} },
                Components::AngularVelocity2D{ 90.f * static_cast<float>(i + 1) }, // Different spin speeds
                Components::BoxCollider2D{ Vector2D{40,40}, Vector2D{0,0}, 0.f, 0xFFFFFFFF, 0 },
                Components::ShapeBox2D{ Vector2D{40,40}, Vector2D{0,0},
                    Color{0.f, 1.f, static_cast<float>(i) * 0.3f, 1.f}, 2.f, false },
                Components::Name{ "Spinning_Box" }
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
    }
}

void ECSTestScene::_testPhysicsComplex() {
    if (m_testEntities.empty()) {
        // Create a complex scene with multiple interacting bodies
        for (int i = 0; i < 10; ++i) {
            const Entity e = CreateOnLayer(m_physicsLayer,
                Components::LocalTransform{
                    Vector3D{
                        MathUtils::Randomize(100.f, m_worldWidth - 100.f),
                        MathUtils::Randomize(100.f, m_worldHeight - 100.f),
                        0
                    },
                    Quaternion{0,0,0,1}, 
                    Vector3D{1,1,1} 
                },
                Components::Rigidbody2D{ 1.f, 1.f, 0.05f, 0.05f, 0.5f, 0x02 },
                Components::LinearVelocity2D{ 
                    Vector2D{
                        MathUtils::Randomize(-100.f, 100.f),
                        MathUtils::Randomize(-100.f, 100.f)
                    }
                },
                Components::AngularVelocity2D{ MathUtils::Randomize(-180.f, 180.f) },
                Components::CircleCollider2D{ 15.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
                Components::PhysicsMaterial2D{ 0.3f, 0.5f, 0.2f },
                Components::ShapeCircle2D{ 15.f, Vector2D{0,0}, 
                    Color{MathUtils::Randomize(0.f,1.f), MathUtils::Randomize(0.f,1.f), MathUtils::Randomize(0.f,1.f), 1.f}, 
                    0.f, true },
                Components::Name{"Complex_Body"}
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
    }
}

// ================================================================================================
// RENDERER SYSTEM TESTS
// ================================================================================================

void ECSTestScene::_testRenderShapes() {
    if (m_testEntities.empty()) {
        // Create shapes centered horizontally
        const float spacing = 150.f;
        const float totalWidth = 3 * spacing;
        const float startX = (m_worldWidth - totalWidth) * 0.5f;
        const float yPos = m_worldHeight * 0.5f;

        // Circle
        const Entity circle = CreateOnLayer(m_renderLayer,
            Components::LocalTransform{ Vector3D{startX, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeCircle2D{ 40.f, Vector2D{0,0}, Color{1.f,0.f,0.f,1.f}, 0.f, true },
            Components::Name{ "Circle" }
        );

        // Box (filled)
        const Entity box1 = CreateOnLayer(m_renderLayer,
            Components::LocalTransform{ Vector3D{startX + spacing, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeBox2D{ Vector2D{50,50}, Vector2D{0,0}, Color{0.f,1.f,0.f,1.f}, 0.f, true },
            Components::Name{ "Box_Filled" }
        );

        // Box (outline)
        const Entity box2 = CreateOnLayer(m_renderLayer,
            Components::LocalTransform{ Vector3D{startX + 2 * spacing, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeBox2D{ Vector2D{50,50}, Vector2D{0,0}, Color{0.f,0.f,1.f,1.f}, 3.f, false },
            Components::Name{ "Box_Outline" }
        );

        // Line
        const Entity line = CreateOnLayer(m_renderLayer,
            Components::LocalTransform{ Vector3D{startX + 3 * spacing, yPos, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeLine2D{ Vector2D{-50,-50}, Vector2D{50,50}, Color{1.f,1.f,0.f,1.f}, 2.f },
            Components::Name{ "Line" }
        );

        m_testEntities.push_back(EntityUtils::Pack(circle));
        m_testEntities.push_back(EntityUtils::Pack(box1));
        m_testEntities.push_back(EntityUtils::Pack(box2));
        m_testEntities.push_back(EntityUtils::Pack(line));
    }
}

void ECSTestScene::_testRenderSprites() {
    if (m_testEntities.empty()) {
        const std::string playerPath = "assets/textures/test/player.png";
        const std::string fishBoyPath = "assets/textures/test/fishBoy.png";

        auto playerTex = RM.Get<Texture>(playerPath);
        auto fishBoyTex = RM.Get<Texture>(fishBoyPath);

        if (playerTex && fishBoyTex) {
            const Entity sprite1 = CreateOnLayer(m_renderLayer,
                Components::LocalTransform{ 
                    Vector3D{m_worldWidth * 0.3f, m_worldHeight * 0.5f, 0}, 
                    Quaternion{0,0,0,1}, 
                    Vector3D{256, 256, 1} 
                },
                Components::SpriteRenderer2D{ 
                    playerTex->ID(), 
                    Color{1.f,1.f,1.f,1.f}, 
                    Vector2D{1,1}, 
                    Vector2D{0,0} 
                },
                Components::Name{"Sprite_Player"}
            );

            const Entity sprite2 = CreateOnLayer(m_renderLayer,
                Components::LocalTransform{ 
                    Vector3D{m_worldWidth * 0.7f, m_worldHeight * 0.5f, 0}, 
                    Quaternion{0,0,0,1}, 
                    Vector3D{256, 256, 1} 
                },
                Components::SpriteRenderer2D{ 
                    fishBoyTex->ID(), 
                    Color{1.f,1.f,1.f,1.f}, 
                    Vector2D{1,1}, 
                    Vector2D{0,0} 
                },
                Components::Name{"Sprite_FishBoy"}
            );

            m_testEntities.push_back(EntityUtils::Pack(sprite1));
            m_testEntities.push_back(EntityUtils::Pack(sprite2));
        }
    }
}

void ECSTestScene::_testRenderLayers() {
    if (m_testEntities.empty()) {
        // Create overlapping shapes on different layers to test z-ordering
        const uint16_t layer1 = GetLayers().CreateOrGetLayer("back");
        const uint16_t layer2 = GetLayers().CreateOrGetLayer("middle");
        const uint16_t layer3 = GetLayers().CreateOrGetLayer("front");

        const Entity front = CreateOnLayer(layer3,
            Components::LocalTransform{ Vector3D{m_worldWidth * 0.5f + 100, m_worldHeight * 0.5f + 100, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeBox2D{ Vector2D{150,150}, Vector2D{0,0}, Color{0,0,1,0.7f}, 0.f, true },
            Components::Name{ "Front_Layer" }
        );

        const Entity middle = CreateOnLayer(layer2,
            Components::LocalTransform{ Vector3D{m_worldWidth * 0.5f + 50, m_worldHeight * 0.5f + 50, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeBox2D{ Vector2D{150,150}, Vector2D{0,0}, Color{0,1,0,0.7f}, 0.f, true },
            Components::Name{ "Middle_Layer" }
        );

        const Entity back = CreateOnLayer(layer1,
            Components::LocalTransform{ Vector3D{m_worldWidth * 0.5f, m_worldHeight * 0.5f, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
            Components::ShapeBox2D{ Vector2D{150,150}, Vector2D{0,0}, Color{1,0,0,0.7f}, 0.f, true },
            Components::Name{"Back_Layer"}
        );

        m_testEntities.push_back(EntityUtils::Pack(back));
        m_testEntities.push_back(EntityUtils::Pack(middle));
        m_testEntities.push_back(EntityUtils::Pack(front));
    }
}

void ECSTestScene::_testRenderStressTest() {
    if (m_testEntities.empty()) {
        // Create many entities to stress test the renderer
        const int count = 10;
        for (int i = 0; i < count; ++i) {
            const Entity e = CreateOnLayer(m_renderLayer,
                Components::LocalTransform{ 
                    Vector3D{
                        MathUtils::Randomize(50.f, m_worldWidth - 50.f),
                        MathUtils::Randomize(50.f, m_worldHeight - 50.f),
                        0
                    },
                    Quaternion{0,0,0,1}, 
                    Vector3D{1,1,1} 
                },
                Components::ShapeCircle2D{ 
                    MathUtils::Randomize(5.f, 20.f), 
                    Vector2D{0,0}, 
                    Color{
                        MathUtils::Randomize(0.f, 1.f),
                        MathUtils::Randomize(0.f, 1.f),
                        MathUtils::Randomize(0.f, 1.f),
                        1.f
                    }, 
                    0.f, true 
                },
                Components::Name{"Stress_Circle"}
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
        LOG_DEBUG("Spawned " << count << " entities for stress test");
    }
}

// ================================================================================================
// LIFETIME SYSTEM TESTS
// ================================================================================================

void ECSTestScene::_testLifetimeBasic() {
    // Check if the entities are still alive
    for (auto it = m_testEntities.begin(); it != m_testEntities.end(); ) {
        const Entity e = EntityUtils::Unpack(*it);
        if (!GetWorld().IsAlive(e)) {
            it = m_testEntities.erase(it); // Remove from list if dead
            m_spawnCount--;
        }
        else
            ++it;
    }

    // Continuously spawn entities with short lifetimes
    if (m_spawnCount < 20 && static_cast<int>(m_testTimer * 2) > m_spawnCount) {
        const Entity e = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ 
                Vector3D{
                    MathUtils::Randomize(100.f, m_worldWidth - 100.f),
                    MathUtils::Randomize(100.f, m_worldHeight - 100.f),
                    0
                },
                Quaternion{0,0,0,1}, 
                Vector3D{1,1,1} 
            },
            Components::Lifetime{ 2.0f }, // Live for 2 seconds
            Components::ShapeCircle2D{
            	20.f,
            	Vector2D{0,0},
            	Color{
            		MathUtils::Randomize(0.f, 1.f),
            		MathUtils::Randomize(0.f, 1.f),
            		MathUtils::Randomize(0.f, 1.f),
            		MathUtils::Randomize(0.25f, 1.f)
            	},
            	0.f,
            	true },
            Components::Name{"Timed_Entity"}
        );
        m_testEntities.push_back(EntityUtils::Pack(e));
        m_spawnCount++;
    }
}

void ECSTestScene::_testLifetimeWithPhysics() {
    // Check if the entities are still alive
    for (auto it = m_testEntities.begin(); it != m_testEntities.end(); ) {
        const Entity e = EntityUtils::Unpack(*it);
        if (!GetWorld().IsAlive(e)) {
            it = m_testEntities.erase(it); // Remove from list if dead
            m_spawnCount--;
        }
    	else
            ++it;
	}

    // Spawn falling entities that expire
    if (m_spawnCount < 15 && static_cast<int>(m_testTimer * 3) > m_spawnCount) {
        const Entity e = CreateOnLayer(m_physicsLayer,
            Components::LocalTransform{ 
                Vector3D{
                    MathUtils::Randomize(100.f, m_worldWidth - 100.f),
                    m_worldHeight - 50.f,
                    0
                },
                Quaternion{0,0,0,1}, 
                Vector3D{1,1,1} 
            },
            Components::Rigidbody2D{ 1.f, 1.f, 0.0f, 0.0f, 1.0f, 0x02 },
            Components::LinearVelocity2D{ Vector2D{0.f, 0.f} },
            Components::AngularVelocity2D{ MathUtils::Randomize(-180.f, 180.f) },
            Components::CircleCollider2D{ 15.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
            Components::Lifetime{ 3.0f },
            Components::Active{ true },
            Components::ShapeCircle2D{ 15.f, Vector2D{0,0}, 
                Color{MathUtils::Randomize(0.5f,1.f), MathUtils::Randomize(0.5f,1.f), 0.f, 1.f}, 
                0.f, true },
            Components::Name{"Falling_Timed"}
        );
        m_testEntities.push_back(EntityUtils::Pack(e));
        m_spawnCount++;
    }
}

// TODO: Physics probably need to be fixed here
void ECSTestScene::_testLifetimeSpawner() {
    // Continuous spawner - spawn frequently
    if (m_testTimer - static_cast<float>(static_cast<int>(m_testTimer)) < Time::DeltaTime() * 2) {
        const Entity e = CreateOnLayer(m_testLayer,
            Components::LocalTransform{ 
                Vector3D{m_worldWidth * 0.5f, m_worldHeight * 0.5f, 0},
                Quaternion{0,0,0,1}, 
                Vector3D{1,1,1} 
            },
            Components::LinearVelocity2D{ 
                Vector2D{
                    MathUtils::Randomize(-200.f, 200.f),
                    MathUtils::Randomize(-200.f, 200.f)
                }
            },
            Components::Lifetime{ 1.5f },
            Components::Active{ true },
            Components::ShapeCircle2D{ 10.f, Vector2D{0,0}, 
                Color{MathUtils::Randomize(0.f,1.f), MathUtils::Randomize(0.f,1.f), MathUtils::Randomize(0.f,1.f), 0.8f}, 
                0.f, true },
            Components::Name{"Spawned_Particle"}
        );
        m_testEntities.push_back(EntityUtils::Pack(e));
        m_spawnCount++;

        // Move particles manually (since they don't have physics)
        ECS::World& world = GetWorld();
        world.Each<Components::LocalTransform, Components::LinearVelocity2D>([](Entity, Components::LocalTransform& tr, const Components::LinearVelocity2D& vel) {
            tr.Position.X += vel.Value.X * Time::DeltaTime();
            tr.Position.Y += vel.Value.Y * Time::DeltaTime();
        });
    }
}

// ================================================================================================
// INTEGRATION TESTS
// ================================================================================================

void ECSTestScene::_testPhysicsRenderCombo() {
    if (m_testEntities.empty()) {
        // Create entities with both physics and rendering, centered horizontally
        const float spacing = 120.f;
        const float totalWidth = 4 * spacing;
        const float startX = (m_worldWidth - totalWidth) * 0.5f;

        for (int i = 0; i < 5; ++i) {
            const Entity e = CreateOnLayer(m_physicsLayer,
                Components::LocalTransform{
                    Vector3D{startX + static_cast<float>(i) * spacing, 150.f, 0},
                    Quaternion{0,0,0,1},
                    Vector3D{1,1,1}
                },
                Components::Rigidbody2D{ 1.f, 1.f, 0.05f, 0.05f, 1.0f, 0x02 },
                Components::LinearVelocity2D{ Vector2D{0.f, 0.f} },
                Components::AngularVelocity2D{ 0.f },
                Components::CircleCollider2D{ 25.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
                Components::PhysicsMaterial2D{ 0.3f, 0.7f, 0.2f },
                Components::ShapeCircle2D{ 25.f, Vector2D{0,0},
                    Color{static_cast<float>(i) * 0.2f, 1.f - static_cast<float>(i) * 0.2f, 0.5f, 1.f}, 0.f, true },
                Components::Name{ "PhysicsRender_Combo" }
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
    }
}

void ECSTestScene::_testAllSystems() {
    if (m_testEntities.empty()) {
        // Create entities that use all systems
        for (int i = 0; i < 8; ++i) {
            const Entity e = CreateOnLayer(m_testLayer,
                Components::LocalTransform{ 
                    Vector3D{
                        MathUtils::Randomize(100.f, m_worldWidth - 100.f),
                        m_worldHeight - 100.f,
                        0
                    },
                    Quaternion{0,0,0,1}, 
                    Vector3D{1,1,1} 
                },
                Components::Rigidbody2D{ 0.5f, 2.f, 0.1f, 0.1f, 0.8f, 0x02 },
                Components::LinearVelocity2D{ 
                    Vector2D{MathUtils::Randomize(-50.f, 50.f), 0.f}
                },
                Components::AngularVelocity2D{ MathUtils::Randomize(-90.f, 90.f) },
                Components::CircleCollider2D{ 20.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
                Components::PhysicsMaterial2D{ 0.2f, 0.6f, 0.2f },
                Components::Lifetime{ 5.0f + i * 0.5f },
                Components::Active{ true },
                Components::ShapeCircle2D{ 20.f, Vector2D{0,0}, 
                    Color{MathUtils::Randomize(0.3f,1.f), MathUtils::Randomize(0.3f,1.f), MathUtils::Randomize(0.3f,1.f), 1.f}, 
                    0.f, true },
                Components::TagMask{ static_cast<uint32_t>(1 << i) },
                Components::Name{"AllSystems_Entity"}
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
        LOG_DEBUG("Created entities using Physics + Renderer + Lifetime systems");
    }
}

void ECSTestScene::_testStressTestAll() {
    if (m_testEntities.empty()) {
        const int count = 1000;
        for (int i = 0; i < count; ++i) {
            const Entity e = CreateOnLayer(m_testLayer,
                Components::LocalTransform{ 
                    Vector3D{
                        MathUtils::Randomize(0.f, m_worldWidth),
                        MathUtils::Randomize(0.f, m_worldHeight),
                        0
                    },
                    Quaternion{0,0,0,1}, 
                    Vector3D{1,1,1} 
                },
                Components::Rigidbody2D{ 1.5f, 2.f, 0.1f, 0.1f, 0.5f, 0 },
                Components::LinearVelocity2D{ 
                    Vector2D{
                        MathUtils::Randomize(-250.f, 250.f),
                        MathUtils::Randomize(-250.f, 250.f)
                    }
                },
                Components::AngularVelocity2D{ MathUtils::Randomize(-180.f, 180.f) },
                Components::CircleCollider2D{ 10.f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
                Components::ShapeCircle2D{ 10.f, Vector2D{0,0}, 
                    Color{MathUtils::Randomize(0.f,1.f), MathUtils::Randomize(0.f,1.f), MathUtils::Randomize(0.f,1.f), 0.8f}, 
                    0.f, true },
                Components::Active{ true },
                Components::Name{"Stress_Entity"}
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
        LOG_DEBUG("Spawned " << count << " entities for full stress test");
    }
}

// ================================================================================================
// ADVANCED TESTS
// ================================================================================================

void ECSTestScene::_testEntityPooling() {
    // Test entity reuse and generation incrementing
    static int phase = 0;

    const float spacing = 80.f;
    const float totalWidth = 9 * spacing;
    const float startX = (m_worldWidth - totalWidth) * 0.5f;

    if (phase == 0 && m_testTimer > 1.0f) {
        // Phase 1: Create entities, centered
        for (int i = 0; i < 10; ++i) {
            const Entity e = CreateOnLayer(m_testLayer,
                Components::LocalTransform{ Vector3D{startX + static_cast<float>(i) * spacing, 200.f, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
                Components::ShapeCircle2D{ 20.f, Vector2D{0,0}, Color{1.f,1.f,0.f,1.f}, 0.f, true },
                Components::Name{ "Pooled_Entity" }
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
        LOG_DEBUG("Phase 1: Created 10 entities");
        phase = 1;
    }
    else if (phase == 1 && m_testTimer > 2.0f) {
        // Phase 2: Destroy all entities
        _clearTestEntities();
        LOG_DEBUG("Phase 2: Destroyed all entities");
        phase = 2;
    }
    else if (phase == 2 && m_testTimer > 3.0f) {
        // Phase 3: Create new entities (should reuse slots), centered
        for (int i = 0; i < 10; ++i) {
            const Entity e = CreateOnLayer(m_testLayer,
                Components::LocalTransform{ Vector3D{startX + static_cast<float>(i) * spacing, 400.f, 0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
                Components::ShapeCircle2D{ 20.f, Vector2D{0,0}, Color{0.f,1.f,1.f,1.f}, 0.f, true },
                Components::Name{ "Reused_Entity" }
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
        LOG_DEBUG("Phase 3: Created 10 new entities (reused slots)");
        phase = 3;
    }
}

// TODO: Need fixing
void ECSTestScene::_testComponentIteration() {
    if (m_testEntities.empty()) {
        // Create entities with various component combinations
        const int count = 100;
        for (int i = 0; i < count; ++i) {
            if (i % 3 == 0) {
                // Type A: Transform + Circle
                const Entity e = CreateOnLayer(m_testLayer,
                    Components::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
                    Components::ShapeCircle2D{ 10.f, Vector2D{0,0}, Color{1.f,0.f,0.f,1.f}, 0.f, true }
                );
                m_testEntities.push_back(EntityUtils::Pack(e));
            }
            else if (i % 3 == 1) {
                // Type B: Transform + Box + Active
                const Entity e = CreateOnLayer(m_testLayer,
                    Components::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
                    Components::ShapeBox2D{ Vector2D{10,10}, Vector2D{0,0}, Color{0.f,1.f,0.f,1.f}, 0.f, true },
                    Components::Active{ true }
                );
                m_testEntities.push_back(EntityUtils::Pack(e));
            }
            else {
                // Type C: Transform + Circle + Active + TagMask
                const Entity e = CreateOnLayer(m_testLayer,
                    Components::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
                    Components::ShapeCircle2D{ 10.f, Vector2D{0,0}, Color{0.f,0.f,1.f,1.f}, 0.f, true },
                    Components::Active{ true },
                    Components::TagMask{ static_cast<uint32_t>(i) }
                );
                m_testEntities.push_back(EntityUtils::Pack(e));
            }
        }
        LOG_DEBUG("Created " << count << " entities with different archetypes");
    }

    // Measure iteration performance
    static float perfTimer = 0.f;
    perfTimer += Time::DeltaTime();
    if (perfTimer >= 1.0f) {
        int count = 0;
        GetWorld().Each<Components::LocalTransform>([&count](Entity, const Components::LocalTransform&) {
            count++;
        });
        LOG_DEBUG("Iterated over " << count << " entities with LocalTransform");
        perfTimer = 0.f;
    }
}

void ECSTestScene::_testArchetypeChanges() {
    ECS::World& world = GetWorld();

    if (m_testEntities.empty()) {
        // Create base entities, centered horizontally
        const float spacing = 150.f;
        const float totalWidth = 4 * spacing;
        const float startX = (m_worldWidth - totalWidth) * 0.5f;

        for (int i = 0; i < 5; ++i) {
            const Entity e = CreateOnLayer(m_testLayer,
                Components::LocalTransform{
                    Vector3D{startX + static_cast<float>(i) * spacing, m_worldHeight * 0.5f, 0},
                    Quaternion{0,0,0,1},
                    Vector3D{1,1,1}
                },
                Components::ShapeCircle2D{ 30.f, Vector2D{0,0}, Color{1.f,1.f,1.f,1.f}, 0.f, true },
                Components::Name{ "Archetype_Test" }
            );
            m_testEntities.push_back(EntityUtils::Pack(e));
        }
    }

    // Dynamically add/remove components over time
    const int cycle = static_cast<int>(m_testTimer) % 4;

    for (const uint64_t id : m_testEntities) {
        const Entity e = EntityUtils::Unpack(id);
        if (!world.IsAlive(e)) continue;

        switch (cycle) {
        case 0:
            // Add Active component
            if (!world.Has<Components::Active>(e)) {
                world.Add<Components::Active>(e, Components::Active{ true });
                if (world.Has<Components::ShapeCircle2D>(e)) {
                    auto& circle = world.Get<Components::ShapeCircle2D>(e);
                    circle.Color = Color{ 1.f, 0.f, 0.f, 1.f };
                }
            }
            break;
        case 1:
            // Add TagMask component
            if (!world.Has<Components::TagMask>(e)) {
                world.Add<Components::TagMask>(e, Components::TagMask{ 0xFF });
                if (world.Has<Components::ShapeCircle2D>(e)) {
                    auto& circle = world.Get<Components::ShapeCircle2D>(e);
                    circle.Color = Color{ 0.f, 1.f, 0.f, 1.f };
                }
            }
            break;
        case 2:
            // Remove Active component
            if (world.Has<Components::Active>(e)) {
                world.Remove<Components::Active>(e);
                if (world.Has<Components::ShapeCircle2D>(e)) {
                    auto& circle = world.Get<Components::ShapeCircle2D>(e);
                    circle.Color = Color{ 0.f, 0.f, 1.f, 1.f };
                }
            }
            break;
        case 3:
            // Remove TagMask component
            if (world.Has<Components::TagMask>(e)) {
                world.Remove<Components::TagMask>(e);
                if (world.Has<Components::ShapeCircle2D>(e)) {
                    auto& circle = world.Get<Components::ShapeCircle2D>(e);
                    circle.Color = Color{ 1.f, 1.f, 0.f, 1.f };
                }
            }
            break;
        }
    }
}
