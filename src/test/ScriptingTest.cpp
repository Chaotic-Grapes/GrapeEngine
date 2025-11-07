/* Start Header *****************************************************************/
/*!
\file   ScriptingTest.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   27th October 2025
\brief
Implementation of ScriptingTestScene - demonstrates C# scripting integration
with multiple unique game object behaviors.

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

    // Initialize renderer system
    m_rendererSystem = std::make_shared<RendererSystem>();
    m_rendererSystem->Initialize(GetWorld());
    AddSystem([this](Scenes::Scene& s, const float dt) {
        m_rendererSystem->Update(s.GetWorld(), dt);
    }, "Renderer System");

    // ------------------------------------------------------------
    // Create persistent scene camera (centered on viewport)
    // This camera is NOT added to the scripted controller entities so it
    // will remain alive throughout the scene lifetime.
    {
        ECS::World& world = GetWorld();
        ECS::Entity cam = world.Create();

        auto& camTr = world.Add<ECS::Components::LocalTransform>(cam);
        camTr.Position = Vector3D{ m_worldWidth * 0.5f, m_worldHeight * 0.5f, 10.f };
        camTr.Rotation = Quaternion::Identity();
        camTr.Scale = Vector3D{ 1.f, 1.f, 1.f };

        auto& camera = world.Add<ECS::Components::Camera3D>(cam);
        camera.Active = true;
        camera.UsePerspective = false;
        camera.OrthoSize = m_worldHeight;
        camera.NearPlane = 0.1f;
        camera.FarPlane = 100.f;

        const auto window = WindowManager::GetMainWindow();
        if (window) {
            camera.AspectRatio = static_cast<float>(window->Width()) / static_cast<float>(window->Height());
        }

        // Optional debug name
        world.Add<Components::Name>(cam).Value[0] = '\0';
        strncpy_s(world.Get<Components::Name>(cam).Value, "Scripting_Scene_Camera", 63);

        m_cameraEntity = EntityUtils::Pack(cam);
        LOG_DEBUG("Created persistent scripting scene camera at (" << camTr.Position.X << ", " << camTr.Position.Y << ")");
    }

    // AddSystem([](Scenes::Scene& s, const float dt) {
    //     ECS::PhysicsSystem::Update(s.GetWorld(), dt);
    // }, "Physics System");

    // Initialize ScriptSystem
    _initializeScriptSystem();

    // Create a single manager entity that will orchestrate everything from C#
    Entity managerEntity = GetWorld().Create();
    GetWorld().Add<Components::Active>(managerEntity, Components::Active{ true });
    
    // Attach EntityManager script - it will create all entities, boundaries, and logic
    if (!m_scriptSystem->AttachScript(GetWorld(), managerEntity, "MyGame.EntityManager")) {
        LOG_ERROR("FATAL: Failed to attach EntityManager script!");
        return;
    }

    // Also create controller entities for each test script
    // The EntityManager creates visual entities, but we need separate entities for script controllers
    _createScriptedEntities();

    LOG_INFO("ScriptingTestScene initialized");
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

    // Set world pointer for C# API
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
    LOG_INFO("Creating scripted controller entities...");

    // We just create minimal controller entities and attach scripts.

    _createPlayer();
    _createEnemy(1);
    _createEnemy(2);
    // _createCollectible(1);
    // _createCollectible(2);

    LOG_INFO("All scripted controller entities created");
}

void ScriptingTestScene::_createPlayer() {
    LOG_INFO("Creating Player entity with C# script...");

    m_playerEntity = GetWorld().Create();

    // Add active component (required for scripts to run)
    GetWorld().Add<Components::Active>(m_playerEntity, Components::Active{ true });

    // ATTACH C# SCRIPT
    // The script will create its own visual entity and components
    if (!m_scriptSystem->AttachScript(GetWorld(), m_playerEntity, "MyGame.PlayerController"))
        LOG_ERROR("Failed to attach PlayerController script!");
    else
        LOG_INFO("PlayerController script attached successfully");
}

void ScriptingTestScene::_createEnemy(int enemyNumber) {
    LOG_INFO("Creating Enemy " << enemyNumber << " entity with C# script...");

    Entity enemyEntity = GetWorld().Create();

    // Add active component
    GetWorld().Add<Components::Active>(enemyEntity, Components::Active{ true });

    // ATTACH DIFFERENT C# SCRIPT
    // The script will create its own visual entity and components
    // Using EnemyAI script - patrol behavior
    if (!m_scriptSystem->AttachScript(GetWorld(), enemyEntity, "MyGame.EnemyAI"))
        LOG_ERROR("Failed to attach EnemyAI script to Enemy " << enemyNumber);
    else
        LOG_INFO("EnemyAI script attached to Enemy " << enemyNumber);

    // Store references
    if (enemyNumber == 1)
        m_enemy1Entity = enemyEntity;
    else
        m_enemy2Entity = enemyEntity;
}

// void ScriptingTestScene::_createCollectible(int collectibleNumber) {
//     LOG_INFO("Creating Collectible " << collectibleNumber << " entity with C# script...");

//     Entity collectibleEntity = GetWorld().Create();

//     // Add active component
//     GetWorld().Add<Components::Active>(collectibleEntity, Components::Active{ true });

//     // ATTACH COLLECTIBLE C# SCRIPT (bobbing + rainbow colors)
//     // The script will create its own visual entity
//     if (!m_scriptSystem->AttachScript(GetWorld(), collectibleEntity, "MyGame.CollectibleItem"))
//         LOG_ERROR("Failed to attach CollectibleItem script " << collectibleNumber);
//     else
//         LOG_INFO("CollectibleItem script " << collectibleNumber << " attached successfully");

//     // Store references
//     if (collectibleNumber == 1)
//         m_rotatingEntity = collectibleEntity;
//     else
//         m_oscillatingEntity = collectibleEntity;
// }