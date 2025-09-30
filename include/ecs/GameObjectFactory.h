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

        if (comps.contains("Transform")) {
            const float x = comps["Transform"]["x"].get<float>(),
        				y = comps["Transform"]["y"].get<float>();

            const auto tr = entity.GetComponent<Component::Transform>();
            tr->Position.X = x;
            tr->Position.Y = y;
        }

        if (comps.contains("ShapeRenderer2D")) {
            const auto& s = comps["ShapeRenderer2D"];
            const std::string type = s["type"].get<std::string>();

            // TODO: Default to hex value
            Color fill{ 1.f, 1.f, 1.f, 1.f };
            if (s.contains("fillColor") && s["fillColor"].is_array() && s["fillColor"].size() == 4) {
                fill.R = static_cast<HexValue>(s["fillColor"][0].get<float>() * 255.f);
                fill.G = static_cast<HexValue>(s["fillColor"][1].get<float>() * 255.f);
                fill.B = static_cast<HexValue>(s["fillColor"][2].get<float>() * 255.f);
                fill.A = static_cast<HexValue>(s["fillColor"][3].get<float>() * 255.f);
            }

            if (type == "Rectangle") {
                const auto size = Vector2D(s["size"][0].get<float>(), s["size"][1].get<float>());
                entity.AddComponent<Component::ShapeRenderer2D>(
                    Component::ShapeRenderer2D::Rectangle(size, fill)
                );
            }
            else if (type == "Circle") {
                const float radius = s["radius"].get<float>();
                entity.AddComponent<Component::ShapeRenderer2D>(
                    Component::ShapeRenderer2D::Circle(radius, fill)
                );
            }
            else if (type == "Polygon") {
                std::vector<Vector2D> points;
                for (auto& p : s["points"])
                    points.emplace_back(p[0].get<float>(), p[1].get<float>());
                const bool closed = s.value("closed", true);
                entity.AddComponent<Component::ShapeRenderer2D>(
                    Component::ShapeRenderer2D::Polygon(points, fill, closed)
                );
            }
        }

        return entity;
    }
};
#endif
