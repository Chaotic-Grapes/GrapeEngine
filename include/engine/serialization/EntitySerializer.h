/**
 * @file    EntitySerializer.h
 * @author  k.danielneozuofeng
 * @date    26/09/2025
 * @brief   Entity serialization and deserialization to JSON
 *
 * This header defines the EntitySerializer class, which manages
 * the conversion of individual entities (and their components)
 * to and from JSON format. It maintains a registry of supported
 * component types and their serialization/deserialization logic,
 * allowing entities to be reconstructed within a World.
 */

#ifndef ENTITYSERIALIZER_H
#define ENTITYSERIALIZER_H

#include "ecs/Components.h"
#include <nlohmann/json.hpp>
#include "ecs/Entity.h"
#include "ecs/World.h"
#include <unordered_map>
#include <functional>
#include <iostream>
#include "core/Logger.h"

using json = nlohmann::json;

// Utility structs
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector2D, X, Y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, R, G, B, A)

// Transform
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::Transform,
	Position, Rotation, Scale, ParentId
)

// SpriteRenderer (excluding TextureId, Meta)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::SpriteRenderer,
	Width, Height,
	Color, FlipX, FlipY,
	SortingOrder, SortingLayerName,
	TexturePath, Sprite
)

// ShapeRenderer2D
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::ShapeRenderer2D,
	Type, FillColor, OutlineColor, OutlineThickness,
	Size, Radius, Points, Closed
)

// Rigidbody2D
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::Rigidbody2D,
	LinearVelocity, Inertia, AngularVelocity, AngularDamping,
	LinearDamping, CenterOfMass, Mass, GravityScale,
	FreezeRotation, BodyType
)

// Collider2D
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::Collider2D,
	IsTrigger, Offset, Layer
)

// BoxCollider2D (inherits Collider2D)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::BoxCollider2D,
	IsTrigger, Offset, Layer, Size
)

// CircleCollider2D (inherits Collider2D)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::CircleCollider2D,
	IsTrigger, Offset, Layer, Radius
)

// LineRenderer
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::LineRenderer,
	Start, End, Thickness, Color
)

// PrefabLink
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::PrefabLink, prefabPath)

namespace Serialization {

	// this class handles serialization and deserialziation of entities to/from JSON format.
	class EntitySerializer {
	private:

		using SerializationFunction = std::function<json(Entity&)>;
		using DeserializationFunction = std::function<void(Entity&, const json&)>;

		// registry maps component types names to their serialization functions
		inline static std::unordered_map<std::string, SerializationFunction> s_serializationRegistry;
		inline static std::unordered_map<std::string, DeserializationFunction> s_deserializationRegistry;
		inline static bool s_registryInitialized = false;

		// initializes the component registry with all supported component types.
		static void InitializeRegistry() {
			if (s_registryInitialized) return;

			LOG_DEBUG("Initializing component registry...");

			RegisterComponent<Component::Transform>("Transform");
			RegisterComponent<Component::SpriteRenderer>("SpriteRenderer");
			RegisterComponent<Component::Rigidbody2D>("Rigidbody2D");
			RegisterComponent<Component::CircleCollider2D>("CircleCollider2D");
			RegisterComponent<Component::BoxCollider2D>("BoxCollider2D");
			RegisterComponent<Component::ShapeRenderer2D>("ShapeRenderer2D");
			RegisterComponent<Component::LineRenderer>("LineRenderer");
			RegisterComponent<Component::PrefabLink>("PrefabLink");

			LOG_DEBUG("Registered components:");
			for (const auto& [name, _] : s_deserializationRegistry) {
				LOG_DEBUG("  - " << name);
			}

			LOG_DEBUG("EntitySerializer::InitializeRegistry() called");
			LOG_DEBUG("Initializing component registry...");

			s_registryInitialized = true;
		}

		// registers a component type for serialization/deserialization
		template<typename T>
		static void RegisterComponent(const std::string& typeName) {
			// register serialization function
			s_serializationRegistry[typeName] = [](Entity& entity) -> json {
				if (auto* component = entity.GetComponent<T>()) {
					json j;
					to_json(j, *component);
					return j;
				}
				return json{}; // returns empty JSON
			};

			// register deserialization function
			s_deserializationRegistry[typeName] = [typeName](Entity& entity, const json& data) {
				if (!entity.HasComponent<T>()) {
					LOG_DEBUG("Adding " << typeName << " component to entity ID " << entity.GetId());
					entity.AddComponent<T>();
				}
				if (auto* component = entity.GetComponent<T>()) {
					from_json(data, *component);
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
					entityJson["Components"].push_back({
						{"Type", typeName},
						{"Data", componentData}
					});
				}
			}
			return entityJson;
		}

		// deserializes an entity from JSON and adds it to the world
		static Entity DeserializeEntity(World& world, const json& entityJson) {
			InitializeRegistry();

			std::string entityName = entityJson.value("Name", "GameObject");

			// create new entity in the world
			Entity entity = world.CreateEntity(entityName);

			LOG_DEBUG("=== Deserializing Entity ===");
			LOG_DEBUG("Created entity ID: " << entity.GetId() << ", Name: " << entityName);

			// deserialize all components
			if (entityJson.contains("Components")) {
				for (const auto& componentEntry : entityJson["Components"]) {
					if (componentEntry.contains("Type") && componentEntry.contains("Data")) {
						std::string typeName = componentEntry["Type"];
							auto it = s_deserializationRegistry.find(typeName);

							// Special-case Transform to tolerate missing keys (e.g., ParentId)
							if (typeName == "Transform") {
								if (!entity.HasComponent<Component::Transform>()) {
									entity.AddComponent<Component::Transform>();
								}
								if (auto* t = entity.GetComponent<Component::Transform>()) {
									const auto& d = componentEntry["Data"];
									// Position
									if (d.contains("Position")) {
										const auto& p = d["Position"];
										t->Position.X = p.value("X", t->Position.X);
										t->Position.Y = p.value("Y", t->Position.Y);
									}
									// Rotation
									t->Rotation = d.value("Rotation", t->Rotation);
									// Scale
									if (d.contains("Scale")) {
										const auto& s = d["Scale"];
										t->Scale.X = s.value("X", t->Scale.X);
										t->Scale.Y = s.value("Y", t->Scale.Y);
									}
									// ParentId (optional in prefab JSON)
									t->ParentId = d.value("ParentId", t->ParentId);
								}
								continue;
							}

						// A little messy but needed
						// SpriteRenderer has a custom constructor that takes a texture path
						// and is needed to initialize the component properly.
						if (it != s_deserializationRegistry.end()) {
								if (typeName == "SpriteRenderer") {
								const auto& data = componentEntry["Data"];
								if (!entity.HasComponent<Component::SpriteRenderer>()) {
									std::string path = data.value("TexturePath", "");
									entity.AddComponent<Component::SpriteRenderer>(path);
								}
								if (auto* component = entity.GetComponent<Component::SpriteRenderer>()) {
									from_json(data, *component);
								}
							} else {
								it->second(entity, componentEntry["Data"]);
							}
						}
						else {
							LOG_WARNING("Unknown component type: " << typeName);
						}
					}
				}
			}

			LOG_DEBUG("=== Finished Deserializing Entity ===");
			return entity;
		}
	};
}

#endif