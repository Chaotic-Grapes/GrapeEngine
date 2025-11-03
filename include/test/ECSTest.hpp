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
            // Core Component Tests
            BasicEntityCreation     = 1001,  // Create/destroy entities, add/remove components
            ComponentModification   = 1002,  // Get/Set components, check Has()
            LayerSystem             = 1003,  // Layer assignment, filtering
            ActiveAndTags           = 1004,  // Active flags, TagMask filtering
            
            // Transform Tests
            LocalTransformTest      = 1005,  // Position, rotation, scale
            WorldTransformTest      = 1006,  // Hierarchy, parent-child relationships
            TransformInterpolation  = 1007,  // Smooth position/rotation changes
            
            // Physics System Tests
            PhysicsBasic            = 1008,  // Linear velocity, basic movement
            PhysicsGravity          = 1009,  // Falling objects with gravity
            PhysicsCollision        = 1010,  // Circle-circle collision detection
            PhysicsMaterial         = 1011,  // Friction, restitution (bouncing)
            PhysicsAngular          = 1012,  // Angular velocity, rotation
            PhysicsComplex          = 1013,  // Multiple interacting bodies
            
            // Renderer System Tests
            RenderShapes            = 1014,  // Circles, boxes, lines, polygons
            RenderSprites           = 1015,  // Sprite rendering with transforms
            RenderLayers            = 1016,  // Multiple layers, z-ordering
            RenderStressTest        = 1017,  // Many entities, batch performance
            
            // Lifetime System Tests
            LifetimeBasic           = 1018,  // Entities expire after timeout
            LifetimeWithPhysics     = 1019,  // Moving entities that expire
            LifetimeSpawner         = 1020,  // Continuous spawning/destruction
            
            // Integration Tests
            PhysicsRenderCombo      = 1021,  // Physics + Rendering together
            AllSystemsTest          = 1022,  // All systems working together
            StressTestAll           = 1023,  // Performance test with all systems
            
            // Advanced Tests
            EntityPooling           = 1024,  // Test entity reuse and generation
            SpriteAnimation         = 1025,  // Test AnimationSystem with sprite sheets
            ArchetypeChanges        = 1026,  // Adding/removing components dynamically
        };

    private:
        // Test state
        bool m_tHandled = false;  // Key press handler for test switching
        TestType m_currentTest{ TestType::BasicEntityCreation };
        
        // Layers
        uint16_t m_boundaryLayer = 0;
        uint16_t m_testLayer = 0;
        uint16_t m_physicsLayer = 0;
        uint16_t m_renderLayer = 0;
        
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
        
        // ------------------------------------
        // Test case implementations
        // ------------------------------------
        
        // Core Component Tests
        void _testBasicEntityCreation();
        void _testComponentModification();
        void _testLayerSystem();
        void _testActiveAndTags();
        
        // Transform Tests
        void _testLocalTransform();
        void _testWorldTransform();
        void _testTransformInterpolation();
        
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
