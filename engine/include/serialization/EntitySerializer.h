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
#include "services/ResourceManager.h"  // For texture loading during deserialization
#include "core/Logger.h"

// Forward declarations for managed serialization interop (defined in Interop_World.cpp)
extern "C" const char* WorldInterop_SerializeComponentToJson(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash);
extern "C" void WorldInterop_FreeSerializedString(const char* s);

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

		// Custom serialization for SpriteRenderer2D (char array needs special handling)
		inline void to_json(nlohmann::json& j, const SpriteRenderer2D& sprite) {
			j = nlohmann::json{
				{"TextureId", sprite.TextureId},
				{"TexturePath", std::string(sprite.TexturePath)},  // Convert char[] to string
				{"Color", sprite.Color},
				{"Tiling", sprite.Tiling},
				{"Offset", sprite.Offset}
			};
		}

		inline void from_json(const nlohmann::json& j, SpriteRenderer2D& sprite) {
			// Handle TexturePath (char array) first
			std::string texPath = j.value("TexturePath", std::string());
			strncpy_s(sprite.TexturePath, texPath.c_str(), sizeof(sprite.TexturePath) - 1);
			sprite.TexturePath[sizeof(sprite.TexturePath) - 1] = '\0';

			// IMPORTANT: Reload texture from path if available
			// TextureId is a runtime value that doesn't persist across sessions
			// Only reload if TextureId is not already set (0 means not loaded)
			if (!texPath.empty() && sprite.TextureId == 0) {
				auto tex = RM.Get<Texture>(texPath);
				if (tex) {
					sprite.TextureId = static_cast<uint32_t>(tex->ID());
					sprite.Width = tex->Width();
					sprite.Height = tex->Height();
				}
				else {
					sprite.TextureId = 0; // Invalid texture
					LOG_WARNING("Failed to load texture from path: " << texPath);
				}
			}
			else if (texPath.empty()) {
				sprite.TextureId = 0; // No texture path provided
			}

			sprite.Color = j.value("Color", ::Color{ 1.0f, 1.0f, 1.0f, 1.0f });
			sprite.Tiling = j.value("Tiling", Vector2D{ 1, 1 });
			sprite.Offset = j.value("Offset", Vector2D{ 0, 0 });
		}

		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SpriteFlip2D, FlipX, FlipY)

		// Custom serialization for SpriteSheetAnimation2D (char array needs special handling)
		inline void to_json(nlohmann::json& j, const SpriteSheetAnimation2D& anim) {
			j = nlohmann::json{
				{"TextureId", anim.TextureId},
				{"TexturePath", std::string(anim.TexturePath)},  // Convert char[] to string
				{"FrameWidth", anim.FrameWidth},
				{"FrameHeight", anim.FrameHeight},
				{"SheetWidth", anim.SheetWidth},
				{"SheetHeight", anim.SheetHeight},
				{"StartFrame", anim.StartFrame},
				{"FrameCount", anim.FrameCount},
				{"FramesPerSecond", anim.FramesPerSecond},
				{"Loop", anim.Loop},
				{"Playing", anim.Playing}
			};
		}

		inline void from_json(const nlohmann::json& j, SpriteSheetAnimation2D& anim) {
			// Handle TexturePath (char array) first
			std::string texPath = j.value("TexturePath", std::string());
			strncpy_s(anim.TexturePath, texPath.c_str(), sizeof(anim.TexturePath) - 1);
			anim.TexturePath[sizeof(anim.TexturePath) - 1] = '\0';

			// IMPORTANT: Reload texture from path if available
			// TextureId is a runtime value that doesn't persist across sessions
			// Only reload if TextureId is not already set (0 means not loaded)
			if (!texPath.empty() && anim.TextureId == 0) {
				auto tex = RM.Get<Texture>(texPath);
				if (tex) {
					anim.TextureId = static_cast<uint32_t>(tex->ID());
					// Update sheet dimensions from actual texture if not specified
					if (!j.contains("SheetWidth") || j["SheetWidth"].get<int>() == 0) {
						anim.SheetWidth = tex->Width();
					}
					if (!j.contains("SheetHeight") || j["SheetHeight"].get<int>() == 0) {
						anim.SheetHeight = tex->Height();
					}
				}
				else {
					anim.TextureId = 0; // Invalid texture
					LOG_WARNING("Failed to load sprite sheet from path: " << texPath);
				}
			}
			else if (texPath.empty()) {
				anim.TextureId = 0; // No texture path provided
			}

			anim.FrameWidth = j.value("FrameWidth", 0);
			anim.FrameHeight = j.value("FrameHeight", 0);
			anim.SheetWidth = j.value("SheetWidth", 0);
			anim.SheetHeight = j.value("SheetHeight", 0);
			anim.StartFrame = j.value("StartFrame", 0);
			anim.FrameCount = j.value("FrameCount", 0);
			anim.FramesPerSecond = j.value("FramesPerSecond", 0.0f);
			anim.Loop = j.value("Loop", false);
			anim.Playing = j.value("Playing", false);
		}

		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AnimationState2D, CurrentFrame, TimeAccumulator, Finished)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShapeCircle2D, Radius, Offset, Color, Thickness, Filled)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShapeBox2D, HalfExtents, Offset, Color, Thickness, Filled)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShapeLine2D, A, B, Color, Thickness)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ZIndex2D, ZOrder)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Camera3D, UsePerspective, FOV, NearPlane, FarPlane, OrthoSize, AspectRatio, Active)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CameraEditor3D, UsePerspective, FOV, NearPlane, FarPlane, OrthoSize, AspectRatio)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CameraMatrices, View, Projection, ViewProjection)
		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PrefabInstanceMetadata, PrefabHash, Flags)
		
		// Custom serialization for PrefabLink component (char array needs special handling)
		// [DEPRECATED] Kept for backward compatibility during migration to PrefabInstanceMetadata
		inline void to_json(nlohmann::json& j, const PrefabLink& link) {
			j = nlohmann::json{ {"prefabPath", std::string(link.prefabPath)} };
		}

		inline void from_json(const nlohmann::json& j, PrefabLink& link) {
			std::string path = j.at("prefabPath").get<std::string>();
			strncpy_s(link.prefabPath, path.c_str(), sizeof(link.prefabPath) - 1);
			link.prefabPath[sizeof(link.prefabPath) - 1] = '\0';
		}

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

		// Serialize a single entity (including both C++ and C# components)
		static json SerializeEntity(const ECS::World& world, const ECS::Entity e) {
			json entityJson;
			entityJson["Components"] = json::array();

			// CREATE SORTED COMPONENT LIST
			std::vector<std::pair<TypeId, ComponentInfo>> sortedComponents;
			for (const auto& [tid, info] : Registry()) {
				sortedComponents.emplace_back(tid, info);
			}

			// SORT: Transform first, Name second, then alphabetical
			std::sort(sortedComponents.begin(), sortedComponents.end(),
				[](const auto& a, const auto& b) {
					// Transform always comes first
					if (a.second.Name == "LocalTransform") return true;
					if (b.second.Name == "LocalTransform") return false;

					// Name always comes second
					if (a.second.Name == "Name") return true;
					if (b.second.Name == "Name") return false;

					// Everything else alphabetical
					return a.second.Name < b.second.Name;
				});

			// Serialize all registered C++ components
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

			// Also serialize any C# components registered in the native ECS but not in EntitySerializer
			// These are components discovered at runtime that don't have C++ serialization
			auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
			for (ECS::ComponentTypeId id : allIds) {
				const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
				
				// Skip invalid entries
				if (nativeMeta.TypeHash == 0) continue;
				
				// Determine the component name (from native registry or fallback)
				std::string componentName = ECS::ComponentRegistry::GetComponentNameFromHash(nativeMeta.TypeHash);
				if (componentName.empty()) {
					char buffer[64];
					snprintf(buffer, sizeof(buffer), "Component_0x%08x", nativeMeta.TypeHash);
					componentName = buffer;
				}

				// Check if this component is already handled by the C++ serialization registry
				bool alreadyHandled = false;
				for (const auto& [tid, info] : Registry()) {
					if (info.Name == componentName) {
						alreadyHandled = true;
						break;
					}
				}

				if (alreadyHandled) continue;

				// Only serialize C# components that the entity actually has
				bool hasById = world.HasById(e, id);

				if (!hasById) continue;

				// Log that we're serializing a discovered C# component
				// LOG_INFO("[EntitySerializer] Entity " << e.Index << " has C# component ID " << id << " (hash=0x" << std::hex << nativeMeta.TypeHash << std::dec << ")");

				// Try to get per-entity serialized JSON from managed serializer
				const char* jsonPtr = WorldInterop_SerializeComponentToJson((void*)&world, ECS::EntityUtils::Pack(e), nativeMeta.TypeHash);
				if (jsonPtr) {
					try {
						std::string s(jsonPtr);
						WorldInterop_FreeSerializedString(jsonPtr);
						json compJson = json::parse(s);
						entityJson["Components"].push_back({{"TypeName", componentName}, {"Data", compJson}});
					}
					catch (...) {
						WorldInterop_FreeSerializedString(jsonPtr);
						json compJson = json::object();
						entityJson["Components"].push_back({{"TypeName", componentName}, {"Data", compJson}});
					}
				} else {
					json compJson = json::object();
					entityJson["Components"].push_back({{"TypeName", componentName}, {"Data", compJson}});
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
					// Try C++ registry first
					bool found = false;
					for (const auto& [tid, info] : Registry()) {
						if (info.Name == typeName) {
							try {
								info.Deserialize(world, e, comp["Data"]);
								found = true;
							}
							catch (const std::exception& ex) {
								// Log error but continue with other components
								LOG_ERROR("Failed to deserialize component " << typeName << ": " << ex.what());
							}
							break;
						}
					}
					
					// If not found in C++ registry, try to add as C# component
					if (!found) {
						// Attempt to look up by name in native registry (C# components)
						auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
						for (ECS::ComponentTypeId id : allIds) {
							const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
							if (nativeMeta.TypeHash == 0) continue;
							std::string nativeName = ECS::ComponentRegistry::GetComponentNameFromHash(nativeMeta.TypeHash);
							if (nativeName == typeName) {
								// Found the C# component by name, add it with zero-initialized data
								std::vector<uint8_t> buffer(nativeMeta.Size, 0);
								void* ptr = world.AddComponentById(e, id, buffer.data(), nativeMeta.Size);
								if (ptr) {
									LOG_INFO("[EntitySerializer] Deserialized C# component '" << typeName << "' on entity " << e.Index);
								}
								found = true;
								break;
							}
						}
					}

					if (!found) {
						LOG_WARNING("[EntitySerializer] Component '" << typeName << "' not found during deserialization (not in C++ or C# registry)");
					}
				}
			}
			return e;
		}

		// Serialize entity with full hierarchy (includes all children recursively)
		static json SerializeEntityHierarchy(const ECS::World& world, const ECS::Entity e) {
			// Serialize the entity's components first
			json entityJson = SerializeEntity(world, e);

			// Find all children
			std::vector<EntityId> children;
			world.Each<ECS::Parent>([&](ECS::Entity child, const ECS::Parent& p) {
				// Entity's parent is entity we're serializing, it's a child basically
				if (p.ParentEntity.Index == e.Index) {
					children.push_back(child.Index);
				}
				});

			// Recursively serialize children
			if (!children.empty()) {
				// Make a JSON array for them
				json childrenArray = json::array();
				// For each one...
				for (EntityId childId : children) {
					// Call SerializeEntityHierarchy on it
					ECS::Entity childEntity = world.Resolve(childId);
					// Recursion = function calls itself
					if (world.IsAlive(childEntity)) {
						childrenArray.push_back(SerializeEntityHierarchy(world, childEntity));
					}
				}
				// Add all serialized children to parent's JSON
				entityJson["Children"] = childrenArray;
			}

			return entityJson;
		}

		// Deserialize entity with full hierarchy (includes all children recursively)
		static ECS::Entity DeserializeEntityHierarchy(ECS::World& world, const json& entityJson, EntityId parentId = ECS::Entity::NPOS32) {
			// Create this entity from JSON
			ECS::Entity entity = DeserializeEntity(world, entityJson);

			// Creation failed
			if (entity.IsNull() || !world.IsAlive(entity)) {
				return ECS::NULL_ENTITY;
			}

			// If this entity has a parent...
			if (parentId != ECS::Entity::NPOS32) {
				// Connect this entity to its parent in the hierarchy
				ECS::Entity parent = world.Resolve(parentId);
				if (!parent.IsNull() && world.IsAlive(parent)) {
					world.Attach(entity, parent);
				}
			}

			// If this entity has children in the JSON...
			if (entityJson.contains("Children") && entityJson["Children"].is_array()) {
				// Recursively call itself to create each child
				// Pass THIS entity's ID as the parent
				for (const auto& childJson : entityJson["Children"]) {
					DeserializeEntityHierarchy(world, childJson, entity.Index);
				}
			}

			return entity;
		}

		/**
		 * @brief Save an entity as a prefab file (.prefab)
		 * @param filename Path to write (must end with .prefab)
		 * @param world World containing the entity
		 * @param e Entity to serialize
		 * @return true on success
		 */

		/* 
		static bool SavePrefab(const std::string& filename, const ECS::World& world, const ECS::Entity e) {
			json j = SerializeEntity(world, e);
			return Serializer::SaveJson(filename, "prefab", j);
		}
		*/

		/**
		 * @brief Load a prefab file and instantiate it into the provided world
		 * @param filename Path to read (must end with .prefab)
		 * @param world World to instantiate into
		 * @param outEntity Optional out param to receive created entity
		 * @return true on success
		 */

		/*
		static bool LoadPrefab(const std::string& filename, ECS::World& world, ECS::Entity* outEntity = nullptr) {
			json j;
			if (!Serializer::LoadJson(filename, "prefab", j))
				return false;

			ECS::Entity e = DeserializeEntity(world, j);
			if (outEntity)
				*outEntity = e;

			return true;
		}
		*/
	};

	// --- Register all component serializers ---

	REGISTER_COMPONENT_SERIALIZER(Name, ECS::Components::Name, "Name")
	REGISTER_COMPONENT_SERIALIZER(TagMask, ECS::Components::TagMask, "TagMask")
	REGISTER_COMPONENT_SERIALIZER(Active, ECS::Components::Active, "Active")
	REGISTER_COMPONENT_SERIALIZER(Layer, ECS::Components::Layer, "Layer")
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
	REGISTER_COMPONENT_SERIALIZER(ZIndex2D, ECS::Components::ZIndex2D, "ZIndex2D")
	REGISTER_COMPONENT_SERIALIZER(Light2D, ECS::Components::Light2D, "Light2D")
	REGISTER_COMPONENT_SERIALIZER(Camera, ECS::Components::Camera3D, "Camera3D")
	REGISTER_COMPONENT_SERIALIZER(CameraEditor, ECS::Components::CameraEditor3D, "CameraEditor3D")
	REGISTER_COMPONENT_SERIALIZER(CameraMatrices, ECS::Components::CameraMatrices, "CameraMatrices")
	REGISTER_COMPONENT_SERIALIZER(AudioSource, ECS::Components::AudioSource, "AudioSource")
	REGISTER_COMPONENT_SERIALIZER(PrefabLink, ECS::Components::PrefabLink, "PrefabLink")
	REGISTER_COMPONENT_SERIALIZER(PrefabInstanceMetadata, ECS::Components::PrefabInstanceMetadata, "PrefabInstanceMetadata")
}

#endif