#ifndef ENTITYSERIALIZER_H
#define ENTITYSERIALIZER_H

#include "../lib/json.hpp"
#include "Entity.h"
#include "World.h"
#include "Components.h"

#include <typeindex>
#include <unordered_map>
#include <functional>
#include <iostream>

using json = nlohmann::json;

namespace Serialization {
	class EntitySerializer {
	private:
		using SerializationFunction = std::function<json(Entity&)>;
		using DeserializationFunction = std::function<void(Entity&, const json&)>;

		static std::unordered_map<std::string, SerializationFunction> s_serializationRegistry;
		static std::unordered_map<std::string, DeserializationFunction> s_deserializationRegistry;
		static bool s_registryInitialized;

		static void InitializeRegistry() {
			if (s_registryInitialized) return;

			std::cout << "Initializing component registry..." << std::endl;

			RegisterComponent<Component::Transform>("Transform");
			RegisterComponent<Component::SpriteRenderer>("SpriteRenderer");
			RegisterComponent<Component::Rigidbody2D>("RigidBody2D");
			RegisterComponent<Component::Collider2D>("Collider2D");
			RegisterComponent<Component::BoxCollider2D>("BoxCollider2D");
			RegisterComponent<Component::CircleCollider2D>("CircleCollider2D");

			std::cout << "Registered components:" << std::endl;
			for (const auto& [name, func] : s_deserializationRegistry) {
				std::cout << "  - " << name << std::endl;
			}

			s_registryInitialized = true;
		}

		template<typename T>
		static void RegisterComponent(const std::string& typeName) {
			std::cout << "Registering component: " << typeName << std::endl;

			s_serializationRegistry[typeName] = [](Entity& entity) -> json {
				if (auto* component = entity.GetComponent<T>()) {
					return component->Serialize();
				}
				return json{};
				};

			s_deserializationRegistry[typeName] = [typeName](Entity& entity, const json& data) {
				std::cout << "Deserializing " << typeName << "..." << std::endl;

				// Check if component already exists
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
		static json SerializeEntity(Entity entity) {
			InitializeRegistry();

			json entityJson;
			entityJson["EntityId"] = entity.GetId();
			entityJson["Components"] = json::array();

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

		static void DeserializeEntity(World& world, const json& entityJson) {
			InitializeRegistry();

			Entity entity = world.CreateEntity();

			std::cout << "\n=== Deserializing Entity ===" << std::endl;
			std::cout << "Created entity with ID: " << entity.GetId() << std::endl;

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

			// Final verification
			std::cout << "\n=== Final Entity State ===" << std::endl;
			std::cout << "Entity ID: " << entity.GetId() << std::endl;
			std::cout << "Has Transform: " << entity.HasComponent<Component::Transform>() << std::endl;
			std::cout << "Has SpriteRenderer: " << entity.HasComponent<Component::SpriteRenderer>() << std::endl;

			if (auto* transform = entity.GetComponent<Component::Transform>()) {
				std::cout << "Transform data: X=" << transform->X << ", Y=" << transform->Y << std::endl;
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