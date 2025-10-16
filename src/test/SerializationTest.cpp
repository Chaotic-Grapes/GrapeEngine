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
#include "serialization/Serialization.h"
#include "ecs/Components.h"
#include "services/WindowManager.h"
#include <iostream>
#include <filesystem>

using namespace Sandbox;

SerializationTestScene::SerializationTestScene()
    : Scene("SerializationTestScene") {
    CREATE_WINDOW("Serialization Test", 1600, 900);
}

void SerializationTestScene::OnLoad() {
    std::cout << "\nSERIALIZATION INTEGRITY TEST" << '\n';
    std::cout << "Running automated serialization test..." << '\n';

    RunAutomatedTest();
}

void SerializationTestScene::RunAutomatedTest() {
    std::cout << "\nPHASE 1: Creating Test Entities" << '\n';
    CreateTestEntities();

    std::cout << "\nPHASE 2: Saving Scene" << '\n';
    SaveScene();

    if (!m_testPassed) {
        std::cout << "TEST STOPPED: Save failed" << '\n';
        return;
    }

    std::cout << "\nPHASE 3: Loading Scene" << '\n';
    LoadScene();

    if (!m_testPassed) {
        std::cout << "TEST STOPPED: Load failed" << '\n';
        return;
    }

    std::cout << "\nPHASE 4: Verifying Loaded Data" << '\n';
    VerifyLoadedEntities();

    PrintTestResults();

    std::cout << "\nTest complete. Check above for any failures." << '\n';
}

void SerializationTestScene::CreateTestEntities() {
    std::cout << "CreateTestEntities() started" << '\n';

    World& world = GetWorld();
    std::cout << "Got world reference" << '\n';

    // test 1: Basic Transform + SpriteRenderer
    std::cout << "Creating TestSprite entity..." << '\n';
    Entity sprite = world.CreateEntity("TestSprite");
    std::cout << "Entity created with ID: " << sprite.GetId() << '\n';

    auto& spriteTransform = sprite.Transform();
    spriteTransform.Position = { 400.0f, 450.0f };
    spriteTransform.Scale = { 128.0f, 128.0f };
    spriteTransform.Rotation = 45.0f;
    std::cout << "Transform set" << '\n';

    auto& spriteRenderer = sprite.AddComponent<Component::SpriteRenderer>(
        "assets/textures/test/player.png"
    );
    std::cout << "SpriteRenderer component added" << '\n';
    spriteRenderer.Color = { 1.0f, 0.5f, 0.5f, 1.0f };

    m_originalEntities.push_back(sprite);
    m_expectedData.push_back({
        "TestSprite",
        { 400.0f, 450.0f },
        { 128.0f, 128.0f },
           45.0f, "assets/textures/test/player.png",
            0.0f, 0.0f
        });

    std::cout << "Created Sprite entity: " << sprite.GetName() << '\n';

    // test 2: Simple entity with only transform
    std::cout << "Creating simple entity without complex components..." << '\n';
    Entity simpleEntity = world.CreateEntity("SimpleEntity");
    auto& simpleTransform = simpleEntity.Transform();
    simpleTransform.Position = { 100.0f, 100.0f };
    simpleTransform.Scale = { 50.0f, 50.0f };

    m_originalEntities.push_back(simpleEntity);
    m_expectedData.push_back({
        "SimpleEntity",
        { 100.0f, 100.0f },
        { 50.0f, 50.0f },
        0.0f,
        "",
        0.0f,
        0.0f
        });

    std::cout << "Created Simple entity: " << simpleEntity.GetName() << '\n';
    std::cout << "Total entities created: " << m_originalEntities.size() << '\n';
    std::cout << "CreateTestEntities() completed successfully" << '\n';
}

void SerializationTestScene::SaveScene() {
    std::cout << "SaveScene() started" << '\n';

    World& world = GetWorld();
    std::cout << "Got world reference for saving" << '\n';

    std::cout << "Creating save directory..." << '\n';
    std::filesystem::create_directories("assets/saves");
    std::cout << "Save directory ready" << '\n';

    std::cout << "Calling SceneSerializer::SaveScene..." << '\n';
    const bool success = Serialization::SceneSerializer::SaveScene(
        world,
        "assets/samples/sample-scene.json"
    );
    std::cout << "SceneSerializer::SaveScene returned: " << success << '\n';

    if (success) {
        std::cout << "Scene save operation reported success" << '\n';

        if (std::filesystem::exists("assets/samples/sample-scene.json")) {
            const auto fileSize = std::filesystem::file_size("assets/samples/sample-scene.json");
            std::cout << "File created with size: " << fileSize << " bytes" << '\n';

            std::ifstream testFile("assets/samples/sample-scene.json");
            std::string firstLine;
            std::getline(testFile, firstLine);
            std::cout << "First line of file: " << firstLine.substr(0, 50) << "..." << '\n';
            testFile.close();
        }
        else {
            std::cout << "ERROR: File was not created!" << '\n';
            m_testPassed = false;
            return;
        }
    }
    else {
        std::cout << "Scene save failed!" << '\n';
        m_testPassed = false;
        return;
    }

    std::cout << "SaveScene() completed successfully" << '\n';
}

void SerializationTestScene::LoadScene() {
    std::cout << "LoadScene() started" << '\n';

    World& world = GetWorld();
    std::cout << "Got world reference for loading" << '\n';

    if (!std::filesystem::exists("assets/samples/sample-scene.json")) {
        std::cout << "ERROR: Save file doesn't exist for loading!" << '\n';
        m_testPassed = false;
        return;
    }

    std::cout << "Clearing original entities..." << '\n';
    for (auto& entity : m_originalEntities) {
        world.GetEntityManager().DestroyEntity(entity);
    }
    m_originalEntities.clear();
    std::cout << "Original entities cleared" << '\n';

    std::cout << "Calling SceneSerializer::LoadScene..." << '\n';
    const bool success = Serialization::SceneSerializer::LoadScene(
        world,
        "assets/samples/sample-scene.json"
    );
    std::cout << "SceneSerializer::LoadScene returned: " << success << '\n';

    if (success) {
        std::cout << "Scene load operation reported success" << '\n';

        const auto entityIds = world.GetEntityManager().GetAllEntities();
        std::cout << "Found " << entityIds.size() << " entities after load" << '\n';

        for (const EntityId id : entityIds) {
            Entity newEntity = world.GetEntityManager().GetEntity(id);
            std::cout << "Loaded entity: [" << id << "] " << newEntity.GetName() << '\n';
            m_originalEntities.push_back(newEntity);
        }
        std::cout << "Loaded entities: " << m_originalEntities.size() << '\n';
    }
    else {
        std::cout << "Scene load failed!" << '\n';
        m_testPassed = false;
        return;
    }

    std::cout << "LoadScene() completed successfully" << '\n';
}

void SerializationTestScene::VerifyLoadedEntities() {
    std::cout << "VerifyLoadedEntities() started" << '\n';

    World& world = GetWorld();
    const auto entityIds = world.GetEntityManager().GetAllEntities();

    std::cout << "Verifying " << entityIds.size() << " loaded entities..." << '\n';

    bool allTestsPassed = true;

    for (const EntityId id : entityIds) {
        Entity entity = world.GetEntityManager().GetEntity(id);
        std::string name = entity.GetName();

        std::cout << "\nChecking entity: [" << id << "] " << name << '\n';

        const ExpectedData* expected = nullptr;
        for (auto& exp : m_expectedData) {
            if (exp.name == name) {
                expected = &exp;
                break;
            }
        }

        if (!expected) {
            std::cout << "No expected data for entity" << name << "'" << '\n';
            continue;
        }

        const auto& transform = entity.Transform();
        const bool posMatch = (std::abs(transform.Position.X - expected->position.X) < 0.01f &&
            std::abs(transform.Position.Y - expected->position.Y) < 0.01f);
        const bool scaleMatch = (std::abs(transform.Scale.X - expected->scale.X) < 0.01f &&
            std::abs(transform.Scale.Y - expected->scale.Y) < 0.01f);
        const bool rotMatch = std::abs(transform.Rotation - expected->rotation) < 0.01f;

        std::cout << "  Position: " << (posMatch ? "Success" : "Fail")
            << " (" << transform.Position.X << ", " << transform.Position.Y << ")" << '\n';
        std::cout << "  Scale: " << (scaleMatch ? "Success" : "Fail")
            << " (" << transform.Scale.X << ", " << transform.Scale.Y << ")" << '\n';
        std::cout << "  Rotation: " << (rotMatch ? "Success" : "Fail")
            << " (" << transform.Rotation << " deg)" << '\n';

        if (!posMatch || !scaleMatch || !rotMatch) {
            allTestsPassed = false;
        }

        if (name == "TestSprite") {
            const bool hasSprite = entity.HasComponent<Component::SpriteRenderer>();
            std::cout << "  SpriteRenderer: " << (hasSprite ? "Success" : "Fail") << '\n';
            if (!hasSprite) allTestsPassed = false;
        }
    }

    m_testPassed = allTestsPassed;
    std::cout << "VerifyLoadedEntities() completed" << '\n';
}

void SerializationTestScene::PrintTestResults() {
    std::cout << "\nFINAL TEST RESULTS" << '\n';
    if (m_testPassed) {
        std::cout << "ALL TESTS PASSED - Serialization system is working correctly!" << '\n';
    }
    else {
        std::cout << "SOME TESTS FAILED - Serialization system has issues!" << '\n';
    }
    std::cout << "==========================" << '\n';
}

void SerializationTestScene::OnUpdate() {}

void SerializationTestScene::OnUnload() {
    m_originalEntities.clear();
    m_expectedData.clear();
}