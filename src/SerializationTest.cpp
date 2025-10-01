#include "SerializationTest.h"
#include "Serialization.h"
#include "ecs/Components.h"
#include "systems/WindowManager.h"
#include <iostream>
#include <filesystem>

using namespace Sandbox;

SerializationTestScene::SerializationTestScene()
    : Scene("SerializationTestScene") {
    CREATE_WINDOW("Serialization Test", 800, 600);
}

void SerializationTestScene::OnLoad() {
    std::cout << "\nSERIALIZATION INTEGRITY TEST" << std::endl;
    std::cout << "Running automated serialization test..." << std::endl;

    RunAutomatedTest();
}

void SerializationTestScene::RunAutomatedTest() {
    std::cout << "\nPHASE 1: Creating Test Entities" << std::endl;
    CreateTestEntities();

    std::cout << "\nPHASE 2: Saving Scene" << std::endl;
    SaveScene();

    if (!m_testPassed) {
        std::cout << "TEST STOPPED: Save failed" << std::endl;
        return;
    }

    std::cout << "\nHASE 3: Loading Scene" << std::endl;
    LoadScene();

    if (!m_testPassed) {
        std::cout << "TEST STOPPED: Load failed" << std::endl;
        return;
    }

    std::cout << "\nPHASE 4: Verifying Loaded Data" << std::endl;
    VerifyLoadedEntities();

    PrintTestResults();

    std::cout << "\nTest complete. Check above for any failures." << std::endl;
}

void SerializationTestScene::CreateTestEntities() {
    std::cout << "CreateTestEntities() started" << std::endl;

    World& world = GetWorld();
    std::cout << "Got world reference" << std::endl;

    // test 1: Basic Transform + SpriteRenderer
    std::cout << "Creating TestSprite entity..." << std::endl;
    Entity sprite = world.CreateEntity("TestSprite");
    std::cout << "Entity created with ID: " << sprite.GetId() << std::endl;

    auto& spriteTransform = sprite.Transform();
    spriteTransform.Position = { 400.0f, 450.0f };
    spriteTransform.Scale = { 128.0f, 128.0f };
    spriteTransform.Rotation = 45.0f;
    std::cout << "Transform set" << std::endl;

    auto& spriteRenderer = sprite.AddComponent<Component::SpriteRenderer>(
        "assets/textures/test/player.png"
    );
    std::cout << "SpriteRenderer component added" << std::endl;
    spriteRenderer.Color = { 1.0f, 0.5f, 0.5f, 1.0f };

    m_originalEntities.push_back(sprite);
    m_expectedData.push_back({
        "TestSprite",
        { 400.0f, 450.0f },
        { 128.0f, 128.0f },
           45.0f, "assets/textures/test/player.png",
            0.0f, 0.0f
        });

    std::cout << "Created Sprite entity: " << sprite.GetName() << std::endl;

    // test 2: Simple entity with only transform
    std::cout << "Creating simple entity without complex components..." << std::endl;
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

    std::cout << "Created Simple entity: " << simpleEntity.GetName() << std::endl;
    std::cout << "Total entities created: " << m_originalEntities.size() << std::endl;
    std::cout << "CreateTestEntities() completed successfully" << std::endl;
}

void SerializationTestScene::SaveScene() {
    std::cout << "SaveScene() started" << std::endl;

    World& world = GetWorld();
    std::cout << "Got world reference for saving" << std::endl;

    std::cout << "Creating save directory..." << std::endl;
    std::filesystem::create_directories("assets/saves");
    std::cout << "Save directory ready" << std::endl;

    std::cout << "Calling SceneSerializer::SaveScene..." << std::endl;
    bool success = Serialization::SceneSerializer::SaveScene(
        world,
        "assets/saves/serialization_test.json"
    );
    std::cout << "SceneSerializer::SaveScene returned: " << success << std::endl;

    if (success) {
        std::cout << "Scene save operation reported success" << std::endl;

        if (std::filesystem::exists("assets/saves/serialization_test.json")) {
            auto fileSize = std::filesystem::file_size("assets/saves/serialization_test.json");
            std::cout << "File created with size: " << fileSize << " bytes" << std::endl;

            std::ifstream testFile("assets/saves/serialization_test.json");
            std::string firstLine;
            std::getline(testFile, firstLine);
            std::cout << "First line of file: " << firstLine.substr(0, 50) << "..." << std::endl;
            testFile.close();
        }
        else {
            std::cout << "ERROR: File was not created!" << std::endl;
            m_testPassed = false;
            return;
        }
    }
    else {
        std::cout << "Scene save failed!" << std::endl;
        m_testPassed = false;
        return;
    }

    std::cout << "SaveScene() completed successfully" << std::endl;
}

void SerializationTestScene::LoadScene() {
    std::cout << "LoadScene() started" << std::endl;

    World& world = GetWorld();
    std::cout << "Got world reference for loading" << std::endl;

    if (!std::filesystem::exists("assets/saves/serialization_test.json")) {
        std::cout << "ERROR: Save file doesn't exist for loading!" << std::endl;
        m_testPassed = false;
        return;
    }

    std::cout << "Clearing original entities..." << std::endl;
    for (auto& entity : m_originalEntities) {
        world.GetEntityManager().DestroyEntity(entity);
    }
    m_originalEntities.clear();
    std::cout << "Original entities cleared" << std::endl;

    std::cout << "Calling SceneSerializer::LoadScene..." << std::endl;
    bool success = Serialization::SceneSerializer::LoadScene(
        world,
        "assets/saves/serialization_test.json"
    );
    std::cout << "SceneSerializer::LoadScene returned: " << success << std::endl;

    if (success) {
        std::cout << "Scene load operation reported success" << std::endl;

        auto entityIds = world.GetEntityManager().GetAllEntities();
        std::cout << "Found " << entityIds.size() << " entities after load" << std::endl;

        for (EntityId id : entityIds) {
            Entity newEntity = world.GetEntityManager().GetEntity(id);
            std::cout << "Loaded entity: [" << id << "] " << newEntity.GetName() << std::endl;
            m_originalEntities.push_back(newEntity);
        }
        std::cout << "Loaded entities: " << m_originalEntities.size() << std::endl;
    }
    else {
        std::cout << "Scene load failed!" << std::endl;
        m_testPassed = false;
        return;
    }

    std::cout << "LoadScene() completed successfully" << std::endl;
}

void SerializationTestScene::VerifyLoadedEntities() {
    std::cout << "VerifyLoadedEntities() started" << std::endl;

    World& world = GetWorld();
    auto entityIds = world.GetEntityManager().GetAllEntities();

    std::cout << "Verifying " << entityIds.size() << " loaded entities..." << std::endl;

    bool allTestsPassed = true;

    for (EntityId id : entityIds) {
        Entity entity = world.GetEntityManager().GetEntity(id);
        std::string name = entity.GetName();

        std::cout << "\nChecking entity: [" << id << "] " << name << std::endl;

        ExpectedData* expected = nullptr;
        for (auto& exp : m_expectedData) {
            if (exp.name == name) {
                expected = &exp;
                break;
            }
        }

        if (!expected) {
            std::cout << "No expected data for entity" << name << "'" << std::endl;
            continue;
        }

        auto& transform = entity.Transform();
        bool posMatch = (std::abs(transform.Position.X - expected->position.X) < 0.01f &&
            std::abs(transform.Position.Y - expected->position.Y) < 0.01f);
        bool scaleMatch = (std::abs(transform.Scale.X - expected->scale.X) < 0.01f &&
            std::abs(transform.Scale.Y - expected->scale.Y) < 0.01f);
        bool rotMatch = std::abs(transform.Rotation - expected->rotation) < 0.01f;

        std::cout << "  Position: " << (posMatch ? "Success" : "Fail")
            << " (" << transform.Position.X << ", " << transform.Position.Y << ")" << std::endl;
        std::cout << "  Scale: " << (scaleMatch ? "Success" : "Fail")
            << " (" << transform.Scale.X << ", " << transform.Scale.Y << ")" << std::endl;
        std::cout << "  Rotation: " << (rotMatch ? "Success" : "Fail")
            << " (" << transform.Rotation << " deg)" << std::endl;

        if (!posMatch || !scaleMatch || !rotMatch) {
            allTestsPassed = false;
        }

        if (name == "TestSprite") {
            bool hasSprite = entity.HasComponent<Component::SpriteRenderer>();
            std::cout << "  SpriteRenderer: " << (hasSprite ? "Success" : "Fail") << std::endl;
            if (!hasSprite) allTestsPassed = false;
        }
    }

    m_testPassed = allTestsPassed;
    std::cout << "VerifyLoadedEntities() completed" << std::endl;
}

void SerializationTestScene::PrintTestResults() {
    std::cout << "\nFINAL TEST RESULTS" << std::endl;
    if (m_testPassed) {
        std::cout << "ALL TESTS PASSED - Serialization system is working correctly!" << std::endl;
    }
    else {
        std::cout << "SOME TESTS FAILED - Serialization system has issues!" << std::endl;
    }
    std::cout << "==========================" << std::endl;
}

void SerializationTestScene::OnUpdate() {}

void SerializationTestScene::OnUnload() {
    m_originalEntities.clear();
    m_expectedData.clear();
}