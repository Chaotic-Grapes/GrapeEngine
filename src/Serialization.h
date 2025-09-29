#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include "../include/nlohmann/json.hpp"
#include "EntitySerializer.h"

#include <fstream>
#include <iostream>

using json = nlohmann::json;

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

				std::cout << "Scene successfully saved to: " << filename << std::endl;
				std::cout << " Entities: " << entityCount << std::endl;
				return true;

			}
			catch (const std::exception& e) {
				std::cout << "Error saving scene: " << e.what() << std::endl;
				return false;
			}
		}

		static bool LoadScene(World& world, const std::string& filename) {
			try {
				std::ifstream file(filename);
				if (!file.is_open()) {
					std::cout << " Cannot open file: " << filename << std::endl;
					return false;
				}

				json scene = json::parse(file);
				file.close();

				std::cout << "Scene successfully loaded: " << scene["SceneName"] << std::endl;
				std::cout << " Version: " << scene["Version"] << std::endl;

				world.GetEntityManager().DestroyAllEntities();

				if (scene.contains("Entities")) {
					int loadedCount = 0;
					for (const auto& entityJson : scene["Entities"]) {
						EntitySerializer::DeserializeEntity(world, entityJson);
						loadedCount++;
					}
					std::cout << " Entities loaded: " << loadedCount << std::endl;
				}
				return true;

			}
			catch (const json::parse_error& e) {
				std::cout << " JSON parse error: " << e.what() << std::endl;
				return false;
			}
			catch (const std::exception& e) {
				std::cout << " Error loading scene: " << e.what() << std::endl;
				return false;
			}
		}
	};
}

#endif