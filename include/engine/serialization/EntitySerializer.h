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

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <utility>
#include <functional>
#include <string>
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include <helpers/EntityUtils.h>
#include "Serializer.h"

using json = nlohmann::json;

#pragma region nlohmann_adl_helpers
// ADL helpers: call unqualified to_json/from_json with 'using' to enable ADL
	namespace Serialization {
		template<typename U>
		inline void to_json_adl(nlohmann::json& j, const U& v) {
			using nlohmann::to_json;
			to_json(j, v);
		}

		template<typename U>
		inline U from_json_adl(const nlohmann::json& j) {
			U tmp{};
			using nlohmann::from_json;
			// Avoid json.at() type errors when j is not an object
			if (j.is_object()) {
				from_json(j, tmp);
			}
			return tmp;
		}
	}
#pragma endregion

#define REGISTER_COMPONENT_SERIALIZER(VAR, TYPE) \
    namespace { const bool _registered_##VAR = (Serialization::EntitySerializer::RegisterComponent<TYPE>(#TYPE), true); }

// --- Nested Type Serialization Definitions ---

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector2D, X, Y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector3D, X, Y, Z)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector4D, X, Y, Z, W)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Quaternion, X, Y, Z, W)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, R, G, B, A)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Matrix4x4, m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33)

// --- ECS Component Serialization Definitions ---
namespace ECS { 
	namespace Components {
		// Place component-specific NLOHMANN macros inside the component namespace so ADL
		// can locate the generated to_json/from_json overloads for these types.
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Name, Value)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TagMask, Mask)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Active, Enabled)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Lifetime, Time)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Layer, Id)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LocalTransform, Position, Rotation, Scale)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WorldTransform, Matrix, Dirty)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Velocity, Value)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Acceleration, Value)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AngularVelocity, Value)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Rigidbody, Mass, InverseMass, LinearDrag, AngularDrag, Flags)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PhysicsMaterial2D, Friction, Restitution, PositionCorrectPercent)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BoxCollider, HalfExtents, LayerMask)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SphereCollider, Radius, LayerMask)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LinearVelocity2D, Value)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Acceleration2D, Value)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AngularVelocity2D, Value)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Rigidbody2D, Mass, InverseMass, LinearDamping, AngularDamping, GravityScale, Flags)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BoxCollider2D, HalfExtents, Offset, Rotation, LayerMask, Flags)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CircleCollider2D, Radius, Offset, LayerMask, Flags)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SpriteRenderer2D, TextureId, Color, Tiling, Offset)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SpriteFlip2D, FlipX, FlipY)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShapeCircle2D, Radius, Offset, Color, Thickness, Filled)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShapeBox2D, HalfExtents, Offset, Color, Thickness, Filled)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShapeLine2D, A, B, Color, Thickness)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ZIndex2D, ZOrder)
        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Camera3D, UsePerspective, FOV, NearPlane, FarPlane, OrthoSize, AspectRatio, Active)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CameraMatrices, View, Projection, ViewProjection)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ScriptId, Id)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AudioSource, CueId, Volume, Pitch, Loop)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PrefabLink, prefabPath)
	} 
}

namespace Serialization {
	class EntitySerializer {
	public:
		using SerializeFn = std::function<void(const ECS::World&, ECS::Entity, json&)>;
		using DeserializeFn = std::function<void(ECS::World&, ECS::Entity, const json&)>;

		struct ComponentInfo {
			std::string Name;
			SerializeFn Serialize;
			DeserializeFn Deserialize;
		};

		// Registry: TypeId -> ComponentInfo
		static std::unordered_map<TypeId, ComponentInfo>& Registry() {
			static std::unordered_map<TypeId, ComponentInfo> reg;
			return reg;
		}

		// Registration macro
		template<typename T>
		static void RegisterComponent(const char* name) {
			TypeId tid = ECS::TypeIdOf<T>();
			Registry()[tid] = ComponentInfo{
				name,
				// Serialize
				[](const ECS::World& world, ECS::Entity e, json& j) {
					if (world.Has<T>(e)) {
						const T& component = world.Get<T>(e); // Retrieve the component
						Serialization::to_json_adl<T>(j, component);
					} else {
						j = nullptr;
					}
				},
				// Deserialize
				[](ECS::World& world, ECS::Entity e, const json& j) {
					// Guard legacy shapes for specific components where Data may be a bare value
					if constexpr (std::is_same_v<T, ECS::Components::Name>) {
						if (j.is_string()) {
							json jj = json{ {"Value", j.get<std::string>()} };
							if (world.Has<T>(e)) {
								world.template Set<T>(e, Serialization::from_json_adl<T>(jj));
							}
							else {
								world.template Add<T>(e, Serialization::from_json_adl<T>(jj));
							}
							return;
						}
					}
					if (world.Has<T>(e)) {
						world.template Set<T>(e, Serialization::from_json_adl<T>(j));
					}
					else {
						world.template Add<T>(e, Serialization::from_json_adl<T>(j));
					}
				}
			};
		}

		// Serialize a single entity
		static json SerializeEntity(const ECS::World& world, const ECS::Entity e) {
			json entityJson;
			entityJson["EntityId"] = ECS::EntityUtils::Pack(e);
			entityJson["Components"] = json::array();

			for (const auto& [tid, info] : Registry()) {
				json compJson;
				info.Serialize(world, e, compJson);
				if (!compJson.is_null() && !compJson.empty()) {
					entityJson["Components"].push_back({
						{"TypeId", tid},
						{"TypeName", info.Name},
						{"Data", compJson}
						});
				}
			}
			return entityJson;
		}

		// Deserialize a single entity
		static ECS::Entity DeserializeEntity(ECS::World& world, const json& entityJson) {
			ECS::Entity e = world.Create();

			if (!entityJson.is_object()) {
				return e;
			}

			// Prefer robust iteration over components, handling both modern and legacy schemas
			const bool hasComponents = entityJson.contains("Components") && entityJson["Components"].is_array();
			if (!hasComponents) {
				return e;
			}

			// Build a reverse lookup for TypeName -> TypeId for convenience
			std::unordered_map<std::string, TypeId> nameToId;
			for (const auto& [tid, info] : Registry()) {
				nameToId.emplace(info.Name, tid);
			}

			for (const auto& comp : entityJson["Components"]) {
				// Safely read type identification
				TypeId tid = 0;
				std::string typeName;

				if (comp.contains("TypeId") && comp["TypeId"].is_number_unsigned()) {
					// Modern TypeId path
					tid = comp["TypeId"].get<TypeId>();
				}
				else if (comp.contains("TypeName") && comp["TypeName"].is_string()) {
					// Modern TypeName path
					typeName = comp["TypeName"].get<std::string>();
					auto itName = nameToId.find(typeName);
					if (itName != nameToId.end()) tid = itName->second;
				}
				else if (comp.contains("Type") && comp["Type"].is_string()) {
					// Legacy field "Type" (e.g., "Transform", "SpriteRenderer")
					const std::string legacy = comp["Type"].get<std::string>();
					// Map legacy short names to fully-qualified names in our registry
					if (legacy == "Transform") typeName = "ECS::Components::LocalTransform";
					else if (legacy == "SpriteRenderer") typeName = "ECS::Components::SpriteRenderer2D";
					else if (legacy == "Name") typeName = "ECS::Components::Name";
					else if (legacy == "ZIndex2D") typeName = "ECS::Components::ZIndex2D";
					// Resolve mapped name if available
					if (!typeName.empty()) {
						auto itName = nameToId.find(typeName);
						if (itName != nameToId.end()) tid = itName->second;
					}
				}

				// If we still couldn't resolve tid, skip this component gracefully
				auto it = Registry().find(tid);
				if (it == Registry().end()) {
					continue;
				}

                // Safely extract component data; default to empty object
                json data = json::object();
                if (comp.contains("Data")) {
                    data = comp["Data"];
                }

                // Coerce non-object data into expected object shapes to avoid json.at() errors
                const std::string compTypeName = it->second.Name;
                if (!data.is_object()) {
                    if (compTypeName == "ECS::Components::Name" && data.is_string()) {
                        data = json{ {"Value", data.get<std::string>()} };
                    }
                    else if (compTypeName == "ECS::Components::TagMask" && data.is_number_unsigned()) {
                        data = json{ {"Mask", data.get<unsigned int>()} };
                    }
                    else if (compTypeName == "ECS::Components::Active" && data.is_boolean()) {
                        data = json{ {"Enabled", data.get<bool>()} };
                    }
                    else if (compTypeName == "ECS::Components::Layer" && data.is_number()) {
                        data = json{ {"Id", data.get<int>()} };
                    }
                    else if (compTypeName == "ECS::Components::ScriptId" && data.is_number_unsigned()) {
                        data = json{ {"Id", data.get<unsigned int>()} };
                    }
                    else if (compTypeName == "ECS::Components::ZIndex2D" && data.is_number()) {
                        data = json{ {"ZOrder", data.get<int>()} };
                    }
                    else if (compTypeName == "ECS::Components::PrefabLink" && data.is_string()) {
                        data = json{ {"prefabPath", data.get<std::string>()} };
                    }
                    else {
                        // Fallback to empty object if type doesn't match expected shape
                        data = json::object();
                    }
                }

                // Ensure LocalTransform fields exist regardless of legacy or modern naming
                if (compTypeName == "ECS::Components::LocalTransform") {
                    // Create defaults if missing
                    if (!data.contains("Position")) data["Position"] = json{{"X",0.0f},{"Y",0.0f},{"Z",0.0f}};
                    if (!data.contains("Rotation")) data["Rotation"] = json{{"X",0.0f},{"Y",0.0f},{"Z",0.0f},{"W",1.0f}};
                    if (!data.contains("Scale")) data["Scale"] = json{{"X",1.0f},{"Y",1.0f},{"Z",1.0f}};
                }

                // Ensure vector-valued components have a proper Value object
                auto ensure_vec2 = [&](json& v){
                    if (!v.is_object()) v = json::object();
                    if (!v.contains("X")) v["X"] = 0.0f;
                    if (!v.contains("Y")) v["Y"] = 0.0f;
                };
                auto ensure_vec3 = [&](json& v){
                    if (!v.is_object()) v = json::object();
                    if (!v.contains("X")) v["X"] = 0.0f;
                    if (!v.contains("Y")) v["Y"] = 0.0f;
                    if (!v.contains("Z")) v["Z"] = 0.0f;
                };
                auto ensure_quat = [&](json& v){
                    if (!v.is_object()) v = json::object();
                    if (!v.contains("X")) v["X"] = 0.0f;
                    if (!v.contains("Y")) v["Y"] = 0.0f;
                    if (!v.contains("Z")) v["Z"] = 0.0f;
                    if (!v.contains("W")) v["W"] = 1.0f;
                };

                // 2D vector wrappers
                if (compTypeName == "ECS::Components::LinearVelocity2D"
                    || compTypeName == "ECS::Components::Acceleration2D"
                    || compTypeName == "ECS::Components::AngularVelocity2D") {
                    json& v = data["Value"]; // creates if missing
                    ensure_vec2(v);
                }

                // 3D vector wrappers
                if (compTypeName == "ECS::Components::Velocity"
                    || compTypeName == "ECS::Components::Acceleration"
                    || compTypeName == "ECS::Components::AngularVelocity") {
                    json& v = data["Value"]; // creates if missing
                    ensure_vec3(v);
                }

				// Delegate to registered deserializer with safety guard
				try {
					it->second.Deserialize(world, e, data);
				}
				catch (const nlohmann::json::type_error& te) {
					LOG_ERROR("JSON type error in component '" << compTypeName << "': " << te.what());
					// Skip this component on type mismatch to avoid hard crash
					continue;
				}
				catch (const std::exception& ex) {
					LOG_ERROR("Error deserializing component '" << compTypeName << "': " << ex.what());
					continue;
				}
			}

			return e;
		}

		/**
         * @brief Save an entity as a prefab file (.prefab)
         * @param filename Path to write (must end with .prefab)
         * @param world World containing the entity
         * @param e Entity to serialize
         * @return true on success
         */
        static bool SavePrefab(const std::string& filename, const ECS::World& world, const ECS::Entity e) {
            json j = SerializeEntity(world, e);
            return Serializer::SaveJson(filename, "prefab", j);
        }

        /**
         * @brief Load a prefab file and instantiate it into the provided world
         * @param filename Path to read (must end with .prefab)
         * @param world World to instantiate into
         * @param outEntity Optional out param to receive created entity
         * @return true on success
         */
        static bool LoadPrefab(const std::string& filename, ECS::World& world, ECS::Entity* outEntity = nullptr) {
            json j;
            if (!Serializer::LoadJson(filename, "prefab", j))
				return false;
            
			ECS::Entity e = DeserializeEntity(world, j);
			if (outEntity)
				*outEntity = e;
            
				return true;
        }
	};

	// --- Register all component serializers ---

	// --- Register all component serializers ---

	REGISTER_COMPONENT_SERIALIZER(Name, ECS::Components::Name)
	REGISTER_COMPONENT_SERIALIZER(TagMask, ECS::Components::TagMask)
	REGISTER_COMPONENT_SERIALIZER(Active, ECS::Components::Active)
	REGISTER_COMPONENT_SERIALIZER(PrefabLink, ECS::Components::PrefabLink)
	REGISTER_COMPONENT_SERIALIZER(Lifetime, ECS::Components::Lifetime)
	REGISTER_COMPONENT_SERIALIZER(Layer, ECS::Components::Layer)
	REGISTER_COMPONENT_SERIALIZER(LocalTransform, ECS::Components::LocalTransform)
	REGISTER_COMPONENT_SERIALIZER(WorldTransform, ECS::Components::WorldTransform)
	REGISTER_COMPONENT_SERIALIZER(Velocity, ECS::Components::Velocity)
	REGISTER_COMPONENT_SERIALIZER(Acceleration, ECS::Components::Acceleration)
	REGISTER_COMPONENT_SERIALIZER(AngularVelocity, ECS::Components::AngularVelocity)
	REGISTER_COMPONENT_SERIALIZER(Rigidbody, ECS::Components::Rigidbody)
	REGISTER_COMPONENT_SERIALIZER(PhysicsMaterial2D, ECS::Components::PhysicsMaterial2D)
	REGISTER_COMPONENT_SERIALIZER(BoxCollider, ECS::Components::BoxCollider)
	REGISTER_COMPONENT_SERIALIZER(SphereCollider, ECS::Components::SphereCollider)
	REGISTER_COMPONENT_SERIALIZER(LinearVelocity2D, ECS::Components::LinearVelocity2D)
	REGISTER_COMPONENT_SERIALIZER(Acceleration2D, ECS::Components::Acceleration2D)
	REGISTER_COMPONENT_SERIALIZER(AngularVelocity2D, ECS::Components::AngularVelocity2D)
	REGISTER_COMPONENT_SERIALIZER(Rigidbody2D, ECS::Components::Rigidbody2D)
	REGISTER_COMPONENT_SERIALIZER(BoxCollider2D, ECS::Components::BoxCollider2D)
	REGISTER_COMPONENT_SERIALIZER(CircleCollider2D, ECS::Components::CircleCollider2D)
	REGISTER_COMPONENT_SERIALIZER(SpriteRenderer2D, ECS::Components::SpriteRenderer2D)
	REGISTER_COMPONENT_SERIALIZER(SpriteFlip2D, ECS::Components::SpriteFlip2D)
	REGISTER_COMPONENT_SERIALIZER(ShapeCircle2D, ECS::Components::ShapeCircle2D)
	REGISTER_COMPONENT_SERIALIZER(ShapeBox2D, ECS::Components::ShapeBox2D)
	REGISTER_COMPONENT_SERIALIZER(ShapeLine2D, ECS::Components::ShapeLine2D)
	// ShapePolygon2D is a template, therefore it needs to be rewritten
	// REGISTER_COMPONENT_SERIALIZER(ShapePolygon2D, ECS::Components::ShapePolygon2D)
	REGISTER_COMPONENT_SERIALIZER(ZIndex2D, ECS::Components::ZIndex2D)
	REGISTER_COMPONENT_SERIALIZER(Camera3D, ECS::Components::Camera3D)
	REGISTER_COMPONENT_SERIALIZER(CameraMatrices, ECS::Components::CameraMatrices)
	REGISTER_COMPONENT_SERIALIZER(ScriptId, ECS::Components::ScriptId)
	REGISTER_COMPONENT_SERIALIZER(AudioSource, ECS::Components::AudioSource)
}

#endif
