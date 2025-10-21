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
#include <functional>
#include <string>
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include <helpers/EntityUtils.h>

using json = nlohmann::json;
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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Name, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::TagMask, Mask)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Active, Enabled)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Lifetime, Time)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Layer, Id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::LocalTransform, Position, Rotation, Scale)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::WorldTransform, Matrix, Dirty)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Velocity, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Acceleration, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::AngularVelocity, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Rigidbody, Mass, InverseMass, LinearDrag, AngularDrag, Flags)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::PhysicsMaterial2D, Friction, Restitution, PositionCorrectPercent)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::BoxCollider, HalfExtents, LayerMask)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::SphereCollider, Radius, LayerMask)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::LinearVelocity2D, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Acceleration2D, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::AngularVelocity2D, Value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Rigidbody2D, Mass, InverseMass, LinearDamping, AngularDamping, GravityScale, Flags)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::BoxCollider2D, HalfExtents, Offset, Rotation, LayerMask, Flags)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::CircleCollider2D, Radius, Offset, LayerMask, Flags)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::SpriteRenderer2D, TextureId, Color, Tiling, Offset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::SpriteFlip2D, FlipX, FlipY)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::ShapeCircle2D, Radius, Offset, Color, Thickness, Filled)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::ShapeBox2D, HalfExtents, Offset, Color, Thickness, Filled)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::ShapeLine2D, A, B, Color, Thickness)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::ZIndex2D, ZOrder)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::Camera, IsOrthographic, FovY, OrthoHeight, Near, Far, Aspect)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::CameraMatrices, View, Projection, ViewProjection)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::ScriptId, Id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::AudioSource, CueId, Volume, Pitch, Loop)

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
						j = json(component); // Explicitly convert the component to JSON
					}
				},
				// Deserialize
				[](ECS::World& world, ECS::Entity e, const json& j) {
					if (world.Has<T>(e)) {
						world.Set<T>(e, j.get<T>());
					}
					else {
						world.Add<T>(e, j.get<T>());
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
			if (entityJson.contains("Components")) {
				for (const auto& comp : entityJson["Components"]) {
					TypeId tid = comp["TypeId"];
					auto it = Registry().find(tid);
					if (it != Registry().end()) {
						it->second.Deserialize(world, e, comp["Data"]);
					}
				}
			}
			return e;
		}
	};

	// --- Register all component serializers ---

	REGISTER_COMPONENT_SERIALIZER(Name, ECS::Components::Name)
	REGISTER_COMPONENT_SERIALIZER(TagMask, ECS::Components::TagMask)
	REGISTER_COMPONENT_SERIALIZER(Active, ECS::Components::Active)
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
	REGISTER_COMPONENT_SERIALIZER(Camera, ECS::Components::Camera)
	REGISTER_COMPONENT_SERIALIZER(CameraMatrices, ECS::Components::CameraMatrices)
	REGISTER_COMPONENT_SERIALIZER(ScriptId, ECS::Components::ScriptId)
	REGISTER_COMPONENT_SERIALIZER(AudioSource, ECS::Components::AudioSource)
}

#endif
