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
#include "ecs/StringTable.h"
#include <helpers/EntityUtils.h>
#include "Serializer.h"
#include <string.h>
#include "services/ResourceManager.h"  // For texture loading during deserialization
#include "core/Logger.h"

// Forward declarations for managed serialization interop (defined in Interop_World.cpp)
extern "C" const char* WorldInterop_SerializeComponentToJson(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash);
extern "C" void WorldInterop_DeserializeComponentFromJson(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash, const char* jsonStr);
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

	constexpr uint32_t FNV1aHash(const char* str) {
		uint32_t hash = 2166136261u;
		while (*str) {
			hash ^= static_cast<uint32_t>(*str);
			hash *= 16777619u;
			++str;
		}
		return hash;
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

		// Custom serialization for Name component (StringId stored as uint32_t)
		inline void to_json(nlohmann::json& j, const Name& n) {
			std::string value = ECS::StringTable::Resolve(n.Value);
			j = nlohmann::json{ {"Value", value} };
		}

		inline void from_json(const nlohmann::json& j, Name& n) {
			if (j.contains("Value")) {
				if (j["Value"].is_string()) {
					std::string value = j["Value"].get<std::string>();
					n.Value = ECS::StringTable::Intern(value);
				}
				else if (j["Value"].is_number_unsigned()) {
					n.Value = j["Value"].get<uint32_t>();
				}
				else {
					n.Value = 0;
				}
			}
			else {
				n.Value = 0;
			}
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

		// Custom serialization for SpriteRenderer2D (StringId paths need special handling)
		inline void to_json(nlohmann::json& j, const SpriteRenderer2D& sprite) {
			std::string texturePath = ECS::StringTable::Resolve(sprite.TexturePath);
			std::string normalTexturePath = ECS::StringTable::Resolve(sprite.NormalTexturePath);
			std::string emissiveTexturePath = ECS::StringTable::Resolve(sprite.EmissiveTexturePath);
			j = nlohmann::json{
				{"TextureId", sprite.TextureId},
				{"TexturePath", texturePath},
				{"NormalTextureId", sprite.NormalTextureId},
				{"NormalTexturePath", normalTexturePath},
				{"EmissiveTextureId", sprite.EmissiveTextureId},
				{"EmissiveTexturePath", emissiveTexturePath},
				{"EmissiveStrength", sprite.EmissiveStrength},
				{"Color", sprite.Color},
				{"Tiling", sprite.Tiling},
				{"Offset", sprite.Offset}
			};
		}

		inline void from_json(const nlohmann::json& j, SpriteRenderer2D& sprite) {
			// Handle TexturePath (StringId) first
			std::string texPath = j.value("TexturePath", std::string());
			sprite.TexturePath = texPath.empty() ? 0 : ECS::StringTable::Intern(texPath);

			// IMPORTANT: Reload texture from path if available
			// TextureId is a runtime value that doesn't persist across sessions
			if (!texPath.empty()) {
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

			// Normal map path (optional)
			std::string normalPath = j.value("NormalTexturePath", std::string());
			sprite.NormalTexturePath = normalPath.empty() ? 0 : ECS::StringTable::Intern(normalPath);

			if (!normalPath.empty()) {
				auto normalTex = RM.Get<Texture>(normalPath);
				if (normalTex) {
					sprite.NormalTextureId = static_cast<uint32_t>(normalTex->ID());
				}
				else {
					sprite.NormalTextureId = 0;
					LOG_WARNING("Failed to load normal map from path: " << normalPath);
				}
			}
			else if (normalPath.empty()) {
				sprite.NormalTextureId = 0;
			}

			// Emissive map path (optional)
			std::string emissivePath = j.value("EmissiveTexturePath", std::string());
			sprite.EmissiveTexturePath = emissivePath.empty() ? 0 : ECS::StringTable::Intern(emissivePath);

			if (!emissivePath.empty()) {
				auto emissiveTex = RM.Get<Texture>(emissivePath);
				if (emissiveTex) {
					sprite.EmissiveTextureId = static_cast<uint32_t>(emissiveTex->ID());
				}
				else {
					sprite.EmissiveTextureId = 0;
					LOG_WARNING("Failed to load emissive map from path: " << emissivePath);
				}
			}
			else if (emissivePath.empty()) {
				sprite.EmissiveTextureId = 0;
			}

			sprite.Color = j.value("Color", ::Color{ 1.0f, 1.0f, 1.0f, 1.0f });
			sprite.Tiling = j.value("Tiling", Vector2D{ 1, 1 });
			sprite.Offset = j.value("Offset", Vector2D{ 0, 0 });
			sprite.EmissiveStrength = j.value("EmissiveStrength", 5.0f);
		}

		NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SpriteFlip2D, FlipX, FlipY)

		// Custom serialization for SpriteSheetAnimation2D (StringId paths need special handling)
		inline void to_json(nlohmann::json& j, const SpriteSheetAnimation2D& anim) {
			std::string texturePath = ECS::StringTable::Resolve(anim.TexturePath);
			std::string normalTexturePath = ECS::StringTable::Resolve(anim.NormalTexturePath);
			j = nlohmann::json{
				{"TextureId", anim.TextureId},
				{"TexturePath", texturePath},
				{"NormalTextureId", anim.NormalTextureId},
				{"NormalTexturePath", normalTexturePath},
				{"FrameWidth", anim.FrameWidth},
				{"FrameHeight", anim.FrameHeight},
				{"SheetWidth", anim.SheetWidth},
				{"SheetHeight", anim.SheetHeight},
				{"StartFrame", anim.StartFrame},
				{"FrameCount", anim.FrameCount},
				{"Row", anim.Row},
				{"FrameOffset", anim.FrameOffset},
				{"FrameLength", anim.FrameLength},
				{"FramesPerSecond", anim.FramesPerSecond},
				{"Loop", anim.Loop},
				{"Playing", anim.Playing},
				{"UseRow", anim.UseRow}
			};
		}

		inline void from_json(const nlohmann::json& j, SpriteSheetAnimation2D& anim) {
			// Handle TexturePath (StringId) first
			std::string texPath = j.value("TexturePath", std::string());
			anim.TexturePath = texPath.empty() ? 0 : ECS::StringTable::Intern(texPath);

			// IMPORTANT: Reload texture from path if available
			// TextureId is a runtime value that doesn't persist across sessions
			if (!texPath.empty()) {
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

			// Normal map path (optional)
			std::string normalPath = j.value("NormalTexturePath", std::string());
			anim.NormalTexturePath = normalPath.empty() ? 0 : ECS::StringTable::Intern(normalPath);

			if (!normalPath.empty()) {
				auto normalTex = RM.Get<Texture>(normalPath);
				if (normalTex) {
					anim.NormalTextureId = static_cast<uint32_t>(normalTex->ID());
				}
				else {
					anim.NormalTextureId = 0;
					LOG_WARNING("Failed to load normal sprite sheet from path: " << normalPath);
				}
			}
			else if (normalPath.empty()) {
				anim.NormalTextureId = 0;
			}

			anim.FrameWidth = j.value("FrameWidth", 0);
			anim.FrameHeight = j.value("FrameHeight", 0);
			anim.SheetWidth = j.value("SheetWidth", 0);
			anim.SheetHeight = j.value("SheetHeight", 0);
			anim.StartFrame = j.value("StartFrame", 0);
			anim.FrameCount = j.value("FrameCount", 0);
			anim.Row = j.value("Row", j.value("RowIndex", 0));
			anim.FrameOffset = j.value("FrameOffset", j.value("RowStartColumn", 0));
			anim.FrameLength = j.value("FrameLength", j.value("RowFrameCount", 0));
			anim.FramesPerSecond = j.value("FramesPerSecond", 0.0f);
			anim.Loop = j.value("Loop", false);
			anim.Playing = j.value("Playing", false);
			anim.UseRow = j.value("UseRow", false);
		}

		// Custom serialization for TileMapComponent (StringId paths need special handling).
		inline void to_json(nlohmann::json& j, const TileMapComponent& tilemap) {
			std::string mapPath = ECS::StringTable::Resolve(tilemap.TileMapPath); // Resolve StringId -> path string.
			std::string tilesetPath = ECS::StringTable::Resolve(tilemap.TilesetTexturePath); // Resolve StringId -> path string.
			j = nlohmann::json{
				{"TileMapPath", mapPath},
				{"TilesetTexturePath", tilesetPath},
				{"TileWorldSize", tilemap.TileWorldSize},
				{"TilePixelSize", tilemap.TilePixelSize},
				{"DefaultWidth", tilemap.DefaultWidth},
				{"DefaultHeight", tilemap.DefaultHeight},
				{"LayerIndex", tilemap.LayerIndex},
				{"Visible", tilemap.Visible}
			};
		}

		inline void from_json(const nlohmann::json& j, TileMapComponent& tilemap) {
			std::string mapPath = j.value("TileMapPath", std::string()); // Read map path from JSON.
			std::string tilesetPath = j.value("TilesetTexturePath", std::string()); // Read tileset path from JSON.
			tilemap.TileMapPath = mapPath.empty() ? 0 : ECS::StringTable::Intern(mapPath); // Store as StringId.
			tilemap.TilesetTexturePath = tilesetPath.empty() ? 0 : ECS::StringTable::Intern(tilesetPath); // Store as StringId.
			tilemap.TileWorldSize = j.value("TileWorldSize", 1.0f); // Default to 1.0 if missing.
			tilemap.TilePixelSize = j.value("TilePixelSize", 32u); // Default tile pixel size.
			tilemap.DefaultWidth = j.value("DefaultWidth", 64u); // Default map width.
			tilemap.DefaultHeight = j.value("DefaultHeight", 64u); // Default map height.
			tilemap.LayerIndex = j.value("LayerIndex", 0u); // Default layer index.
			tilemap.Visible = j.value("Visible", true); // Default visibility.
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
		
		// Custom serialization for PrefabLink component (StringId path needs special handling)
		// [DEPRECATED] Kept for backward compatibility during migration to PrefabInstanceMetadata
		inline void to_json(nlohmann::json& j, const PrefabLink& link) {
			std::string prefabPath = ECS::StringTable::Resolve(link.PrefabPath);
			j = nlohmann::json{ {"prefabPath", prefabPath} };
		}

		inline void from_json(const nlohmann::json& j, PrefabLink& link) {
			std::string path = j.at("prefabPath").get<std::string>();
			link.PrefabPath = path.empty() ? 0 : ECS::StringTable::Intern(path);
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


		// Custom serialization for AudioSource to handle backward-compatible defaults
		inline void to_json(nlohmann::json& j, const AudioSource& src) {
			j = nlohmann::json{
				{"CueId", src.CueId},
				{"Volume", src.Volume},
				{"Pitch", src.Pitch},
				{"Loop", src.Loop},
				{"PlayOnStart", src.PlayOnStart},
				{"Spatial3D", src.Spatial3D},
				{"Bus", src.Bus},
				{"Pan", src.Pan},
				{"EnableFadeIn", src.EnableFadeIn},
				{"EnableFadeOut", src.EnableFadeOut},
				{"FadeInDuration", src.FadeInDuration},
				{"FadeOutDuration", src.FadeOutDuration}
			};
		}

		inline void from_json(const nlohmann::json& j, AudioSource& src) {
			src.CueId = j.value("CueId", 0u);
			src.Volume = j.value("Volume", 1.0f);
			src.Pitch = j.value("Pitch", 1.0f);
			src.Loop = j.value("Loop", false);
			src.PlayOnStart = j.value("PlayOnStart", false);
			src.Spatial3D = j.value("Spatial3D", true);
			src.Bus = j.value("Bus", static_cast<uint8_t>(Audio::Bus::SFX));
			src.Pan = j.value("Pan", 0.0f);
			src.EnableFadeIn = j.value("EnableFadeIn", false);
			src.EnableFadeOut = j.value("EnableFadeOut", false);
			src.FadeInDuration = j.value("FadeInDuration", 1.0f);
			src.FadeOutDuration = j.value("FadeOutDuration", 1.0f);
		}
	
		// Custom serialization for Material2D
		inline void to_json(nlohmann::json& j, const Material2D& mat) {
			std::string normalTexturePath = ECS::StringTable::Resolve(mat.NormalTexturePath);
			std::string mraTexturePath = ECS::StringTable::Resolve(mat.MRA_TexturePath);
			j = nlohmann::json{
				{"NormalTextureId", mat.NormalTextureId},
				{"MRA_TextureId", mat.MRA_TextureId},
				{"NormalTexturePath", normalTexturePath},
				{"MRA_TexturePath", mraTexturePath},
				{"Metallic", mat.Metallic},
				{"Smoothness", mat.Smoothness},
				{"AOStrength", mat.AOStrength},
				{"NormalStrength", mat.NormalStrength},
				{"AlphaCutoff", mat.AlphaCutoff},
				{"Flags", mat.Flags}
			};
		}

		inline void from_json(const nlohmann::json& j, Material2D& mat) {
			// 1. Handle Normal Map
			std::string normPath = j.value("NormalTexturePath", std::string());
			mat.NormalTexturePath = normPath.empty() ? 0 : ECS::StringTable::Intern(normPath);

			if (!normPath.empty()) {
				auto tex = RM.Get<Texture>(normPath);
				if (tex) mat.NormalTextureId = static_cast<uint32_t>(tex->ID());
				else mat.NormalTextureId = 0;
			} else {
				mat.NormalTextureId = 0;
			}

			// 2. Handle MRA Map
			std::string mraPath = j.value("MRA_TexturePath", std::string());
			mat.MRA_TexturePath = mraPath.empty() ? 0 : ECS::StringTable::Intern(mraPath);

			if (!mraPath.empty()) {
				auto tex = RM.Get<Texture>(mraPath);
				if (tex) mat.MRA_TextureId = static_cast<uint32_t>(tex->ID());
				else mat.MRA_TextureId = 0;
			} else {
				mat.MRA_TextureId = 0;
			}

			// 3. Scalar properties
			mat.Metallic = j.value("Metallic", 0.0f);
			mat.Smoothness = j.value("Smoothness", 0.5f);
			mat.AOStrength = j.value("AOStrength", 1.0f);
			mat.NormalStrength = j.value("NormalStrength", 1.0f);
			mat.AlphaCutoff = j.value("AlphaCutoff", 0.5f);
			mat.Flags = j.value("Flags", 0u);
		}

		inline std::string ResolveStringId(uint32_t id) {
			return id ? ECS::StringTable::Resolve(id) : std::string();
		}

		inline uint32_t ReadStringId(const nlohmann::json& j, const char* key, uint32_t defaultId = 0) {
			if (!j.contains(key)) {
				return defaultId;
			}

			const auto& value = j.at(key);
			if (value.is_string()) {
				const std::string str = value.get<std::string>();
				return str.empty() ? 0 : ECS::StringTable::Intern(str);
			}

			if (value.is_number_unsigned()) {
				return value.get<uint32_t>();
			}

			return defaultId;
		}

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GUICanvas, ReferenceSize, Offset, ScaleMode)
	inline void to_json(nlohmann::json& j, const GUIElement& element) {
		j = nlohmann::json{
			{"Position", element.Position},
			{"Size", element.Size},
			{"Visible", element.Visible},
			{"Alignment", static_cast<uint8_t>(element.Alignment)},
			{"ZOrder", element.ZOrder},
			{"Margin", element.Margin},
			{"Padding", element.Padding}
		};
	}

	inline void from_json(const nlohmann::json& j, GUIElement& element) {
		if (j.contains("Position")) element.Position = j.at("Position").get<Vector2D>();
		if (j.contains("Size")) element.Size = j.at("Size").get<Vector2D>();
		element.Visible = j.value("Visible", true);
		element.Alignment = static_cast<GUIAlignment>(j.value("Alignment", static_cast<uint8_t>(GUIAlignment::TopLeft)));
		element.ZOrder = static_cast<int16_t>(j.value("ZOrder", 0));
		element.Margin = j.contains("Margin") ? j.at("Margin").get<Vector4D>() : Vector4D(0.0f, 0.0f, 0.0f, 0.0f);
		element.Padding = j.contains("Padding") ? j.at("Padding").get<Vector4D>() : Vector4D(0.0f, 0.0f, 0.0f, 0.0f);
	}

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GUIPanel, Color, CornerRadius)
	inline void to_json(nlohmann::json& j, const GUIText& text) {
		const std::string value = ECS::StringTable::Resolve(text.TextId);
		const std::string fontPath = ECS::StringTable::Resolve(text.FontPathId);
		j = nlohmann::json{
			{"Text", value},
			{"FontPath", fontPath},
			{"Color", text.Color},
			{"FontSize", text.FontSize},
			{"Wrap", text.Wrap},
			{"HAlign", static_cast<uint8_t>(text.HAlign)},
			{"VAlign", static_cast<uint8_t>(text.VAlign)}
		};
	}

	inline void from_json(const nlohmann::json& j, GUIText& text) {
		const std::string value = j.value("Text", "");
		const std::string fontPath = j.value("FontPath", "");
		text.TextId = value.empty() ? 0 : ECS::StringTable::Intern(value);
		text.FontPathId = fontPath.empty() ? 0 : ECS::StringTable::Intern(fontPath);
		if (j.contains("Color")) {
			text.Color = j.at("Color").get<::Color>();
		}
		if (j.contains("FontSize")) {
			text.FontSize = j.at("FontSize").get<float>();
		} else {
			text.FontSize = j.value("PixelSize", 24.0f);
		}
		text.Wrap = j.value("Wrap", false);
		text.HAlign = static_cast<GUIText::HorizontalAlign>(j.value("HAlign", 0));
		text.VAlign = static_cast<GUIText::VerticalAlign>(j.value("VAlign", 0));
	}

	inline void to_json(nlohmann::json& j, const GUIImage& image) {
		j = nlohmann::json{
			{"TexturePath", ResolveStringId(image.TexturePathId)},
			{"Color", image.Color},
			{"UVRect", image.UVRect},
			{"ScaleMode", static_cast<uint8_t>(image.ScaleMode)},
			{"UseSlicing", image.UseSlicing},
			{"SliceBorder", image.SliceBorder}
		};
	}

	inline void from_json(const nlohmann::json& j, GUIImage& image) {
		image.TexturePathId = ReadStringId(j, "TexturePath", 0);
		if (j.contains("Color")) {
			image.Color = j.at("Color").get<::Color>();
		}
		if (j.contains("UVRect")) {
			image.UVRect = j.at("UVRect").get<Vector4D>();
		}
		image.ScaleMode = static_cast<GUIImageScaleMode>(j.value("ScaleMode", static_cast<uint8_t>(GUIImageScaleMode::Stretch)));
		image.UseSlicing = j.value("UseSlicing", false);
		image.SliceBorder = j.contains("SliceBorder") ? j.at("SliceBorder").get<Vector4D>() : Vector4D(0.0f, 0.0f, 0.0f, 0.0f);
	}

	inline void to_json(nlohmann::json& j, const GUIInput& input) {
		j = nlohmann::json{
			{"Hovered", input.Hovered},
			{"Pressed", input.Pressed},
			{"Clicked", input.Clicked},
			{"Released", input.Released},
			{"Dragging", input.Dragging},
			{"Entered", input.Entered},
			{"Exited", input.Exited}
		};
	}

	inline void from_json(const nlohmann::json& j, GUIInput& input) {
		input.Hovered = j.value("Hovered", false);
		input.Pressed = j.value("Pressed", false);
		input.Clicked = j.value("Clicked", false);
		input.Released = j.value("Released", false);
		input.Dragging = j.value("Dragging", false);
		input.Entered = j.value("Entered", false);
		input.Exited = j.value("Exited", false);
	}

	inline void to_json(nlohmann::json& j, const GUIStateStyle& style) {
		j = nlohmann::json{
			{"NormalColor", style.NormalColor},
			{"HoverColor", style.HoverColor},
			{"PressedColor", style.PressedColor},
			{"DisabledColor", style.DisabledColor}
		};
	}

	inline void from_json(const nlohmann::json& j, GUIStateStyle& style) {
		if (j.contains("NormalColor")) style.NormalColor = j.at("NormalColor").get<::Color>();
		if (j.contains("HoverColor")) style.HoverColor = j.at("HoverColor").get<::Color>();
		if (j.contains("PressedColor")) style.PressedColor = j.at("PressedColor").get<::Color>();
		if (j.contains("DisabledColor")) style.DisabledColor = j.at("DisabledColor").get<::Color>();
	}

	inline void to_json(nlohmann::json& j, const GUIButton& button) {
		j = nlohmann::json{
			{"Text", ResolveStringId(button.TextId)},
			{"FontPath", ResolveStringId(button.FontPathId)},
			{"IconPath", ResolveStringId(button.IconPathId)},
			{"NormalColor", button.NormalColor},
			{"HoverColor", button.HoverColor},
			{"PressedColor", button.PressedColor},
			{"DisabledColor", button.DisabledColor},
			{"TextColor", button.TextColor},
			{"IconColor", button.IconColor},
			{"FontSize", button.FontSize},
			{"CornerRadius", button.CornerRadius},
			{"IconSize", button.IconSize},
			{"IconOffset", button.IconOffset},
			{"Padding", button.Padding},
			{"Disabled", button.Disabled},
			{"Toggle", button.Toggle},
			{"Toggled", button.Toggled}
		};
	}

	inline void from_json(const nlohmann::json& j, GUIButton& button) {
		button.TextId = ReadStringId(j, "Text", 0);
		button.FontPathId = ReadStringId(j, "FontPath", 0);
		button.IconPathId = ReadStringId(j, "IconPath", 0);
		if (j.contains("NormalColor")) button.NormalColor = j.at("NormalColor").get<::Color>();
		if (j.contains("HoverColor")) button.HoverColor = j.at("HoverColor").get<::Color>();
		if (j.contains("PressedColor")) button.PressedColor = j.at("PressedColor").get<::Color>();
		if (j.contains("DisabledColor")) button.DisabledColor = j.at("DisabledColor").get<::Color>();
		if (j.contains("TextColor")) button.TextColor = j.at("TextColor").get<::Color>();
		if (j.contains("IconColor")) button.IconColor = j.at("IconColor").get<::Color>();
		button.FontSize = j.value("FontSize", 24.0f);
		button.CornerRadius = j.value("CornerRadius", 0.0f);
		if (j.contains("IconSize")) button.IconSize = j.at("IconSize").get<Vector2D>();
		if (j.contains("IconOffset")) button.IconOffset = j.at("IconOffset").get<Vector2D>();
		if (j.contains("Padding")) button.Padding = j.at("Padding").get<Vector4D>();
		button.Disabled = j.value("Disabled", false);
		button.Toggle = j.value("Toggle", false);
		button.Toggled = j.value("Toggled", false);
	}

	inline void to_json(nlohmann::json& j, const GUISlider& slider) {
		j = nlohmann::json{
			{"Value", slider.Value},
			{"Min", slider.Min},
			{"Max", slider.Max},
			{"Step", slider.Step},
			{"TrackColor", slider.TrackColor},
			{"FillColor", slider.FillColor},
			{"KnobColor", slider.KnobColor},
			{"CornerRadius", slider.CornerRadius},
			{"KnobSize", slider.KnobSize},
			{"Padding", slider.Padding},
			{"Horizontal", slider.Horizontal},
			{"Disabled", slider.Disabled}
		};
	}

	inline void from_json(const nlohmann::json& j, GUISlider& slider) {
		slider.Value = j.value("Value", 0.0f);
		slider.Min = j.value("Min", 0.0f);
		slider.Max = j.value("Max", 1.0f);
		slider.Step = j.value("Step", 0.0f);
		if (j.contains("TrackColor")) slider.TrackColor = j.at("TrackColor").get<::Color>();
		if (j.contains("FillColor")) slider.FillColor = j.at("FillColor").get<::Color>();
		if (j.contains("KnobColor")) slider.KnobColor = j.at("KnobColor").get<::Color>();
		slider.CornerRadius = j.value("CornerRadius", 0.0f);
		if (j.contains("KnobSize")) slider.KnobSize = j.at("KnobSize").get<Vector2D>();
		if (j.contains("Padding")) slider.Padding = j.at("Padding").get<Vector4D>();
		slider.Horizontal = j.value("Horizontal", true);
		slider.Disabled = j.value("Disabled", false);
		slider.ValueChanged = false;
	}

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
			uint32_t TypeHash;
			SerializeFn Serialize;
			DeserializeFn Deserialize;
			HasFn Has;
			RemoveFn Remove;
		};

		// Registry: TypeHash -> ComponentInfo
		static std::unordered_map<uint32_t, ComponentInfo>& Registry() {
			static std::unordered_map<uint32_t, ComponentInfo> reg;
			return reg;
		}

		// Registration macro
		template<typename T>
		static void RegisterComponent(const char* name) {
			const uint32_t typeHash = Serialization::FNV1aHash(name);
			// Capture the concrete component id so serialization does not depend on hash registration order
			const ECS::ComponentTypeId componentId = ECS::ComponentRegistry::Type<T>();
			// Special case: For TileMapComponent, always use the captured component ID
			const bool useCapturedIdOnly = (name && std::strcmp(name, "TileMapComponent") == 0);
			// Resolver lambda
			auto resolveId = [typeHash, componentId, useCapturedIdOnly]() -> ECS::ComponentTypeId {
				if (useCapturedIdOnly) {
					return componentId;
				}
				return ECS::ComponentRegistry::GetComponentIdFromHash(typeHash);
			};
			// Register the component info
			Registry()[typeHash] = ComponentInfo{
				name,
				typeHash,
				// Serialize
				// Capture by value to ensure correct behavior even if registry changes
				[resolveId](const ECS::World& world, ECS::Entity e, json& j) {
					const ECS::ComponentTypeId id = resolveId();
					if (id == ECS::NULL_COMPONENT_ID) {
						j = nullptr;
						return;
					}

					// Check if entity has the component
					if (world.HasById(e, id)) {
						const void* ptr = const_cast<ECS::World&>(world).GetRawComponentPtr(e, id);
						if (ptr) {
							const T& component = *static_cast<const T*>(ptr);
							Serialization::to_json_adl<T>(j, component);
							return;
						}
					}

					j = nullptr;
				},
				// Deserialize
				// Capture by value to ensure correct behavior even if registry changes
				[resolveId](ECS::World& world, ECS::Entity e, const json& j) {
					const ECS::ComponentTypeId id = resolveId();
					if (id == ECS::NULL_COMPONENT_ID) {
						return;
					}

					T value = Serialization::from_json_adl<T>(j);
					void* ptr = world.GetRawComponentPtr(e, id);
					if (ptr) {
						*static_cast<T*>(ptr) = value;
					}
					else {
						// Component not present, add it
						world.AddComponentById(e, id, &value, sizeof(T));
					}
				},
				// Has
				// Capture by value to ensure correct behavior even if registry changes
				[resolveId](const ECS::World& world, ECS::Entity e) -> bool {
					const ECS::ComponentTypeId id = resolveId();
					if (id == ECS::NULL_COMPONENT_ID) {
						return false;
					}
					// Check presence by ID
					return world.HasById(e, id);
				},
				// Remove
				// Capture by value to ensure correct behavior even if registry changes
				[resolveId](ECS::World& world, ECS::Entity e) {
					const ECS::ComponentTypeId id = resolveId();
					if (id == ECS::NULL_COMPONENT_ID) {
						return;
					}
					// Remove by ID
					world.RemoveById(e, id);
				}
			};
		}

		// Serialize a single entity (including both C++ and managed components)
		static json SerializeEntity(const ECS::World& world, const ECS::Entity e) {
			json entityJson;
			entityJson["Components"] = json::array();

			// CREATE SORTED COMPONENT LIST
			std::vector<std::pair<uint32_t, ComponentInfo>> sortedComponents;
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

			// Also serialize any managed components registered in the native ECS but not in EntitySerializer
			// These are components discovered at runtime that don't have C++ serialization
			auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
			for (ECS::ComponentTypeId id : allIds) {
				const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
				
				// Skip invalid entries
				if (nativeMeta.TypeHash == 0 || !nativeMeta.IsManaged) continue;
				
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

				// Only serialize managed components that the entity actually has
				bool hasById = world.HasById(e, id);

				if (!hasById) continue;

				// Log that we're serializing a discovered managed component
				// LOG_INFO("[EntitySerializer] Entity " << e.Index << " has managed component ID " << id << " (hash=0x" << std::hex << nativeMeta.TypeHash << std::dec << ")");

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
					constexpr const char* kPrefix = "ECS::Components::";
					std::string normalizedTypeName = typeName;
					if (normalizedTypeName.rfind(kPrefix, 0) == 0) {
						normalizedTypeName = normalizedTypeName.substr(std::strlen(kPrefix));
					}
					// Try C++ registry first
					bool found = false;
					for (const auto& [tid, info] : Registry()) {
						if (info.Name == typeName || info.Name == normalizedTypeName) {
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
					
					// If not found in C++ registry, try to add as managed component
					if (!found) {
						// Attempt to look up by name in native registry (managed components)
						auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
						for (ECS::ComponentTypeId id : allIds) {
							const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
							if (nativeMeta.TypeHash == 0 || !nativeMeta.IsManaged) continue;
							std::string nativeName = ECS::ComponentRegistry::GetComponentNameFromHash(nativeMeta.TypeHash);
							if (nativeName == typeName || nativeName == normalizedTypeName) {
								// Found the managed component by name, add it with zero-initialized data
								std::vector<uint8_t> buffer(nativeMeta.Size, 0);
								void* ptr = world.AddComponentById(e, id, buffer.data(), nativeMeta.Size);
								if (ptr) {
									if (!comp["Data"].is_null()) {
										const std::string jsonStr = comp["Data"].dump();
										WorldInterop_DeserializeComponentFromJson(
											(void*)&world,
											ECS::EntityUtils::Pack(e),
											nativeMeta.TypeHash,
											jsonStr.c_str());
									}
									LOG_INFO("[EntitySerializer] Deserialized managed component '" << typeName << "' on entity " << e.Index);
								}
								found = true;
								break;
							}
						}
					}

					if (!found) {
						LOG_WARNING("[EntitySerializer] Component '" << typeName << "' not found during deserialization (not in C++ or managed registry)");
					}
				}
			}

			if (!world.Has<ECS::Components::LocalTransform>(e)) {
				world.Set<ECS::Components::LocalTransform>(e, ECS::Components::LocalTransform{});
			}

			if (!world.Has<ECS::Components::WorldTransform>(e)) {
				ECS::Components::WorldTransform wt{};
				wt.Dirty = true;
				world.Set<ECS::Components::WorldTransform>(e, wt);
			}

			// Ensure every entity has a Layer so editor picking and rendering remain functional.
			if (!world.Has<ECS::Components::Layer>(e)) {
				world.Set<ECS::Components::Layer>(e, ECS::Components::Layer{ 0 });
			}
			return e;
		}

		// Serialize entity with full hierarchy (includes all children recursively)
		static json SerializeEntityHierarchy(const ECS::World& world, const ECS::Entity e) {
			// Serialize the entity's components first
			json entityJson = SerializeEntity(world, e);

			// Find all children
			std::vector<EntityId> children;
			world.Each<ECS::Components::Parent>([&](ECS::Entity child, const ECS::Components::Parent& p) {
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
	REGISTER_COMPONENT_SERIALIZER(TileMapComponent, ECS::Components::TileMapComponent, "TileMapComponent")
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
	REGISTER_COMPONENT_SERIALIZER(Material2D, ECS::Components::Material2D, "Material2D");
	REGISTER_COMPONENT_SERIALIZER(GUICanvas, ECS::Components::GUICanvas, "GUICanvas")
	REGISTER_COMPONENT_SERIALIZER(GUIElement, ECS::Components::GUIElement, "GUIElement")
	REGISTER_COMPONENT_SERIALIZER(GUIPanel, ECS::Components::GUIPanel, "GUIPanel");
	REGISTER_COMPONENT_SERIALIZER(GUIText, ECS::Components::GUIText, "GUIText");
	REGISTER_COMPONENT_SERIALIZER(GUIImage, ECS::Components::GUIImage, "GUIImage");
	REGISTER_COMPONENT_SERIALIZER(GUIInput, ECS::Components::GUIInput, "GUIInput");
	REGISTER_COMPONENT_SERIALIZER(GUIStateStyle, ECS::Components::GUIStateStyle, "GUIStateStyle");
	REGISTER_COMPONENT_SERIALIZER(GUIButton, ECS::Components::GUIButton, "GUIButton");
	REGISTER_COMPONENT_SERIALIZER(GUISlider, ECS::Components::GUISlider, "GUISlider");
}

#endif
