#ifndef GAMEOBJECTFACTORY_H
#define GAMEOBJECTFACTORY_H

#include <nlohmann/json.hpp>
#include <fstream>
#include "ecs/World.h"
#include "ecs/Entity.h"
#include "ecs/Components.h"

class GameObjectFactory {
public:
    static Entity CreateFromFile(World& world, const std::string& filename) {
        std::ifstream file(filename);
        nlohmann::json j;
        file >> j;
        return CreateFromJson(world, j);
    }

    static Entity CreateFromJson(World& world, const nlohmann::json& j) {
        Entity entity = world.CreateEntity(j["name"].get<std::string>());
        auto& comps = j["components"];

		// For now, transform component only
        if (comps.contains("Transform")) {
            float x = comps["Transform"]["X"].get<float>();
            float y = comps["Transform"]["Y"].get<float>();
            entity.AddComponent<Component::Transform>(x, y);
        }

        return entity;
    }
};
#endif
