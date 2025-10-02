#ifndef ENTITYSERIALIZER_H
#define ENTITYSERIALIZER_H

#include "ecs/Components.h"
#include <nlohmann/json.hpp>
#include "ecs/Entity.h"
#include "ecs/World.h"
#include <unordered_map>
#include <functional>
#include <iostream>
#include "systems/Logger.h"

using json = nlohmann::json;

// Utility structs
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector2D, X, Y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, R, G, B, A)

// Transform
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::Transform,
	Position, Rotation, Scale
)

// SpriteRenderer (excluding TextureId, Width, Height, Meta)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::SpriteRenderer,
	TextureId, Width, Height,
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
		static void DeserializeEntity(World& world, const json& entityJson) {
			InitializeRegistry();

			std::string entityName = entityJson.value("Name", "GameObject");

			// create new entity in the world
			Entity entity = world.CreateEntity(entityName);

			LOG_DEBUG("\n=== Deserializing Entity ===");
			LOG_DEBUG("Created entity ID: " << entity.GetId() << ", Name: " << entityName);

			// deserialize all components
			if (entityJson.contains("Components")) {
				for (const auto& componentEntry : entityJson["Components"]) {
					if (componentEntry.contains("Type") && componentEntry.contains("Data")) {
						std::string typeName = componentEntry["Type"];
						auto it = s_deserializationRegistry.find(typeName);

						if (it != s_deserializationRegistry.end()) {
							it->second(entity, componentEntry["Data"]);
						}
						else {
							LOG_WARNING("Unknown component type: " << typeName);
						}
					}
				}
			}

			LOG_DEBUG("=== Finished Deserializing Entity ===");
		}
	};
}

#endif