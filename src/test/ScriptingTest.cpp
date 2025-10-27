/* Start Header *****************************************************************/
/*!
\file   ScriptingTest.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   27th October 2025
\brief
Implementation of ScriptingTestScene - demonstrates C# scripting integration
with multiple unique game object behaviors.

RUBRIC DEMONSTRATION:
- Creates at least 5 different game objects
- Each runs unique C# script logic
- Player: PlayerController script (figure-8 movement)
- Enemy1, Enemy2: EnemyAI script (patrol behavior, unique instances)
- Collectible1, Collectible2: CollectibleItem script (bobbing + rainbow, unique instances)

This proves that:
1. Multiple objects can run scripts simultaneously
2. Each object has unique behavior
3. Scripts execute independently per object

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ScriptingTest.hpp"
#include "core/Application.h"
#include "core/Logger.h"
#include "ecs/Components.h"
#include "ecs/systems/PhysicsSystem.h"
#include "physics/Physics.h"
#include "helpers/EntityUtils.h"
#include "services/Input.h"
#include "services/Time.h"
#include "services/Window.h"
#include "services/WindowManager.h"
#include <iostream>

using namespace Sandbox;
using namespace ECS;

// ============================================================================
// SCENE LIFECYCLE
// ============================================================================

void ScriptingTestScene::OnLoad() {
    LOG_INFO("=== ScriptingTestScene Loading ===");
    
    const auto& config = Engine::CORE->GetConfig();
    const int windowWidth = config.WindowConfig.Width;
    const int windowHeight = config.WindowConfig.Height;

    CREATE_WINDOW("C# Scripting Test Scene", windowWidth, windowHeight);
    m_worldWidth = static_cast<float>(windowWidth);
    m_worldHeight = static_cast<float>(windowHeight);

    // Create layers for organization
    m_playerLayer = GetLayers().CreateOrGetLayer("player");
    m_enemyLayer = GetLayers().CreateOrGetLayer("enemy");
    m_propsLayer = GetLayers().CreateOrGetLayer("props");

    // Initialize renderer system
    m_rendererSystem = std::make_shared<RendererSystem>();
    m_rendererSystem->Initialize();
    AddSystem([this](Scenes::Scene& s, const float dt) {
        m_rendererSystem->Update(s.GetWorld(), dt);
    }, "Renderer System");

    // Initialize physics system (for movement)
    AddSystem([](Scenes::Scene& s, const float dt) {
        PhysicsSystem::Update(s.GetWorld(), dt);
    }, "Physics System");

    // Initialize ScriptSystem
    _initializeScriptSystem();

    // Configure world boundaries
    const float wallThickness = 20.0f;
    Engine::Physics::SetWorldBounds(
        wallThickness, 
        m_worldWidth - wallThickness, 
        wallThickness, 
        m_worldHeight - wallThickness, 
        false, 
        0.8f
    );
    Engine::Physics::EnableWorldBounds(true);

    // Create visual boundaries
    _createWorldBoundaries();

    // ========== Create all scripted entities ==========
    _createScriptedEntities();

    LOG_INFO("ScriptingTestScene initialized with " << GetSystemCount() << " systems");
    LOG_INFO("Created 5 entities with unique C# scripts:");
    LOG_INFO("  - PlayerController (green circle, figure-8 movement)");
    LOG_INFO("  - 2x EnemyAI (red circles, patrol behavior)");
    LOG_INFO("  - 2x CollectibleItem (rainbow circles, bobbing)");
    LOG_INFO("Press ESC to exit");
}

void ScriptingTestScene::OnUpdate() {
    // Scene-specific update logic can go here
    // The scripts run automatically via the registered systems
    
    // Example: Debug visualization
    static float debugTimer = 0.0f;
    debugTimer += Time::DeltaTime();
    
    if (debugTimer >= 5.0f) {
        debugTimer = 0.0f;
        LOG_INFO("Scripts running: Player (figure-8), 2 Enemies (patrol), 2 Collectibles (bobbing)");
    }
}

void ScriptingTestScene::OnUnload() {
    LOG_INFO("=== ScriptingTestScene Unloading ===");
    
    // Cleanup script system
    if (m_scriptSystem) {
        m_scriptSystem->OnDestroy(GetWorld());
        m_scriptSystem->Shutdown();
    }

    if (m_rendererSystem) {
        m_rendererSystem.reset();
    }

    LOG_INFO("ScriptingTestScene unloaded");
}

// ============================================================================
// SCRIPT SYSTEM INITIALIZATION
// ============================================================================

void ScriptingTestScene::_initializeScriptSystem() {
    LOG_INFO("Initializing C# ScriptSystem...");

    // Create script system
    m_scriptSystem = std::make_shared<ScriptSystem>();

    LOG_INFO(std::filesystem::current_path());

    // Initialize CoreCLR runtime    
    if (!m_scriptSystem->Initialize()) {
        LOG_ERROR("FATAL: Failed to initialize ScriptSystem!");
        LOG_ERROR("Is .NET runtime is installed and are scripts built?");
        return;
    }

    // Load game scripts assembly
    // Write this for now    
    if (!m_scriptSystem->LoadAssembly("MyGame.dll")) {
        LOG_ERROR("FATAL: Failed to load game scripts assembly!");
        LOG_ERROR("Is MyGame.dll is built? (CMake should auto-build it)");
        return;
    }

    // CRITICAL: Set world pointer for C# API
    ScriptAPI_SetWorld(&GetWorld());

    // ========== Register script system update methods ==========
    // This is what actually makes the scripts run!

    // OnStart - called once to initialize scripts
    AddSystem([this](Scenes::Scene& s, const float dt) {
        (void)dt;
        m_scriptSystem->OnStart(s.GetWorld());
    }, "Script OnStart");

    // Update - called every frame
    AddSystem([this](Scenes::Scene& s, const float dt) {
        (void)dt;
        m_scriptSystem->Update(s.GetWorld());
    }, "Script Update");

    // FixedUpdate - called at fixed intervals (for physics)
    AddSystem([this](Scenes::Scene& s, const float dt) {
        (void)dt;
        m_scriptSystem->FixedUpdate(s.GetWorld());
    }, "Script FixedUpdate");

    // LateUpdate - called after all updates
    AddSystem([this](Scenes::Scene& s, const float dt) {
        (void)dt;
        m_scriptSystem->LateUpdate(s.GetWorld());
    }, "Script LateUpdate");

    LOG_INFO("ScriptSystem initialized successfully!");
}

// ============================================================================
// CREATE SCRIPTED ENTITIES
// ============================================================================

void ScriptingTestScene::_createScriptedEntities() {
    LOG_INFO("Creating scripted entities...");

    // Create player (center of screen)
    _createPlayer();

    // Create two enemies at different positions
    _createEnemy(200.0f, 200.0f, 1);
    _createEnemy(1080.0f, 520.0f, 2);

    // Create rotating object
    _createRotatingObject();

    // Create oscillating object
    _createOscillatingObject();

    LOG_INFO("All scripted entities created!");
}

void ScriptingTestScene::_createPlayer() {
    LOG_INFO("Creating Player entity with C# script...");

    m_playerEntity = GetWorld().Create();

    // Add transform component
    GetWorld().Add<Components::LocalTransform>(m_playerEntity, Components::LocalTransform{
        Vector3D{ m_worldWidth * 0.5f, m_worldHeight * 0.5f, 0.0f },
        Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f },
        Vector3D{ 1.0f, 1.0f, 1.0f }
    });

    // Add active component (required for scripts to run)
    GetWorld().Add<Components::Active>(m_playerEntity, Components::Active{ true });

    // Add visual representation (green circle)
    GetWorld().Add<Components::ShapeCircle2D>(m_playerEntity, Components::ShapeCircle2D{
        20.0f,
        Vector2D{},
        Color{ 0.0f, 1.0f, 0.0f, 1.0f }  // Green
    });

    GetWorld().Add<Components::ZIndex2D>(m_playerEntity, Components::ZIndex2D{ 10 });

    // ATTACH C# SCRIPT
    // This script handles player movement
    if (!m_scriptSystem->AttachScript(GetWorld(), m_playerEntity, "TestGame.PlayerController"))
        LOG_ERROR("Failed to attach PlayerController script!");
    else
        LOG_INFO("PlayerController script attached successfully");
}

void ScriptingTestScene::_createEnemy(float x, float y, int enemyNumber) {
    LOG_INFO("Creating Enemy " << enemyNumber << " entity with C# script...");

    Entity enemyEntity = GetWorld().Create();

    // Add transform component
    GetWorld().Add<Components::LocalTransform>(enemyEntity, Components::LocalTransform{
        Vector3D{ x, y, 0.0f },
        Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f },
        Vector3D{ 1.0f, 1.0f, 1.0f }
    });

    // Add active component
    GetWorld().Add<Components::Active>(enemyEntity, Components::Active{ true });

    // Add visual representation (red circle)
    GetWorld().Add<Components::ShapeCircle2D>(enemyEntity, Components::ShapeCircle2D{
        15.0f,
        Vector2D{},
        Color{ 1.0f, 0.0f, 0.0f, 1.0f }  // Red
    });

    GetWorld().Add<Components::ZIndex2D>(enemyEntity, Components::ZIndex2D{ 5 });

    // ATTACH DIFFERENT C# SCRIPT
    // This demonstrates that different objects can have different scripts
    // Using EnemyAI script - patrol behavior
    if (!m_scriptSystem->AttachScript(GetWorld(), enemyEntity, "TestGame.EnemyAI"))
        LOG_ERROR("Failed to attach EnemyAI script to Enemy " << enemyNumber);
    else
        LOG_INFO("EnemyAI script attached to Enemy " << enemyNumber);

    // Store references
    if (enemyNumber == 1)
        m_enemy1Entity = enemyEntity;
    else
        m_enemy2Entity = enemyEntity;
}

void ScriptingTestScene::_createRotatingObject() {
    LOG_INFO("Creating First Collectible entity with C# script...");

    m_rotatingEntity = GetWorld().Create();

    // Add transform component
    GetWorld().Add<Components::LocalTransform>(m_rotatingEntity, Components::LocalTransform{
        Vector3D{ 640.0f, 200.0f, 0.0f },
        Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f },
        Vector3D{ 1.0f, 1.0f, 1.0f }
    });

    // Add active component
    GetWorld().Add<Components::Active>(m_rotatingEntity, Components::Active{ true });

    // Add visual representation (blue box)
    GetWorld().Add<Components::ShapeBox2D>(m_rotatingEntity, Components::ShapeBox2D{
        Vector2D{40.0f, 40.0f},
        Vector2D{},
        Color{ 0.0f, 0.0f, 1.0f, 1.0f }  // Blue
    });

    GetWorld().Add<Components::ZIndex2D>(m_rotatingEntity, Components::ZIndex2D{ 3 });

    // ATTACH COLLECTIBLE C# SCRIPT (bobbing + rainbow colors)
    if (!m_scriptSystem->AttachScript(GetWorld(), m_rotatingEntity, "TestGame.CollectibleItem"))
        LOG_ERROR("Failed to attach CollectibleItem script!");
    else
        LOG_INFO("CollectibleItem script attached successfully");
}

void ScriptingTestScene::_createOscillatingObject() {
    LOG_INFO("Creating Third Collectible entity with C# script...");

    m_oscillatingEntity = GetWorld().Create();

    // Add transform component
    GetWorld().Add<Components::LocalTransform>(m_oscillatingEntity, Components::LocalTransform{
        Vector3D{ 640.0f, 520.0f, 0.0f },
        Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f },
        Vector3D{ 1.0f, 1.0f, 1.0f }
    });

    // Add active component
    GetWorld().Add<Components::Active>(m_oscillatingEntity, Components::Active{ true });

    // Add visual representation (magenta circle)
    GetWorld().Add<Components::ShapeCircle2D>(m_oscillatingEntity, Components::ShapeCircle2D{
        18.0f,
        Vector2D{},
        Color{ 1.0f, 0.0f, 1.0f, 1.0f }  // Magenta
    });

    GetWorld().Add<Components::ZIndex2D>(m_oscillatingEntity, Components::ZIndex2D{ 3 });

    // ATTACH COLLECTIBLE C# SCRIPT (another instance, different position)
    if (!m_scriptSystem->AttachScript(GetWorld(), m_oscillatingEntity, "TestGame.CollectibleItem"))
        LOG_ERROR("Failed to attach CollectibleItem script!");
    else
        LOG_INFO("CollectibleItem script attached successfully (instance 2)");
}

// ============================================================================
// WORLD BOUNDARIES
// ============================================================================

void ScriptingTestScene::_createWorldBoundaries() {
    const float wallThickness = 20.0f;
    const Color wallColor = { 0.5f, 0.5f, 0.5f, 1.0f };  // Gray

    // Top wall
    Entity topWall = GetWorld().Create();
    GetWorld().Add<Components::LocalTransform>(topWall, Components::LocalTransform{
        Vector3D{ m_worldWidth * 0.5f, wallThickness * 0.5f, 0.0f },
        Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f },
        Vector3D{ 1.0f, 1.0f, 1.0f }
    });
    GetWorld().Add<Components::ShapeBox2D>(topWall, Components::ShapeBox2D{
        Vector2D{ m_worldWidth, wallThickness },
        Vector2D{},
        wallColor
    });
    GetWorld().Add<Components::ZIndex2D>(topWall, Components::ZIndex2D{ 0 });

    // Bottom wall
    Entity bottomWall = GetWorld().Create();
    GetWorld().Add<Components::LocalTransform>(bottomWall, Components::LocalTransform{
        Vector3D{ m_worldWidth * 0.5f, m_worldHeight - wallThickness * 0.5f, 0.0f },
        Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f },
        Vector3D{ 1.0f, 1.0f, 1.0f }
    });
    GetWorld().Add<Components::ShapeBox2D>(bottomWall, Components::ShapeBox2D{
        Vector2D{ m_worldWidth, wallThickness },
        Vector2D{},
        wallColor
    });
    GetWorld().Add<Components::ZIndex2D>(bottomWall, Components::ZIndex2D{ 0 });

    // Left wall
    Entity leftWall = GetWorld().Create();
    GetWorld().Add<Components::LocalTransform>(leftWall, Components::LocalTransform{
        Vector3D{ wallThickness * 0.5f, m_worldHeight * 0.5f, 0.0f },
        Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f },
        Vector3D{ 1.0f, 1.0f, 1.0f }
    });
    GetWorld().Add<Components::ShapeBox2D>(leftWall, Components::ShapeBox2D{
        Vector2D{ wallThickness, m_worldHeight },
        Vector2D{},
        wallColor
    });
    GetWorld().Add<Components::ZIndex2D>(leftWall, Components::ZIndex2D{ 0 });

    // Right wall
    Entity rightWall = GetWorld().Create();
    GetWorld().Add<Components::LocalTransform>(rightWall, Components::LocalTransform{
        Vector3D{ m_worldWidth - wallThickness * 0.5f, m_worldHeight * 0.5f, 0.0f },
        Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f },
        Vector3D{ 1.0f, 1.0f, 1.0f }
    });
    GetWorld().Add<Components::ShapeBox2D>(rightWall, Components::ShapeBox2D{
        Vector2D{ wallThickness, m_worldHeight },
        Vector2D{},
        wallColor
    });
    GetWorld().Add<Components::ZIndex2D>(rightWall, Components::ZIndex2D{ 0 });
}
