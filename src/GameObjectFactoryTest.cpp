#include "GameObjectFactoryTest.h"
#include "ecs/GameObjectFactory.h"

void Sandbox::GameObjectFactoryTestScene::OnLoad() {
    // Load player from file
    auto& world = GetWorld();

    // Player
    Entity player = GameObjectFactory::CreateFromFile(world, "assets/samples/sample-player.json");

    // Load objects from a level file
    // Enemies predefined in level
    std::ifstream file("assets/samples/sample-level.json");
    nlohmann::json j;
    file >> j;

    for (auto& obj : j["objects"]) {
        GameObjectFactory::CreateFromJson(world, obj);
    }

    // Enemy prefab
    // TODO: Dedicated prefab class?
    Entity enemyPrefab = GameObjectFactory::CreateFromFile(world, "assets/samples/sample-enemy-prefab.prefab");
    std::vector<Entity> enemyClones;
    for (int i = 0; i < 5; ++i) {
        Entity clone = enemyPrefab.Clone();
        auto t = clone.GetComponent<Component::Transform>();
        t->Position.X = 50.0f + static_cast<float>(i) * 40.0f;
        t->Position.Y = 300.0f;
        enemyClones.push_back(clone);
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

        // Messy
        if (e.HasComponent<Component::ShapeRenderer2D>()) {
            auto s = e.GetComponent<Component::ShapeRenderer2D>();
            std::string type = (s->Type == Component::ShapeRenderer2D::ShapeType::Circle ? "Circle" :
                s->Type == Component::ShapeRenderer2D::ShapeType::Rectangle ? "Rectangle" : "Polygon");
            std::cout << "  Shape: " << type << "\n";
        }
    }
}
