#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <nlohmann/json.hpp>
#include "EntitySerializer.h"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

// TODO: Merge with SceneManager
namespace Serialization {
	class SceneSerializer {
	public:
		static bool SaveScene(World& world, const std::string& filename) {
			try {
				json scene;
				scene["Version"] = "1.0";
				scene["SceneName"] = "TestScene";
				scene["EntityCount"] = 0; // update this!!

				json entities = json::array();

				int entityCount = 0;
				world.ForEachEntity([&](Entity entity) {
					entities.push_back(EntitySerializer::SerializeEntity(entity));
					entityCount++;
					});

				scene["Entities"] = entities;
				scene["EntityCount"] = entityCount;

				std::ofstream file(filename);
				file << scene.dump(4);
				file.close();

				std::cout << "Scene successfully saved to: " << filename << '\n';
				std::cout << " Entities: " << entityCount << '\n';
				return true;

			}
			catch (const std::exception& e) {
				std::cout << "Error saving scene: " << e.what() << '\n';
				return false;
			}
		}

		static bool LoadScene(World& world, const std::string& filename) {
			try {
				// open and parse JSON file
				std::ifstream file(filename);
				if (!file.is_open()) {
					std::cout << " Cannot open file: " << filename << '\n';
					return false;
				}

				json scene = json::parse(file);
				file.close();

				std::cout << "Scene successfully loaded: " << scene["SceneName"] << '\n';
				std::cout << " Version: " << scene["Version"] << '\n';

				// clears existing entities before loading
				world.GetEntityManager().DestroyAllEntities();

				// deserialize each entity from JSON array
				if (scene.contains("Entities")) {
					int loadedCount = 0;
					for (const auto& entityJson : scene["Entities"]) {
						EntitySerializer::DeserializeEntity(world, entityJson);
						loadedCount++;
					}
					std::cout << " Entities loaded: " << loadedCount << '\n';
				}
				return true;

			}
			catch (const json::parse_error& e) {
				std::cout << " JSON parse error: " << e.what() << '\n';
				return false;
			}
			catch (const std::exception& e) {
				std::cout << " Error loading scene: " << e.what() << '\n';
				return false;
			}
		}
	};
}

#endif