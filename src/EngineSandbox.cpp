#ifdef _DEBUG
#include <crtdbg.h>
#include "Application.h"
#include "Serialization.h"
#include "Components.h"

#include <iostream>

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    Engine::Application engine;

    // TEST START
    std::cout << "Serialization Test" << std::endl;

    World& testWorld = CREATE_WORLD();
    Entity entity = testWorld.CreateEntity();

    const float expectedY = 42.5f;
    const float expectedX = 13.7f;
    const std::string expectedSprite = "test_sprite.png";

    entity.Transform().X = expectedX;
    entity.Transform().Y = expectedY;
    entity.AddComponent<Component::SpriteRenderer>(expectedSprite);

    std::cout << "Original Entity - Position: (" << expectedX << ", " << expectedY << ")" << std::endl;
    std::cout << "Original Sprite: " << expectedSprite << std::endl;

    bool saveSuccess = Serialization::SceneSerializer::SaveScene(testWorld, "src/quick_test.json");
    std::cout << "Save result: " << (saveSuccess ? "SUCCESS" : "FAILED") << std::endl;

    World& loadWorld = CREATE_WORLD();
    std::cout << "=== BEFORE LOADING ===" << std::endl;
    std::cout << "testWorld entities: " << testWorld.GetEntityManager().GetAllEntities().size() << std::endl;
    std::cout << "loadWorld entities: " << loadWorld.GetEntityManager().GetAllEntities().size() << std::endl;

    bool loadSuccess = Serialization::SceneSerializer::LoadScene(loadWorld, "src/quick_test.json");
    std::cout << "Load result: " << (loadSuccess ? "SUCCESS" : "FAILED") << std::endl;

    std::cout << "- AFTER LOADING -" << std::endl;

    // debug part starts here
    std::cout << "- DEBUG INFO -" << std::endl;

    auto allEntityIds = loadWorld.GetEntityManager().GetAllEntities();
    std::cout << "Number of entities in loadWorld: " << allEntityIds.size() << std::endl;
    std::cout << "Entity IDs: ";
    for (EntityId id : allEntityIds) {
        std::cout << id << " ";
    }
    std::cout << std::endl;

    if (std::find(allEntityIds.begin(), allEntityIds.end(), 1) != allEntityIds.end()) {
        std::cout << "=== Manually checking Entity ID 1 ===" << std::endl;
        Entity entity1 = loadWorld.GetEntityManager().GetEntity(1);
        std::cout << "Entity 1 - Transform: " << (entity1.GetComponent<Component::Transform>() ? "EXISTS" : "NULL") << std::endl;
        std::cout << "Entity 1 - SpriteRenderer: " << (entity1.GetComponent<Component::SpriteRenderer>() ? "EXISTS" : "NULL") << std::endl;

        if (auto* transform = entity1.GetComponent<Component::Transform>()) {
            std::cout << "Entity 1 Transform: X=" << transform->X << ", Y=" << transform->Y << std::endl;
        }
        if (auto* sprite = entity1.GetComponent<Component::SpriteRenderer>()) {
            std::cout << "Entity 1 Sprite: " << sprite->Sprite << std::endl;
        }
    }

    bool dataIntegrityPassed = false;

    loadWorld.ForEachEntity([&dataIntegrityPassed, expectedX, expectedY, expectedSprite](Entity e) {
        std::cout << "Entity ID: " << e.GetId() << std::endl;

        auto* transform = e.GetComponent<Component::Transform>();
        auto* sprite = e.GetComponent<Component::SpriteRenderer>();

        std::cout << "Transform via Entity: " << (transform ? "EXISTS" : "NULL") << std::endl;
        std::cout << "SpriteRenderer via Entity: " << (sprite ? "EXISTS" : "NULL") << std::endl;

        if (transform && sprite) {
            std::cout << "Loaded Entity - Position: (" << transform->X << ", " << transform->Y << ")" << std::endl;
            std::cout << "Loaded Sprite: " << sprite->Sprite << std::endl;

            bool positionXMatch = std::abs(transform->X - expectedX) < 0.001f;
            bool positionYMatch = std::abs(transform->Y - expectedY) < 0.001f;
            bool spriteMatch = (sprite->Sprite == expectedSprite);

            if (positionXMatch && positionYMatch && spriteMatch) {
                std::cout << "Data is correct!" << std::endl;
                dataIntegrityPassed = true;
            }
            else {
                std::cout << "Data is wrong" << std::endl;
                std::cout << "Expected position: (" << expectedX << ", " << expectedY << ")" << std::endl;
                std::cout << "Expected Sprite: " << expectedSprite << std::endl;
                std::cout << "Position X match: " << positionXMatch << " (got " << transform->X << ")" << std::endl;
                std::cout << "Position Y match: " << positionYMatch << " (got " << transform->Y << ")" << std::endl;
                std::cout << "Sprite match: " << spriteMatch << " (got '" << sprite->Sprite << "')" << std::endl;
            }
        }
        else {
            std::cout << "ERROR: Missing components after loading" << std::endl;
            std::cout << "Transform exists: " << (transform != nullptr) << std::endl;
            std::cout << "SpriteRenderer exists: " << (sprite != nullptr) << std::endl;

            // Try HasComponent as well
            std::cout << "HasComponent<Transform>: " << e.HasComponent<Component::Transform>() << std::endl;
            std::cout << "HasComponent<SpriteRenderer>: " << e.HasComponent<Component::SpriteRenderer>() << std::endl;
        }
        });

    std::cout << "=================" << std::endl;

    if (!dataIntegrityPassed) {
        std::cout << "WARNING: Data verification failed" << std::endl;
    }

    std::cout << "Check 'quick_test.json' file for serialized data." << std::endl;
    std::cout << "Test complete" << std::endl;
    std::cout << "Press Enter to continue to engine window..." << std::endl;
    std::cin.get();

    // TEST END

    engine.Run(true);
    return 0;
}
#endif