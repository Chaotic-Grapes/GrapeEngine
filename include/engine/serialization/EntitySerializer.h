/* Start Header *****************************************************************/
/*!
\file    EntitySerializer.h
\authors Daniel Neo Zuo Feng Kay (90%), Muhammad Nur Fadzly Bin Zulkifli (10%)
\par     k.danielneozuofeng@digipen.edu, muhammadnurfadzly.b@digipen.edu
\date    26th September 2025
\brief
This header defines the EntitySerializer class, which manages
the conversion of individual entities (and their components)
to and from JSON format. It maintains a registry of supported
component types and their serialization/deserialization logic,
allowing entities to be reconstructed within a World.

Originally written by Daniel Neo Zuo Feng Kay, with contributions such as fixes
and improvements by Muhammad Nur Fadzly Bin Zulkifli.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ENTITYSERIALIZER_H
#define ENTITYSERIALIZER_H

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <utility>
#include <functional>
#include <string>
#include <cstring>
#include <iostream>
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include <helpers/EntityUtils.h>
#include "Serializer.h"
#include <string.h>

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
		U tmp;
		using nlohmann::from_json;
		from_json(j, tmp);
		return tmp;
	}
}
#pragma endregion

#define REGISTER_COMPONENT_SERIALIZER(VAR, TYPE, NAME) \
    namespace { const bool _registered_##VAR = (Serialization::EntitySerializer::RegisterComponent<TYPE>(NAME), true); }

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
		
		// Custom serialization for Name component (char array needs special handling)
		inline void to_json(nlohmann::json& j, const Name& n) {
			j = nlohmann::json{ {"Value", std::string(n.Value)} }; // convert char array to std::string
		}
		
		inline void from_json(const nlohmann::json& j, Name& n) {
			std::string value = j.at("Value").get<std::string>(); // get as std::string
			strncpy_s(n.Value, value.c_str(), sizeof(n.Value) - 1); // copy to char array
			n.Value[sizeof(n.Value) - 1] = '\0'; // null terminator
		}
		
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TagMask, Mask)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Active, Enabled)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Lifetime, Time)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Layer, Id)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Rotator, RotationSpeed, RotationOffset)
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
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SpriteSheetAnimation2D, TextureId, FrameWidth, FrameHeight, SheetWidth, SheetHeight, StartFrame, FrameCount, FramesPerSecond, Loop, Playing)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AnimationState2D, CurrentFrame, TimeAccumulator, Finished)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShapeCircle2D, Radius, Offset, Color, Thickness, Filled)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShapeBox2D, HalfExtents, Offset, Color, Thickness, Filled)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShapeLine2D, A, B, Color, Thickness)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ZIndex2D, ZOrder)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Camera3D, UsePerspective, FOV, NearPlane, FarPlane, OrthoSize, AspectRatio, Active)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CameraEditor3D, UsePerspective, FOV, NearPlane, FarPlane, OrthoSize, AspectRatio)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CameraMatrices, View, Projection, ViewProjection)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PrefabLink, prefabPath)
		
		// Custom serialization for Light2D enum
		inline void to_json(nlohmann::json& j, const Light2D& light) {
			j = nlohmann::json{
				{"LightType", static_cast<uint8_t>(light.LightType)},
				{"Position", light.Position},
				{"Direction", light.Direction},
				{"Color", light.Color},
				{"Intensity", light.Intensity},
				{"Range", light.Range},
				{"CastsShadows", light.CastsShadows}
			};
		}
		
		inline void from_json(const nlohmann::json& j, Light2D& light) {
			light.LightType = static_cast<Light2D::Type>(j.at("LightType").get<uint8_t>());
			light.Position = j.at("Position").get<Vector3D>();
			light.Direction = j.at("Direction").get<Vector3D>();
			light.Color = j.at("Color").get<::Color>();
			light.Intensity = j.at("Intensity").get<float>();
			light.Range = j.at("Range").get<float>();
			light.CastsShadows = j.at("CastsShadows").get<bool>();
		}
		
		// Custom serialization for Text component (char arrays need special handling)
		inline void to_json(nlohmann::json& j, const Text& text) {
			j = nlohmann::json{
				{"Content", std::string(text.Content)},
				{"FontPath", std::string(text.FontPath)},
				{"PixelSize", text.PixelSize},
				{"Color", text.Color},
				{"Anchor", static_cast<uint8_t>(text.Anchor)}
			};
		}
		
		inline void from_json(const nlohmann::json& j, Text& text) {
			std::string content = j.at("Content").get<std::string>();
			strncpy_s(text.Content, content.c_str(), sizeof(text.Content) - 1);
			text.Content[sizeof(text.Content) - 1] = '\0';
			
			std::string fontPath = j.at("FontPath").get<std::string>();
			strncpy_s(text.FontPath, fontPath.c_str(), sizeof(text.FontPath) - 1);
			text.FontPath[sizeof(text.FontPath) - 1] = '\0';
			
			text.PixelSize = j.at("PixelSize").get<float>();
			text.Color = j.at("Color").get<::Color>();
			text.Anchor = static_cast<TextAnchor>(j.at("Anchor").get<uint8_t>());
		}
		
		// Custom serialization for ScriptInstance component (char array needs special handling)
		inline void to_json(nlohmann::json& j, const ScriptInstance& s) {
			j = nlohmann::json{ // convert char array to std::string
				{"ManagedHandle", s.ManagedHandle},
				{"TypeHash", s.TypeHash},
				{"Initialized", s.Initialized},
				{"TypeName", std::string(s.TypeName)},
				{"ScriptPath", std::string(s.ScriptPath)}
			};
		}
		
		inline void from_json(const nlohmann::json& j, ScriptInstance& s) {
			s.ManagedHandle = j.at("ManagedHandle").get<uint64_t>(); // get managed handle as uint64_t
			s.TypeHash = j.at("TypeHash").get<uint32_t>(); // get type hash as uint32_t
			s.Initialized = j.at("Initialized").get<bool>(); // get initialized as bool
			std::string typeName = j.at("TypeName").get<std::string>(); // get type name as std::string
			strncpy_s(s.TypeName, typeName.c_str(), sizeof(s.TypeName) - 1); // copy to char array
			s.TypeName[sizeof(s.TypeName) - 1] = '\0';
			
			// Handle ScriptPath (may not exist in older save files)
			if (j.contains("ScriptPath")) {
				std::string scriptPath = j.at("ScriptPath").get<std::string>();
				strncpy_s(s.ScriptPath, scriptPath.c_str(), sizeof(s.ScriptPath) - 1);
				s.ScriptPath[sizeof(s.ScriptPath) - 1] = '\0';
			}
		}
		
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AudioSource, CueId, Volume, Pitch, Loop, PlayOnStart, Spatial3D)
	} 
}

namespace Serialization {
	class EntitySerializer {
	public:
		using SerializeFn = std::function<void(const ECS::World&, ECS::Entity, json&)>;
		using DeserializeFn = std::function<void(ECS::World&, ECS::Entity, const json&)>;
		using HasFn = std::function<bool(const ECS::World&, ECS::Entity)>;
		using RemoveFn = std::function<void(ECS::World&, ECS::Entity)>;

		struct ComponentInfo {
			std::string Name;
			SerializeFn Serialize;
			DeserializeFn Deserialize;
			HasFn Has;
			RemoveFn Remove;
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
					}
					else {
						j = nullptr;
					}
				},
				// Deserialize
				[](ECS::World& world, ECS::Entity e, const json& j) {
					if (world.Has<T>(e)) {
						world.template Set<T>(e, Serialization::from_json_adl<T>(j));
					}
					else {
						world.template Add<T>(e, Serialization::from_json_adl<T>(j));
					}
				},
				// Has
				[](const ECS::World& world, ECS::Entity e) -> bool {
					return world.Has<T>(e);
				},
				// Remove
				[](ECS::World& world, ECS::Entity e) {
					if (world.Has<T>(e)) {
						world.Remove<T>(e);
					}
				}
			};
		}

		// Serialize a single entity
		static json SerializeEntity(const ECS::World& world, const ECS::Entity e) {
			json entityJson;
			entityJson["Components"] = json::array();

			// CREATE SORTED COMPONENT LIST
			std::vector<std::pair<TypeId, ComponentInfo>> sortedComponents;
			for (const auto& [tid, info] : Registry()) {
				sortedComponents.emplace_back(tid, info);
			}

			// SORT: Transform first, then alphabetical
			std::sort(sortedComponents.begin(), sortedComponents.end(),
				[](const auto& a, const auto& b) {
					// Transform always comes first
					if (a.second.Name == "LocalTransform") return true;
					if (b.second.Name == "LocalTransform") return false;
					// Everything else alphabetical
					return a.second.Name < b.second.Name;
				});

			for (const auto& [tid, info] : sortedComponents) {
				json compJson;
				info.Serialize(world, e, compJson);
				if (!compJson.is_null() && !compJson.empty()) {
					entityJson["Components"].push_back({
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
			if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
				for (const auto& comp : entityJson["Components"]) {
					if (!comp.contains("TypeName") || !comp.contains("Data")) {
						continue; // Skip malformed component entries
					}
					
					std::string typeName = comp["TypeName"].get<std::string>();
					// Find component by name instead of TypeId
					for (const auto& [tid, info] : Registry()) {
						if (info.Name == typeName) {
							try {
								info.Deserialize(world, e, comp["Data"]);
							} 
							catch (const std::exception& ex) {
								// Log error but continue with other components
								std::cerr << "Failed to deserialize component " << typeName << ": " << ex.what() << std::endl;
							}
							break;
						}
					}
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

	REGISTER_COMPONENT_SERIALIZER(Name, ECS::Components::Name, "Name")
	REGISTER_COMPONENT_SERIALIZER(TagMask, ECS::Components::TagMask, "TagMask")
	REGISTER_COMPONENT_SERIALIZER(Active, ECS::Components::Active, "Active")
	REGISTER_COMPONENT_SERIALIZER(Lifetime, ECS::Components::Lifetime, "Lifetime")
	REGISTER_COMPONENT_SERIALIZER(Layer, ECS::Components::Layer, "Layer")
	REGISTER_COMPONENT_SERIALIZER(Rotator, ECS::Components::Rotator, "Rotator")
	REGISTER_COMPONENT_SERIALIZER(LocalTransform, ECS::Components::LocalTransform, "LocalTransform")
	REGISTER_COMPONENT_SERIALIZER(WorldTransform, ECS::Components::WorldTransform, "WorldTransform")
	REGISTER_COMPONENT_SERIALIZER(Velocity, ECS::Components::Velocity, "Velocity")
	REGISTER_COMPONENT_SERIALIZER(Acceleration, ECS::Components::Acceleration, "Acceleration")
	REGISTER_COMPONENT_SERIALIZER(AngularVelocity, ECS::Components::AngularVelocity, "AngularVelocity")
	REGISTER_COMPONENT_SERIALIZER(Rigidbody, ECS::Components::Rigidbody, "Rigidbody")
	REGISTER_COMPONENT_SERIALIZER(PhysicsMaterial2D, ECS::Components::PhysicsMaterial2D, "PhysicsMaterial2D")
	REGISTER_COMPONENT_SERIALIZER(BoxCollider, ECS::Components::BoxCollider, "BoxCollider")
	REGISTER_COMPONENT_SERIALIZER(SphereCollider, ECS::Components::SphereCollider, "SphereCollider")
	REGISTER_COMPONENT_SERIALIZER(LinearVelocity2D, ECS::Components::LinearVelocity2D, "LinearVelocity2D")
	REGISTER_COMPONENT_SERIALIZER(Acceleration2D, ECS::Components::Acceleration2D, "Acceleration2D")
	REGISTER_COMPONENT_SERIALIZER(AngularVelocity2D, ECS::Components::AngularVelocity2D, "AngularVelocity2D")
	REGISTER_COMPONENT_SERIALIZER(Rigidbody2D, ECS::Components::Rigidbody2D, "Rigidbody2D")
	REGISTER_COMPONENT_SERIALIZER(BoxCollider2D, ECS::Components::BoxCollider2D, "BoxCollider2D")
	REGISTER_COMPONENT_SERIALIZER(CircleCollider2D, ECS::Components::CircleCollider2D, "CircleCollider2D")
	REGISTER_COMPONENT_SERIALIZER(SpriteRenderer2D, ECS::Components::SpriteRenderer2D, "SpriteRenderer2D")
	REGISTER_COMPONENT_SERIALIZER(SpriteFlip2D, ECS::Components::SpriteFlip2D, "SpriteFlip2D")
	REGISTER_COMPONENT_SERIALIZER(SpriteSheetAnimation2D, ECS::Components::SpriteSheetAnimation2D, "SpriteSheetAnimation2D")
	REGISTER_COMPONENT_SERIALIZER(AnimationState2D, ECS::Components::AnimationState2D, "AnimationState2D")
	REGISTER_COMPONENT_SERIALIZER(ShapeCircle2D, ECS::Components::ShapeCircle2D, "ShapeCircle2D")
	REGISTER_COMPONENT_SERIALIZER(ShapeBox2D, ECS::Components::ShapeBox2D, "ShapeBox2D")
	REGISTER_COMPONENT_SERIALIZER(ShapeLine2D, ECS::Components::ShapeLine2D, "ShapeLine2D")
	// ShapePolygon2D is a template, therefore it needs to be rewritten
	// REGISTER_COMPONENT_SERIALIZER(ShapePolygon2D, ECS::Components::ShapePolygon2D, "ShapePolygon2D")
	REGISTER_COMPONENT_SERIALIZER(ZIndex2D, ECS::Components::ZIndex2D, "ZIndex2D")
	REGISTER_COMPONENT_SERIALIZER(Light2D, ECS::Components::Light2D, "Light2D")
	REGISTER_COMPONENT_SERIALIZER(Text, ECS::Components::Text, "Text")
	REGISTER_COMPONENT_SERIALIZER(Camera, ECS::Components::Camera3D, "Camera3D")
	REGISTER_COMPONENT_SERIALIZER(CameraEditor, ECS::Components::CameraEditor3D, "CameraEditor3D")
	REGISTER_COMPONENT_SERIALIZER(CameraMatrices, ECS::Components::CameraMatrices, "CameraMatrices")
	REGISTER_COMPONENT_SERIALIZER(ScriptInstance, ECS::Components::ScriptInstance, "ScriptInstance")
	REGISTER_COMPONENT_SERIALIZER(AudioSource, ECS::Components::AudioSource, "AudioSource")
	REGISTER_COMPONENT_SERIALIZER(PrefabLink, ECS::Components::PrefabLink, "PrefabLink")
}

#endif
