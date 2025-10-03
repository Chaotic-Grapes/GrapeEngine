#ifndef ENTITYSERIALIZER_H
#define ENTITYSERIALIZER_H

#include <nlohmann/json.hpp>
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include <unordered_map>
#include <functional>
#include <iostream>

using json = nlohmann::json;

namespace Serialization {

	// this class handles serialization and deserialziation of entities to/from JSON format.
	class EntitySerializer {
	private:

		using SerializationFunction = std::function<json(Entity&)>;
		using DeserializationFunction = std::function<void(Entity&, const json&)>;

		// registry maps component types names to their serialization functions
		static std::unordered_map<std::string, SerializationFunction> s_serializationRegistry;
		static std::unordered_map<std::string, DeserializationFunction> s_deserializationRegistry;
		static bool s_registryInitialized;

		// initializes the component registry with all supported component types.
		static void InitializeRegistry() {
			if (s_registryInitialized) return;

			std::cout << "Initializing component registry..." << std::endl;

			RegisterComponent<Component::Transform>("Transform");
			RegisterComponent<Component::SpriteRenderer>("SpriteRenderer");
			RegisterComponent<Component::Rigidbody2D>("Rigidbody2D");
			RegisterComponent<Component::CircleCollider2D>("CircleCollider2D");
			RegisterComponent<Component::BoxCollider2D>("BoxCollider2D");
			RegisterComponent<Component::ShapeRenderer2D>("ShapeRenderer2D");
			RegisterComponent<Component::LineRenderer>("LineRenderer");

			std::cout << "Registered components:" << std::endl;
			for (const auto& [name, func] : s_deserializationRegistry) {
				std::cout << "  - " << name << std::endl;
			}

			std::cout << "EntitySerializer::InitializeRegistry() called" << std::endl;
			std::cout << "Initializing component registry..." << std::endl;

			RegisterComponent<Component::Transform>("Transform");

			std::cout << "Registry initialization complete" << std::endl;

			s_registryInitialized = true;
		}

		// registers a component type for serialization/deserialization
		template<typename T>
		static void RegisterComponent(const std::string& typeName) {
			std::cout << "Registering component: " << typeName << std::endl;

			// register serialization function
			s_serializationRegistry[typeName] = [](Entity& entity) -> json {
				if (auto* component = entity.GetComponent<T>()) {
					return component->Serialize();
				}
				return json{};	// returns empty JSON
				};

			// register deserialization function
			s_deserializationRegistry[typeName] = [typeName](Entity& entity, const json& data) {
				std::cout << "Deserializing " << typeName << "..." << std::endl;

				bool hasComponent = entity.HasComponent<T>();
				std::cout << "  Entity already has " << typeName << ": " << (hasComponent ? "YES" : "NO") << std::endl;

				if (!hasComponent) {
					std::cout << "  Adding " << typeName << " component..." << std::endl;
					try {
						entity.AddComponent<T>();
						std::cout << "  Successfully added " << typeName << std::endl;
					}
					catch (const std::exception& e) {
						std::cout << "  ERROR adding " << typeName << ": " << e.what() << std::endl;
						return;
					}
				}

				// deserialize the component data
				if (auto* component = entity.GetComponent<T>()) {
					std::cout << "  Got component pointer, deserializing data..." << std::endl;
					try {
						component->Deserialize(data);
						std::cout << "  Successfully deserialized " << typeName << std::endl;
					}
					catch (const std::exception& e) {
						std::cout << "  ERROR deserializing " << typeName << ": " << e.what() << std::endl;
					}
				}
				else {
					std::cout << "  ERROR: Failed to get " << typeName << " component after adding!" << std::endl;
				}
				};
		}

	public:
		// serializes an entity to JSON format including all its components.
		static json SerializeEntity(Entity entity) {
			InitializeRegistry();

			json entityJson;
			entityJson["EntityId"] = entity.GetId();
			entityJson["Components"] = json::array();
			entityJson["Name"] = entity.GetName();

			// serialize each component that the entity has
			for (const auto& [typeName, serializeFunc] : s_serializationRegistry) {
				json componentData = serializeFunc(entity);
				if (!componentData.empty()) {
					json componentEntry;
					componentEntry["Type"] = typeName;
					componentEntry["Data"] = componentData;
					entityJson["Components"].push_back(componentEntry);
				}
			}
			return entityJson;
		}

		// deserializes an entity from JSON and adds it to the world
		static void DeserializeEntity(World& world, const json& entityJson) {
			InitializeRegistry();

			std::string entityName = entityJson.value("Name", "GameObject");

			// create new entity in the world
			Entity entity = world.CreateEntity(entityName);

			std::cout << "\n=== Deserializing Entity ===" << std::endl;
			std::cout << "Created entity with ID: " << entity.GetId() << std::endl;
			std::cout << "Entity name: " << entityName << std::endl;

			// deserialize all components
			if (entityJson.contains("Components")) {
				std::cout << "Found " << entityJson["Components"].size() << " components to deserialize" << std::endl;

				for (const auto& componentEntry : entityJson["Components"]) {
					if (componentEntry.contains("Type") && componentEntry.contains("Data")) {
						std::string typeName = componentEntry["Type"];
						std::cout << "\nProcessing component: " << typeName << std::endl;

						auto it = s_deserializationRegistry.find(typeName);
						if (it != s_deserializationRegistry.end()) {
							it->second(entity, componentEntry["Data"]);
						}
						else {
							std::cout << "Warning: Unknown component type: " << typeName << std::endl;
							std::cout << "Available types:" << std::endl;
							for (const auto& [name, func] : s_deserializationRegistry) {
								std::cout << "  - " << name << std::endl;
							}
						}
					}
				}
			}

			std::cout << "\n=== Final Entity State ===" << std::endl;
			std::cout << "Entity ID: " << entity.GetId() << std::endl;
			std::cout << "Entity Name: " << entity.GetName() << std::endl;
			std::cout << "Has Transform: " << entity.HasComponent<Component::Transform>() << std::endl;
			std::cout << "Has SpriteRenderer: " << entity.HasComponent<Component::SpriteRenderer>() << std::endl;

			if (auto* transform = entity.GetComponent<Component::Transform>()) {
				std::cout << "Transform data: X=" << transform->Position.X << ", Y=" << transform->Position.Y << std::endl;
			}
			if (auto* sprite = entity.GetComponent<Component::SpriteRenderer>()) {
				std::cout << "Sprite data: " << sprite->Sprite << std::endl;
			}
			std::cout << "=========================" << std::endl;
		}
	};

	std::unordered_map<std::string, EntitySerializer::SerializationFunction> EntitySerializer::s_serializationRegistry;
	std::unordered_map<std::string, EntitySerializer::DeserializationFunction> EntitySerializer::s_deserializationRegistry;
	bool EntitySerializer::s_registryInitialized = false;
}

#endif