#include "GameObjectFactoryTest.h"
#include "ecs/GameObjectFactory.h"

void Sandbox::GameObjectFactoryTestScene::OnLoad() {
    // Load player from file
    auto& world = GetWorld();
    Entity player = GameObjectFactory::CreateFromFile(world, "assets/samples/sample-player.json");

    // Load objects from a level file
    std::ifstream file("assets/samples/sample-level.json");
    nlohmann::json j;
    file >> j;

    for (auto& obj : j["objects"]) {
        GameObjectFactory::CreateFromJson(world, obj);
    }

    // Print all entities
    std::cout << "Entities in the world:\n";

    const auto& em = world.GetEntityManager();
    for (const EntityId id : em.GetAllEntities()) {
		Entity e = em.GetEntity(id);
        std::cout << "Entity [" << e.GetId() << "] Name: " << e.GetName() << "\n";

        if (e.HasComponent<Component::Transform>()) {
            const auto t = e.GetComponent<Component::Transform>();
            std::cout << "  Transform: (" << t->Position.X << ", " << t->Position.Y << ")\n";
        }
    }
}
