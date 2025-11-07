/* Start Header *****************************************************************/
/*!
\file   ECSTest.hpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th October 2025
\brief
Defines the ECSTestScene, a comprehensive testbed for validating all ECS
systems, components, and their interactions. This test scene covers:

- PhysicsSystem: 2D rigidbody physics, collisions, velocity, acceleration
- RendererSystem: Shape rendering, sprites, layers, z-ordering
- LifetimeSystem: Entity destruction after timeout
- Transform hierarchy and WorldTransform updates
- Component interactions (Active, Layer, TagMask, etc.)
- Performance and stress testing for ECS operations

The scene provides structured test cases that can be cycled through using
keyboard input, with each test validating specific ECS functionality.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#include "Game.h"
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/systems/PhysicsSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "ecs/systems/LifetimeSystem.h"
#include <vector>
#include <memory>

namespace Sandbox {
    class ECSTestScene : public Scenes::Scene {
    public:
        void OnLoad() override;
        void OnUpdate() override;
        void OnUnload() override;

        enum class TestType {
            // Transformation System
            LocalTransformTest      = 1001,  // Position, rotation, scale
            WorldTransformTest      = 1002,  // Hierarchy, parent-child relationships
            TransformInterpolation  = 1003,  // Smooth position/rotation changes
            ComponentModification   = 1004,  // Get/Set components, check Has()
            RotateSquares           = 1005,  // Create 3 squares and rotate them

            // Sprite Animation
            SpriteAnimation         = 1006,  // Test AnimationSystem with sprite sheets

            // Core Component Tests
            BasicEntityCreation     = 1007,  // Create/destroy entities, add/remove components
            LayerSystem             = 1008,  // Layer assignment, filtering
            ActiveAndTags           = 1009,  // Active flags, TagMask filtering

            // Physics System Tests
            PhysicsBasic            = 1010,  // Linear velocity, basic movement
            PhysicsGravity          = 1011,  // Falling objects with gravity
            PhysicsCollision        = 1012,  // Circle-circle collision detection
            PhysicsMaterial         = 1013,  // Friction, restitution (bouncing)
            PhysicsAngular          = 1014,  // Angular velocity, rotation
            PhysicsComplex          = 1015,  // Multiple interacting bodies

            // Renderer System Tests
            RenderShapes            = 1016,  // Circles, boxes, lines, polygons
            RenderSprites           = 1017,  // Sprite rendering with transforms
            RenderLayers            = 1018,  // Multiple layers, z-ordering
            RenderStressTest        = 1019,  // Many entities, batch performance

            // Lifetime System Tests
            LifetimeBasic           = 1020,  // Entities expire after timeout
            LifetimeWithPhysics     = 1021,  // Moving entities that expire
            LifetimeSpawner         = 1022,  // Continuous spawning/destruction

            // Integration Tests
            PhysicsRenderCombo      = 1023,  // Physics + Rendering together
            AllSystemsTest          = 1024,  // All systems working together
            StressTestAll           = 1025,  // Performance test with all systems

            // Advanced Tests
            EntityPooling           = 1026,  // Test entity reuse and generation
            ArchetypeChanges        = 1027,  // Adding/removing components dynamically
        };

    private:
        // Test state
        bool m_tHandled = false;  // Key press handler for test switching
        TestType m_currentTest{ TestType::LocalTransformTest };
        
        // Layers
        uint16_t m_boundaryLayer = 0;
        uint16_t m_testLayer = 0;
        uint16_t m_physicsLayer = 0;
        uint16_t m_renderLayer = 0;
        // Layer for UI/text that should render last
        uint16_t m_textLayer = 0;
        
        // Systems
        std::shared_ptr<ECS::RendererSystem> m_rendererSystem;
        
        // Test entities (stored as packed IDs)
        std::vector<uint64_t> m_testEntities;
        
        // World dimensions
        float m_worldWidth = 1600.f;
        float m_worldHeight = 900.f;
        
        // Test-specific state
        float m_testTimer = 0.f;
        int m_spawnCount = 0;
        // Title text entity (packed id)
        uint64_t m_testTitleEntity = 0;
        
        // ------------------------------------
        // Test case implementations
        // ------------------------------------
        
        // Core Component Tests
        void _testBasicEntityCreation();
        void _testLayerSystem();
        void _testActiveAndTags();

        // Transform Tests
        void _testLocalTransform();
        void _testWorldTransform();
        void _testTransformInterpolation();
        void _testComponentModification();
        void _testRotateSquares();
        
        // Physics System Tests
        void _testPhysicsBasic();
        void _testPhysicsGravity();
        void _testPhysicsCollision();
        void _testPhysicsMaterial();
        void _testPhysicsAngular();
        void _testPhysicsComplex();
        
        // Renderer System Tests
        void _testRenderShapes();
        void _testRenderSprites();
        void _testRenderLayers();
        void _testRenderStressTest();
        
        // Lifetime System Tests
        void _testLifetimeBasic();
        void _testLifetimeWithPhysics();
        void _testLifetimeSpawner();
        
        // Integration Tests
        void _testPhysicsRenderCombo();
        void _testAllSystems();
        void _testStressTestAll();
        
        // Advanced Tests
        void _testEntityPooling();
        void _testSpriteAnimation();
        void _testArchetypeChanges();
        
        // ------------------------------------
        // Helper methods
        // ------------------------------------
        void _clearTestEntities();
        void _logTestInfo(const char* testName);
        void _cycleTests();
        void _createWorldBoundaries();
    };
}
