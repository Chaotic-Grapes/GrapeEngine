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
    m_rendererSystem->Initialize(GetWorld());
    AddSystem([this](Scenes::Scene& s, const float dt) {
        m_rendererSystem->Update(s.GetWorld(), dt);
    }, "Renderer System");

    RunAutomatedTest();
}

void SerializationTestScene::RunAutomatedTest() {
    LOG_DEBUG("PHASE 1: Loading Prefab from assets/samples/sample-enemy-prefab.prefab");
    LoadPrefabAndVerify();
    
    LOG_DEBUG("PHASE 2: Verifying Prefab Data");
    if (m_testPassed && GetWorld().IsAlive(m_loadedEntity)) {
        VerifyPrefabData(m_loadedEntity);
    }
    else {
        LOG_ERROR("TEST STOPPED: Prefab load failed");
        m_testPassed = false;
    }
    
    PrintTestResults();
    LOG_DEBUG("Test complete. Check above for any failures.");
}

void SerializationTestScene::LoadPrefabAndVerify() {
    ECS::World& world = GetWorld();
    
    try {
        json prefabJson;
        if (!Serialization::Serializer::LoadJson("assets/samples/sample-enemy-prefab.prefab", "prefab", prefabJson)) {
            LOG_ERROR("[FAIL] Cannot load prefab JSON");
            m_testPassed = false;
            return;
        }
        
        // Deserialize the prefab entity
        m_loadedEntity = Serialization::EntitySerializer::DeserializeEntity(world, prefabJson);
        
        if (!world.IsAlive(m_loadedEntity)) {
            LOG_ERROR("[FAIL] Loaded entity is not alive");
            m_testPassed = false;
            return;
        }
        
        LOG_INFO("[PASS] Prefab loaded successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR("[FAIL] Error during prefab load: " << e.what());
        m_testPassed = false;
    }
}

void SerializationTestScene::VerifyPrefabData(ECS::Entity entity) {
    ECS::World& world = GetWorld();
    bool allTestsPassed = true;
    
    // Expected data from sample-enemy-prefab.prefab
    const std::string expectedName = "EnemyPrefab";
    const Vector3D expectedPosition = {650.0f, 750.0f, 0.0f};
    const Vector3D expectedScale = {256.0f, 256.0f, 1.0f};
    const Quaternion expectedRotation = {0.0f, 0.0f, 0.0f, 1.0f};
    const uint32_t expectedTextureId = 1;
    const Color expectedColor = {1.0f, 0.498039f, 0.498039f, 1.0f};
    const uint16_t expectedLayerId = 0;
    
    // Verify Name component
    if (world.Has<ECS::Components::Name>(entity)) {
        const auto& nameComp = world.Get<ECS::Components::Name>(entity);
        bool nameMatch = (std::strcmp(nameComp.Value, expectedName.c_str()) == 0);
        LOG_DEBUG("  Name: " << (nameMatch ? "[PASS]" : "[FAIL]") 
                  << " (Expected: '" << expectedName << "', Got: '" << nameComp.Value << "')");
        if (!nameMatch) allTestsPassed = false;
    }
    else {
        LOG_ERROR("[FAIL] Entity missing Name component");
        allTestsPassed = false;
    }
    
    // Verify Layer component
    if (world.Has<ECS::Components::Layer>(entity)) {
        const auto& layerComp = world.Get<ECS::Components::Layer>(entity);
        bool layerMatch = (layerComp.Id == expectedLayerId);
        LOG_DEBUG("  Layer: " << (layerMatch ? "[PASS]" : "[FAIL]")
                  << " (Expected: " << expectedLayerId << ", Got: " << layerComp.Id << ")");
        if (!layerMatch) allTestsPassed = false;
    }
    else {
        LOG_ERROR("[FAIL] Entity missing Layer component");
        allTestsPassed = false;
    }
    
    // Verify LocalTransform component
    if (world.Has<ECS::Components::LocalTransform>(entity)) {
        const auto& transform = world.Get<ECS::Components::LocalTransform>(entity);
        
        bool posMatch = (std::abs(transform.Position.X - expectedPosition.X) < 0.01f &&
                       std::abs(transform.Position.Y - expectedPosition.Y) < 0.01f &&
                       std::abs(transform.Position.Z - expectedPosition.Z) < 0.01f);
        LOG_DEBUG("  Position: " << (posMatch ? "[PASS]" : "[FAIL]")
                  << " (Expected: [" << expectedPosition.X << ", " << expectedPosition.Y << ", " << expectedPosition.Z << "], "
                  << "Got: [" << transform.Position.X << ", " << transform.Position.Y << ", " << transform.Position.Z << "])");
        if (!posMatch) allTestsPassed = false;
        
        bool scaleMatch = (std::abs(transform.Scale.X - expectedScale.X) < 0.01f &&
                         std::abs(transform.Scale.Y - expectedScale.Y) < 0.01f &&
                         std::abs(transform.Scale.Z - expectedScale.Z) < 0.01f);
        LOG_DEBUG("  Scale: " << (scaleMatch ? "[PASS]" : "[FAIL]")
                  << " (Expected: [" << expectedScale.X << ", " << expectedScale.Y << ", " << expectedScale.Z << "], "
                  << "Got: [" << transform.Scale.X << ", " << transform.Scale.Y << ", " << transform.Scale.Z << "])");
        if (!scaleMatch) allTestsPassed = false;
        
        bool rotMatch = (std::abs(transform.Rotation.W - expectedRotation.W) < 0.01f &&
                       std::abs(transform.Rotation.X - expectedRotation.X) < 0.01f &&
                       std::abs(transform.Rotation.Y - expectedRotation.Y) < 0.01f &&
                       std::abs(transform.Rotation.Z - expectedRotation.Z) < 0.01f);
        LOG_DEBUG("  Rotation: " << (rotMatch ? "[PASS]" : "[FAIL]")
                  << " (Expected: [" << expectedRotation.W << ", " << expectedRotation.X << ", " 
                  << expectedRotation.Y << ", " << expectedRotation.Z << "], "
                  << "Got: [" << transform.Rotation.W << ", " << transform.Rotation.X << ", " 
                  << transform.Rotation.Y << ", " << transform.Rotation.Z << "])");
        if (!rotMatch) allTestsPassed = false;
    }
    else {
        LOG_ERROR("[FAIL] Entity missing LocalTransform component");
        allTestsPassed = false;
    }
    
    // Verify SpriteRenderer2D component
    if (world.Has<ECS::Components::SpriteRenderer2D>(entity)) {
        const auto& sprite = world.Get<ECS::Components::SpriteRenderer2D>(entity);
        
        bool texMatch = (sprite.TextureId == expectedTextureId);
        LOG_DEBUG("  TextureId: " << (texMatch ? "[PASS]" : "[FAIL]")
                  << " (Expected: " << expectedTextureId << ", Got: " << sprite.TextureId << ")");
        if (!texMatch) allTestsPassed = false;
        
        bool colorMatch = (std::abs(sprite.Color.R - expectedColor.R) < 0.01f &&
                         std::abs(sprite.Color.G - expectedColor.G) < 0.01f &&
                         std::abs(sprite.Color.B - expectedColor.B) < 0.01f &&
                         std::abs(sprite.Color.A - expectedColor.A) < 0.01f);
        LOG_DEBUG("  Color: " << (colorMatch ? "[PASS]" : "[FAIL]")
                  << " (Expected: [" << expectedColor.R << ", " << expectedColor.G << ", " 
                  << expectedColor.B << ", " << expectedColor.A << "], "
                  << "Got: [" << sprite.Color.R << ", " << sprite.Color.G << ", " 
                  << sprite.Color.B << ", " << sprite.Color.A << "])");
        if (!colorMatch) allTestsPassed = false;
    }
    else {
        LOG_ERROR("[FAIL] Entity missing SpriteRenderer2D component");
        allTestsPassed = false;
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
    if (GetWorld().IsAlive(m_loadedEntity)) {
        GetWorld().Destroy(m_loadedEntity);
    }
}
