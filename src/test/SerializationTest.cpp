/**
 * @file    SerializationTest.cpp
 * @author  k.danielneozuofeng
 * @date    26/09/2025
 * @brief   Implementation of automated serialization test scene
 *
 * This source file implements the SerializationTestScene class,
 * which performs a multi-phase integrity test of the serialization
 * system. It creates sample entities, saves them to a JSON scene file,
 * reloads them, and verifies that entity data (such as transform,
 * components, and names) are preserved correctly across the process.
 *
 * Test phases include:
 *  - Creating controlled test entities
 *  - Saving the current world to disk
 *  - Loading the saved world back into memory
 *  - Verifying loaded entities against expected values
 *  - Printing final pass/fail test results
 */

#include "SerializationTest.h"
#include "serialization/EntitySerializer.h"
#include "serialization/Serializer.h"
#include "ecs/Components.h"
#include "core/Logger.h"
#include "core/Application.h"
#include "services/WindowManager.h"
#include <filesystem>
#include <cmath>
#include <cstring>

using namespace Sandbox;

void SerializationTestScene::OnLoad() {
    const auto &config = Engine::CORE->GetConfig();
    const int windowWidth = config.WindowConfig.Width;
    const int windowHeight = config.WindowConfig.Height;

    CREATE_WINDOW("Serialization Test Scene", windowWidth, windowHeight);
    LOG_INFO("========================================");
    LOG_INFO("SERIALIZATION INTEGRITY TEST");
    LOG_INFO("========================================");

    m_rendererSystem = std::make_shared<ECS::RendererSystem>();
    m_rendererSystem->Initialize();
    AddSystem([this](Scenes::Scene& s, const float dt) {
        m_rendererSystem->Update(s.GetWorld(), dt);
    }, "Renderer System");

    RunAutomatedTest();
}

void SerializationTestScene::RunAutomatedTest() {
    LOG_DEBUG("PHASE 1: Creating Test Entities");
    CreateTestEntities();
    
    LOG_DEBUG("PHASE 2: Saving Scene");
    SaveScene();
    if (!m_testPassed) {
        LOG_ERROR("TEST STOPPED: Scene save failed"); 
        return;
    }

    LOG_DEBUG("PHASE 3: Loading Scene");
    LoadScene();
    if (!m_testPassed) {
        LOG_ERROR("TEST STOPPED: Scene load failed"); 
        return;
    }

    LOG_DEBUG("PHASE 4: Verifying Loaded Data");
    VerifyLoadedEntities();
    PrintTestResults();

    LOG_DEBUG("Test complete. Check above for any failures.");
}

void SerializationTestScene::CreateTestEntities() {
    ECS::World& world = GetWorld();
    
    // Test 1: Entity with LocalTransform + SpriteRenderer2D + Name
    ECS::Entity sprite = world.Create();
    world.Add<ECS::Components::Name>(sprite);
    auto& spriteName = world.Get<ECS::Components::Name>(sprite); // This assumes entity has Name component
    std::strncpy(spriteName.Value, "TestSprite", sizeof(spriteName.Value) - 1);
    
    world.Add<ECS::Components::LocalTransform>(sprite);
    auto& spriteTransform = world.Get<ECS::Components::LocalTransform>(sprite);
    spriteTransform.Position = Vector3D{400.0f, 450.0f, 0.0f};
    spriteTransform.Scale = Vector3D{128.0f, 128.0f, 1.0f};
    spriteTransform.Rotation = Quaternion{0.0f, 0.0f, 0.382683f, 0.923880f};
    
    world.Add<ECS::Components::SpriteRenderer2D>(sprite);
    auto& spriteRenderer = world.Get<ECS::Components::SpriteRenderer2D>(sprite);
    spriteRenderer.TextureId = 1;
    spriteRenderer.Color = Color{1.0f, 0.5f, 0.5f, 1.0f};
    
    m_originalEntities.push_back(sprite);
    m_expectedData.push_back({"TestSprite", Vector3D{400.0f, 450.0f, 0.0f},  
        Vector3D{128.0f, 128.0f, 1.0f}, Quaternion{0.0f, 0.0f, 0.382683f, 0.923880f}, 1, 0.0f, 0.0f});
    
    // Test 2: Entity with LocalTransform + LinearVelocity2D
    ECS::Entity movingEntity = world.Create();
    world.Add<ECS::Components::Name>(movingEntity);
    auto& movingName = world.Get<ECS::Components::Name>(movingEntity);
    std::strncpy(movingName.Value, "MovingEntity", sizeof(movingName.Value) - 1);
    
    world.Add<ECS::Components::LocalTransform>(movingEntity);
    auto& movingTransform = world.Get<ECS::Components::LocalTransform>(movingEntity);
    movingTransform.Position = Vector3D{100.0f, 100.0f, 0.0f};
    movingTransform.Scale = Vector3D{50.0f, 50.0f, 1.0f};
    
    world.Add<ECS::Components::LinearVelocity2D>(movingEntity);
    auto& velocity = world.Get<ECS::Components::LinearVelocity2D>(movingEntity);
    velocity.Value = Vector2D{10.0f, 5.0f};
    
    m_originalEntities.push_back(movingEntity);
    m_expectedData.push_back({"MovingEntity", Vector3D{100.0f, 100.0f, 0.0f},
        Vector3D{50.0f, 50.0f, 1.0f}, Quaternion{0.0f, 0.0f, 0.0f, 1.0f}, 0, 10.0f, 5.0f});
    
    LOG_DEBUG("Created " << m_originalEntities.size() << " test entities");
}

void SerializationTestScene::SaveScene() {
    ECS::World& world = GetWorld();
    std::filesystem::create_directories("assets/samples");
    
    try {
        json sceneJson;
        sceneJson["Version"] = "1.0";
        sceneJson["SceneName"] = "SerializationTest";
        sceneJson["EntityCount"] = m_originalEntities.size();
        
        json entities = json::array();
        for (const auto& entity : m_originalEntities) {
            entities.push_back(Serialization::EntitySerializer::SerializeEntity(world, entity));
        }
        sceneJson["Entities"] = std::move(entities);
        
        if (!Serialization::Serializer::SaveJson("assets/samples/sample-scene.scn", "scn", sceneJson)) {
            LOG_ERROR("[FAIL] Scene save failed");
            m_testPassed = false;
            return;
        }
        LOG_INFO("[PASS] Scene saved successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("[FAIL] Error during save: " << e.what());
        m_testPassed = false;
    }
}

void SerializationTestScene::LoadScene() {
    ECS::World& world = GetWorld();
    
    for (auto& entity : m_originalEntities) {
        if (world.IsAlive(entity)) {
            world.Destroy(entity);
        }
    }
    m_originalEntities.clear();
    
    try {
        json sceneJson;
        if (!Serialization::Serializer::LoadJson("assets/samples/sample-scene.scn", "scn", sceneJson)) {
            LOG_ERROR("[FAIL] Cannot load scene JSON");
            m_testPassed = false;
            return;
        }
        
        int loadedCount = 0;
        if (sceneJson.contains("Entities")) {
            for (const auto& entityJson : sceneJson["Entities"]) {
                ECS::Entity newEntity = Serialization::EntitySerializer::DeserializeEntity(world, entityJson);
                m_originalEntities.push_back(newEntity);
                loadedCount++;
            }
        }
        
        if (loadedCount == 0) {
            LOG_ERROR("[FAIL] No entities were loaded");
            m_testPassed = false;
            return;
        }
        LOG_INFO("[PASS] Loaded " << loadedCount << " entities");
    }
    catch (const std::exception& e) {
        LOG_ERROR("[FAIL] Error during load: " << e.what());
        m_testPassed = false;
    }
}

void SerializationTestScene::VerifyLoadedEntities() {
    ECS::World& world = GetWorld();
    bool allTestsPassed = true;
    
    for (const auto& entity : m_originalEntities) {
        if (!world.IsAlive(entity)) {
            LOG_ERROR("[FAIL] Entity is not alive");
            allTestsPassed = false;
            continue;
        }
        
        std::string name = "Unknown";
        if (world.Has<ECS::Components::Name>(entity)) {
            const auto& nameComp = world.Get<ECS::Components::Name>(entity);
            name = nameComp.Value;
        }
        
        const ExpectedData* expected = nullptr;
        for (auto& exp : m_expectedData) {
            if (exp.name == name) {
                expected = &exp;
                break;
            }
        }
        
        if (!expected) continue;
        
        if (world.Has<ECS::Components::LocalTransform>(entity)) {
            const auto& transform = world.Get<ECS::Components::LocalTransform>(entity);
            bool posMatch = (std::abs(transform.Position.X - expected->position.X) < 0.01f &&
                           std::abs(transform.Position.Y - expected->position.Y) < 0.01f &&
                           std::abs(transform.Position.Z - expected->position.Z) < 0.01f);
            bool scaleMatch = (std::abs(transform.Scale.X - expected->scale.X) < 0.01f &&
                             std::abs(transform.Scale.Y - expected->scale.Y) < 0.01f &&
                             std::abs(transform.Scale.Z - expected->scale.Z) < 0.01f);
            
            LOG_DEBUG("  " << name << " - Position: " << (posMatch ? "[PASS]" : "[FAIL]"));
            LOG_DEBUG("  " << name << " - Scale: " << (scaleMatch ? "[PASS]" : "[FAIL]"));
            
            if (!posMatch || !scaleMatch) allTestsPassed = false;
        }
        
        if (name == "TestSprite" && world.Has<ECS::Components::SpriteRenderer2D>(entity)) {
            const auto& sprite = world.Get<ECS::Components::SpriteRenderer2D>(entity);
            bool texMatch = sprite.TextureId == expected->textureId;
            LOG_DEBUG("  " << name << " - TextureId: " << (texMatch ? "[PASS]" : "[FAIL]"));
            if (!texMatch)
                allTestsPassed = false;
        }
        
        if (name == "MovingEntity" && world.Has<ECS::Components::LinearVelocity2D>(entity)) {
            const auto& vel = world.Get<ECS::Components::LinearVelocity2D>(entity);
            bool velMatch = (std::abs(vel.Value.X - expected->linearVelocityX) < 0.01f &&
                           std::abs(vel.Value.Y - expected->linearVelocityY) < 0.01f);
            LOG_DEBUG("  " << name << " - Velocity: " << (velMatch ? "[PASS]" : "[FAIL]"));
            if (!velMatch)
                allTestsPassed = false;
        }
    }
    
    m_testPassed = allTestsPassed;
}

void SerializationTestScene::PrintTestResults() {
    LOG_INFO("========================================");
    if (m_testPassed)
        LOG_INFO("[PASS] ALL TESTS PASSED");
    else
        LOG_ERROR("[FAIL] SOME TESTS FAILED");
    
    LOG_INFO("========================================");

    // exit(m_testPassed ? 0 : 1);
}

void SerializationTestScene::OnUpdate() {}
void SerializationTestScene::OnUnload() {
    m_originalEntities.clear();
    m_expectedData.clear();
}
