/* Start Header *****************************************************************/
/*!
\file    EntitySerializer.h
\authors Daniel Neo Zuo Feng Kay (85%)
         Muhammad Nur Fadzly Bin Zulkifli (10%)
         Foo Rui Qin (5%)
\par     k.danielneozuofeng@digipen.edu
         muhammadnurfadzly.b@digipen.edu
         ruiqin.foo@digipen.edu
\date    12th March 2026
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
#include <algorithm>
#include <iostream>
#include <filesystem>
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/StringTable.h"
#include <helpers/EntityUtils.h>
#include "Serializer.h"
#include <string.h>
#include "audio/AudioCueRegistry.h"
#include "services/ResourceManager.h"  // For texture loading during deserialization
#include "core/Logger.h"
#include "core/ProjectPaths.h"

// Forward declarations for managed serialization interop (defined in Interop_World.cpp)
extern "C" const char* WorldInterop_SerializeComponentToJson(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash);
extern "C" void WorldInterop_DeserializeComponentFromJson(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash, const char* jsonStr);
extern "C" void WorldInterop_FreeSerializedString(const char* s);

using json = nlohmann::json;

#pragma region nlohmann_adl_helpers
// ADL helpers: call unqualified to_json/from_json with 'using' to enable ADL
namespace Serialization {
    // Route serialization through ADL so component-specific overloads are discovered reliably
    template<typename U>
    inline void to_json_adl(nlohmann::json& j, const U& v) {
        using nlohmann::to_json;
        to_json(j, v);
    }

    // Route deserialization through ADL so component-specific overloads are discovered reliably
    template<typename U>
    inline U from_json_adl(const nlohmann::json& j) {
        U tmp;
        using nlohmann::from_json;
        from_json(j, tmp);
        return tmp;
    }

    // Hash a component name into a stable 32-bit key used by the serializer registry
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

// Nested Type Serialization Definitions
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector2D, X, Y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector3D, X, Y, Z)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector4D, X, Y, Z, W)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Quaternion, X, Y, Z, W)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, R, G, B, A)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Matrix4x4, m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33)

// ECS Component Serialization Definitions
namespace ECS {
    namespace Components {
        // Convert absolute project paths into project-relative strings for portable saved files
        inline std::string NormalizeProjectPathForStorage(const std::string& path) {
            if (path.empty() || !Engine::ProjectPaths::IsInitialized()) {
                return path;
            }

            std::filesystem::path fsPath(path);
            if (!fsPath.is_absolute()) {
                return path;
            }

            const std::string relative = Engine::ProjectPaths::ToRelativePath(path);
            return relative.empty() ? path : relative;
        }

        // Expand saved relative project paths back into absolute paths for runtime loading
        inline std::string ResolveProjectPathForLoad(const std::string& path) {
            if (path.empty() || !Engine::ProjectPaths::IsInitialized()) {
                return path;
            }

            std::filesystem::path fsPath(path);
            if (fsPath.is_absolute()) {
                return path;
            }

            return Engine::ProjectPaths::ToAbsolutePath(path);
        }

        // Place component-specific NLOHMANN macros inside the component namespace so ADL
        // can locate the generated to_json/from_json overloads for these types
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
            texturePath = NormalizeProjectPathForStorage(texturePath);
            normalTexturePath = NormalizeProjectPathForStorage(normalTexturePath);
            emissiveTexturePath = NormalizeProjectPathForStorage(emissiveTexturePath);
            j = nlohmann::json{
                {"TextureId", sprite.TextureId},
                {"TexturePath", texturePath},
                {"NormalTextureId", sprite.NormalTextureId},
                {"NormalTexturePath", normalTexturePath},
                {"EmissiveTextureId", sprite.EmissiveTextureId},
                {"EmissiveTexturePath", emissiveTexturePath},
                {"EmissiveStrength", sprite.EmissiveStrength},
                {"TextureFilter", static_cast<uint8_t>(sprite.TextureFilter)},
                {"Color", sprite.Color},
                {"Tiling", sprite.Tiling},
                {"Offset", sprite.Offset}
            };
        }

        inline void from_json(const nlohmann::json& j, SpriteRenderer2D& sprite) {
            // Handle TexturePath (StringId) first
            std::string texPath = j.value("TexturePath", std::string());
            texPath = NormalizeProjectPathForStorage(texPath);
            sprite.TexturePath = texPath.empty() ? 0 : ECS::StringTable::Intern(texPath);

            // IMPORTANT: Reload texture from path if available
            // TextureId is a runtime value that doesn't persist across sessions
            if (!texPath.empty()) {
                const std::string texLoadPath = ResolveProjectPathForLoad(texPath);
                auto tex = RM.Get<Texture>(texLoadPath);
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
            normalPath = NormalizeProjectPathForStorage(normalPath);
            sprite.NormalTexturePath = normalPath.empty() ? 0 : ECS::StringTable::Intern(normalPath);

            if (!normalPath.empty()) {
                const std::string normalLoadPath = ResolveProjectPathForLoad(normalPath);
                auto normalTex = RM.Get<Texture>(normalLoadPath);
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
            emissivePath = NormalizeProjectPathForStorage(emissivePath);
            sprite.EmissiveTexturePath = emissivePath.empty() ? 0 : ECS::StringTable::Intern(emissivePath);

            if (!emissivePath.empty()) {
                const std::string emissiveLoadPath = ResolveProjectPathForLoad(emissivePath);
                auto emissiveTex = RM.Get<Texture>(emissiveLoadPath);
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
            sprite.TextureFilter = static_cast<Graphics::TextureFilter>(
                j.value("TextureFilter", static_cast<uint8_t>(Graphics::TextureFilter::Nearest)));
        }

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SpriteFlip2D, FlipX, FlipY)

        // Custom serialization for SpriteSheetAnimation2D (StringId paths need special handling)
        inline void to_json(nlohmann::json& j, const SpriteSheetAnimation2D& anim) {
            std::string texturePath = ECS::StringTable::Resolve(anim.TexturePath);
            std::string normalTexturePath = ECS::StringTable::Resolve(anim.NormalTexturePath);
            texturePath = NormalizeProjectPathForStorage(texturePath);
            normalTexturePath = NormalizeProjectPathForStorage(normalTexturePath);

            nlohmann::json segments = nlohmann::json::array();
            const int segCount = std::clamp(static_cast<int>(anim.SegmentCount), 0, SpriteSheetAnimation2D::MaxSegments);
            for (int i = 0; i < segCount; ++i) {
                segments.push_back({
                    {"Row", anim.SegmentRows[i]},
                    {"FrameOffset", anim.SegmentOffsets[i]},
                    {"FrameLength", anim.SegmentLengths[i]}
                });
            }

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
                {"UseRow", anim.UseRow},
                {"UseSegments", anim.UseSegments},
                {"Segments", segments},
                {"TextureFilter", static_cast<uint8_t>(anim.TextureFilter)}
            };
        }

        inline void from_json(const nlohmann::json& j, SpriteSheetAnimation2D& anim) {
            // Handle TexturePath (StringId) first
            std::string texPath = j.value("TexturePath", std::string());
            texPath = NormalizeProjectPathForStorage(texPath);
            anim.TexturePath = texPath.empty() ? 0 : ECS::StringTable::Intern(texPath);

            // IMPORTANT: Reload texture from path if available
            // TextureId is a runtime value that doesn't persist across sessions
            if (!texPath.empty()) {
                const std::string texLoadPath = ResolveProjectPathForLoad(texPath);
                auto tex = RM.Get<Texture>(texLoadPath);
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
            normalPath = NormalizeProjectPathForStorage(normalPath);
            anim.NormalTexturePath = normalPath.empty() ? 0 : ECS::StringTable::Intern(normalPath);

            if (!normalPath.empty()) {
                const std::string normalLoadPath = ResolveProjectPathForLoad(normalPath);
                auto normalTex = RM.Get<Texture>(normalLoadPath);
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
            anim.UseSegments = j.value("UseSegments", false);
            anim.SegmentCount = 0;
            for (int i = 0; i < SpriteSheetAnimation2D::MaxSegments; ++i) {
                anim.SegmentRows[i] = 0;
                anim.SegmentOffsets[i] = 0;
                anim.SegmentLengths[i] = 0;
            }

            if (j.contains("Segments") && j["Segments"].is_array()) {
                const auto& segs = j["Segments"];
                const int count = std::min(static_cast<int>(segs.size()), SpriteSheetAnimation2D::MaxSegments);
                for (int i = 0; i < count; ++i) {
                    const auto& seg = segs[i];
                    if (!seg.is_object()) continue;
                    anim.SegmentRows[i] = seg.value("Row", 0);
                    anim.SegmentOffsets[i] = seg.value("FrameOffset", 0);
                    anim.SegmentLengths[i] = seg.value("FrameLength", 0);
                    ++anim.SegmentCount;
                }
            }

            anim.TextureFilter = static_cast<Graphics::TextureFilter>(
                j.value("TextureFilter", static_cast<uint8_t>(Graphics::TextureFilter::Nearest)));
        }

        // Custom serialization for TileMapComponent (StringId paths need special handling)
        inline void to_json(nlohmann::json& j, const TileMapComponent& tilemap) {
            std::string mapPath = ECS::StringTable::Resolve(tilemap.TileMapPath); // Resolve StringId -> path string.
            std::string tilesetPath = ECS::StringTable::Resolve(tilemap.TilesetTexturePath); // Resolve StringId -> path string.
            mapPath = NormalizeProjectPathForStorage(mapPath);
            tilesetPath = NormalizeProjectPathForStorage(tilesetPath);
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
            mapPath = NormalizeProjectPathForStorage(mapPath);
            tilesetPath = NormalizeProjectPathForStorage(tilesetPath);
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

        // Custom serialization for BoidFlock
        inline void to_json(nlohmann::json& j, const BoidFlock& b)
        {
            j = nlohmann::json{
                {"count", b.count},
                {"separationWeight", b.separationWeight},
                {"alignmentWeight", b.alignmentWeight},
                {"cohesionWeight", b.cohesionWeight},
                {"visualRange", b.visualRange},
                {"maxSpeed", b.maxSpeed},
                {"maxForce", b.maxForce},
                {"boidSize", b.boidSize},
                {"collisionAvoidWeight", b.collisionAvoidWeight},
                {"collisionAvoidRadius", b.collisionAvoidRadius}
            };
        }

        inline void from_json(const nlohmann::json& j, BoidFlock& b)
        {
            // Match struct defaults exactly
            b.count = j.value("count", 5000);
            b.separationWeight = j.value("separationWeight", 2.5f);
            b.alignmentWeight = j.value("alignmentWeight", 3.0f);
            b.cohesionWeight = j.value("cohesionWeight", 0.4f);

            b.collisionAvoidWeight = j.value("collisionAvoidWeight", 2.5f);
            b.collisionAvoidRadius = j.value("collisionAvoidRadius", 3.0f);

            b.visualRange = j.value("visualRange", 4.0f);
            b.maxSpeed = j.value("maxSpeed", 4.0f);
            b.maxForce = j.value("maxForce", 1.2f);
            b.boidSize = j.value("boidSize", 1.0f);
        }

        inline void to_json(nlohmann::json& j, const ParticleEmitter& e)
        {
            std::string path = ECS::StringTable::Resolve(e.TexturePath);
            path = NormalizeProjectPathForStorage(path);
            j = nlohmann::json{
                {"presetId", e.presetId},
                {"maxParticles", e.maxParticles},
                {"emissionRate", e.emissionRate},
                {"burstCount", e.burstCount},
                {"particleSize", e.particleSize},
                {"active", e.active},
                {"TexturePath", path},
                {"speedMin", e.speedMin},           {"speedMax", e.speedMax},
                {"gravityX", e.gravityX},           {"gravityY", e.gravityY},
                {"drag", e.drag},                   {"turbulence", e.turbulence},
                {"wobbleFrequency", e.wobbleFrequency}, {"wobbleAmplitude", e.wobbleAmplitude},
                {"sizeStart", e.sizeStart},         {"sizeEnd", e.sizeEnd},
                {"lifetimeMin", e.lifetimeMin},     {"lifetimeMax", e.lifetimeMax},
                {"emissionAngle", e.emissionAngle}, {"emissionSpread", e.emissionSpread},
                {"emissionRadius", e.emissionRadius},{"emissionShape", e.emissionShape},
                {"colorStartR", e.colorStartR},     {"colorStartG", e.colorStartG},
                {"colorStartB", e.colorStartB},     {"colorStartA", e.colorStartA},
                {"colorEndR", e.colorEndR},         {"colorEndG", e.colorEndG},
                {"colorEndB", e.colorEndB},         {"colorEndA", e.colorEndA},
                {"dieOnCollision", e.dieOnCollision},{"bounciness", e.bounciness},
                {"killOutOfBounds", e.killOutOfBounds},
                {"rotationSpeedMin", e.rotationSpeedMin},{"rotationSpeedMax", e.rotationSpeedMax}
            };
        }

        inline void from_json(const nlohmann::json& j, ParticleEmitter& e)
        {
            e.presetId = j.value("presetId", 0u);
            e.maxParticles = j.value("maxParticles", 1000);
            e.emissionRate = j.value("emissionRate", 50.0f);
            e.burstCount = j.value("burstCount", 0);
            e.particleSize = j.value("particleSize", 0.3f);
            e.active = j.value("active", true);

            e.speedMin = j.value("speedMin", 0.5f);
            e.speedMax = j.value("speedMax", 1.5f);
            e.gravityX = j.value("gravityX", 0.0f);
            e.gravityY = j.value("gravityY", 0.3f);
            e.drag = j.value("drag", 0.3f);
            e.turbulence = j.value("turbulence", 0.0f);
            e.wobbleFrequency = j.value("wobbleFrequency", 0.0f);
            e.wobbleAmplitude = j.value("wobbleAmplitude", 0.0f);
            e.sizeStart = j.value("sizeStart", 0.2f);
            e.sizeEnd = j.value("sizeEnd", 0.5f);
            e.lifetimeMin = j.value("lifetimeMin", 1.0f);
            e.lifetimeMax = j.value("lifetimeMax", 3.0f);
            e.emissionAngle = j.value("emissionAngle", 1.5708f);
            e.emissionSpread = j.value("emissionSpread", 0.5f);
            e.emissionRadius = j.value("emissionRadius", 0.5f);
            e.emissionShape = j.value("emissionShape", (uint8_t)0);
            e.colorStartR = j.value("colorStartR", 1.0f);
            e.colorStartG = j.value("colorStartG", 1.0f);
            e.colorStartB = j.value("colorStartB", 1.0f);
            e.colorStartA = j.value("colorStartA", 1.0f);
            e.colorEndR = j.value("colorEndR", 1.0f);
            e.colorEndG = j.value("colorEndG", 1.0f);
            e.colorEndB = j.value("colorEndB", 1.0f);
            e.colorEndA = j.value("colorEndA", 0.0f);
            e.dieOnCollision = j.value("dieOnCollision", false);
            e.bounciness = j.value("bounciness", 0.0f);
            e.killOutOfBounds = j.value("killOutOfBounds", false);
            e.rotationSpeedMin = j.value("rotationSpeedMin", 0.0f);
            e.rotationSpeedMax = j.value("rotationSpeedMax", 0.0f);

            // Texture (runtime only)
            std::string path = j.value("TexturePath", std::string());
            path = NormalizeProjectPathForStorage(path);
            e.TexturePath = path.empty() ? 0 : ECS::StringTable::Intern(path);
            if (!path.empty()) {
                auto tex = RM.Get<Texture>(ResolveProjectPathForLoad(path));
                e.textureId = tex ? (uint32_t)tex->ID() : 0;
            }
            else {
                e.textureId = 0;
            }
        }
        
        // Custom serialization for PrefabLink component (StringId path needs special handling)
        // [DEPRECATED] Kept for backward compatibility during migration to PrefabInstanceMetadata
        inline void to_json(nlohmann::json& j, const PrefabLink& link) {
            std::string prefabPath = ECS::StringTable::Resolve(link.PrefabPath);
            prefabPath = NormalizeProjectPathForStorage(prefabPath);
            j = nlohmann::json{ {"prefabPath", prefabPath} };
        }

        inline void from_json(const nlohmann::json& j, PrefabLink& link) {
            std::string path = j.at("prefabPath").get<std::string>();
            path = NormalizeProjectPathForStorage(path);
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
            std::string cuePath = ECS::StringTable::Resolve(src.CuePathId);
            cuePath = NormalizeProjectPathForStorage(cuePath);
            j = nlohmann::json{
                {"CuePath", cuePath},
                {"Volume", src.Volume},
                {"Pitch", src.Pitch},
                {"Loop", src.Loop},
                {"PlayOnStart", src.PlayOnStart},
                {"Spatial3D", src.Spatial3D},
                {"Bus", src.Bus},
                {"Pan", src.Pan},
                {"EnableLowPass", src.EnableLowPass},
                {"LowPassGain", src.LowPassGain},
                {"EnableFadeIn", src.EnableFadeIn},
                {"EnableFadeOut", src.EnableFadeOut},
                {"FadeInDuration", src.FadeInDuration},
                {"FadeOutDuration", src.FadeOutDuration}
            };
            if (cuePath.empty()) {
                j["CueId"] = src.CueId;
            }
        }

        inline void from_json(const nlohmann::json& j, AudioSource& src) {
            std::string cuePath = j.value("CuePath", std::string());
            if (!cuePath.empty()) {
                cuePath = NormalizeProjectPathForStorage(cuePath);
                const std::string norm = Audio::AudioCueRegistry::NormalizePath(cuePath);
                src.CuePathId = ECS::StringTable::Intern(norm);
                src.CueId = Audio::AudioCueRegistry::HashPath(norm);
            }
            else {
                src.CuePathId = 0;
                src.CueId = j.value("CueId", 0u);
            }
            src.Volume = j.value("Volume", 1.0f);
            src.Pitch = j.value("Pitch", 1.0f);
            src.Loop = j.value("Loop", false);
            src.PlayOnStart = j.value("PlayOnStart", false);
            src.Spatial3D = j.value("Spatial3D", true);
            src.Bus = j.value("Bus", static_cast<uint8_t>(Audio::Bus::SFX));
            src.Pan = j.value("Pan", 0.0f);
            src.EnableLowPass = j.value("EnableLowPass", false);
            src.LowPassGain = j.value("LowPassGain", 1.0f);
            src.EnableFadeIn = j.value("EnableFadeIn", false);
            src.EnableFadeOut = j.value("EnableFadeOut", false);
            src.FadeInDuration = j.value("FadeInDuration", 1.0f);
            src.FadeOutDuration = j.value("FadeOutDuration", 1.0f);
        }
    
        // Custom serialization for Material2D
        inline void to_json(nlohmann::json& j, const Material2D& mat) {
            std::string normalTexturePath = ECS::StringTable::Resolve(mat.NormalTexturePath);
            std::string mraTexturePath = ECS::StringTable::Resolve(mat.MRA_TexturePath);
            normalTexturePath = NormalizeProjectPathForStorage(normalTexturePath);
            mraTexturePath = NormalizeProjectPathForStorage(mraTexturePath);
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
            normPath = NormalizeProjectPathForStorage(normPath);
            mat.NormalTexturePath = normPath.empty() ? 0 : ECS::StringTable::Intern(normPath);

            if (!normPath.empty()) {
                const std::string normLoadPath = ResolveProjectPathForLoad(normPath);
                auto tex = RM.Get<Texture>(normLoadPath);
                if (tex) mat.NormalTextureId = static_cast<uint32_t>(tex->ID());
                else mat.NormalTextureId = 0;
            }
            else {
                mat.NormalTextureId = 0;
            }

            // 2. Handle MRA Map
            std::string mraPath = j.value("MRA_TexturePath", std::string());
            mraPath = NormalizeProjectPathForStorage(mraPath);
            mat.MRA_TexturePath = mraPath.empty() ? 0 : ECS::StringTable::Intern(mraPath);

            if (!mraPath.empty()) {
                const std::string mraLoadPath = ResolveProjectPathForLoad(mraPath);
                auto tex = RM.Get<Texture>(mraLoadPath);
                if (tex) mat.MRA_TextureId = static_cast<uint32_t>(tex->ID());
                else mat.MRA_TextureId = 0;
            }
            else {
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

        // Resolve a StringTable id to text and return empty when the id is zero
        inline std::string ResolveStringId(uint32_t id) {
            return id ? ECS::StringTable::Resolve(id) : std::string();
        }

        // Resolve a StringTable id and normalize it for stable on-disk path storage
        inline std::string ResolvePathId(uint32_t id) {
            return NormalizeProjectPathForStorage(ResolveStringId(id));
        }

        // Read a path-like field from JSON as either string or legacy uint32 id
        inline uint32_t ReadPathId(const nlohmann::json& j, const char* key, uint32_t defaultId = 0) {
            if (!j.contains(key)) {
                return defaultId;
            }

            const auto& value = j.at(key);
            if (value.is_string()) {
                std::string str = value.get<std::string>();
                str = NormalizeProjectPathForStorage(str);
                return str.empty() ? 0 : ECS::StringTable::Intern(str);
            }

            if (value.is_number_unsigned()) {
                return value.get<uint32_t>();
            }

            return defaultId;
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
    inline void to_json(nlohmann::json& j, const GUIRenderMode& mode) {
        j = nlohmann::json{
            {"Space", static_cast<uint8_t>(mode.Space)}
        };
    }

    inline void from_json(const nlohmann::json& j, GUIRenderMode& mode) {
        mode.Space = static_cast<GUIRenderSpace>(j.value("Space", static_cast<uint8_t>(GUIRenderSpace::Screen)));
    }

    inline Vector2D LegacyAnchorFromAlignment(GUIAlignment alignment) {
        switch (alignment) {
        case GUIAlignment::Top:
            return { 0.5f, 0.0f };
        case GUIAlignment::TopRight:
            return { 1.0f, 0.0f };
        case GUIAlignment::Left:
            return { 0.0f, 0.5f };
        case GUIAlignment::Center:
            return { 0.5f, 0.5f };
        case GUIAlignment::Right:
            return { 1.0f, 0.5f };
        case GUIAlignment::BottomLeft:
            return { 0.0f, 1.0f };
        case GUIAlignment::Bottom:
            return { 0.5f, 1.0f };
        case GUIAlignment::BottomRight:
            return { 1.0f, 1.0f };
        case GUIAlignment::TopLeft:
        default:
            return { 0.0f, 0.0f };
        }
    }
    inline void to_json(nlohmann::json& j, const GUIElement& element) {
        j = nlohmann::json{
            {"Position", element.Position},
            {"Size", element.Size},
            {"Visible", element.Visible},
            {"Alignment", static_cast<uint8_t>(element.Alignment)},
            {"ZOrder", element.ZOrder},
            {"Margin", element.Margin},
            {"Padding", element.Padding},
            {"AnchorMin", element.AnchorMin},
            {"AnchorMax", element.AnchorMax}
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
        if (j.contains("AnchorMin")) {
            element.AnchorMin = j.at("AnchorMin").get<Vector2D>();
        }
        if (j.contains("AnchorMax")) {
            element.AnchorMax = j.at("AnchorMax").get<Vector2D>();
        }
        if (!j.contains("AnchorMin") && !j.contains("AnchorMax")) {
            const Vector2D legacy = LegacyAnchorFromAlignment(element.Alignment);
            element.AnchorMin = legacy;
            element.AnchorMax = legacy;
        }
        else if (j.contains("AnchorMin") && !j.contains("AnchorMax")) {
            element.AnchorMax = element.AnchorMin;
        }
        else if (!j.contains("AnchorMin") && j.contains("AnchorMax")) {
            element.AnchorMin = element.AnchorMax;
        }
    }

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GUIPanel, Color, CornerRadius)
    inline void to_json(nlohmann::json& j, const GUIText& text) {
        const std::string value = ECS::StringTable::Resolve(text.TextId);
        std::string fontPath = ECS::StringTable::Resolve(text.FontPathId);
        fontPath = NormalizeProjectPathForStorage(fontPath);
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
        std::string fontPath = j.value("FontPath", "");
        fontPath = NormalizeProjectPathForStorage(fontPath);
        text.TextId = value.empty() ? 0 : ECS::StringTable::Intern(value);
        text.FontPathId = fontPath.empty() ? 0 : ECS::StringTable::Intern(fontPath);
        if (j.contains("Color")) {
            text.Color = j.at("Color").get<::Color>();
        }
        if (j.contains("FontSize")) {
            text.FontSize = j.at("FontSize").get<float>();
        }
        else {
            text.FontSize = j.value("PixelSize", 24.0f);
        }
        text.Wrap = j.value("Wrap", false);
        text.HAlign = static_cast<GUIText::HorizontalAlign>(j.value("HAlign", 0));
        text.VAlign = static_cast<GUIText::VerticalAlign>(j.value("VAlign", 0));
    }

        inline void to_json(nlohmann::json& j, const GUIImage& image) {
            j = nlohmann::json{
                {"TexturePath", ResolvePathId(image.TexturePathId)},
                {"Color", image.Color},
                {"UVRect", image.UVRect},
                {"ScaleMode", static_cast<uint8_t>(image.ScaleMode)},
                {"TextureFilter", static_cast<uint8_t>(image.TextureFilter)},
                {"UseSlicing", image.UseSlicing},
                {"SliceBorder", image.SliceBorder}
            };
        }

    inline void from_json(const nlohmann::json& j, GUIImage& image) {
        image.TexturePathId = ReadPathId(j, "TexturePath", 0);
        if (j.contains("Color")) {
            image.Color = j.at("Color").get<::Color>();
        }
        if (j.contains("UVRect")) {
            image.UVRect = j.at("UVRect").get<Vector4D>();
        }
            image.ScaleMode = static_cast<GUIImageScaleMode>(j.value("ScaleMode", static_cast<uint8_t>(GUIImageScaleMode::Stretch)));
            image.TextureFilter = static_cast<Graphics::TextureFilter>(
                j.value("TextureFilter", static_cast<uint8_t>(Graphics::TextureFilter::Nearest)));
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
            {"FontPath", ResolvePathId(button.FontPathId)},
            {"IconPath", ResolvePathId(button.IconPathId)},
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
        button.FontPathId = ReadPathId(j, "FontPath", 0);
        button.IconPathId = ReadPathId(j, "IconPath", 0);
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

        // Return the singleton component serializer registry keyed by component hash
        static std::unordered_map<uint32_t, ComponentInfo>& Registry() {
            static std::unordered_map<uint32_t, ComponentInfo> reg;
            return reg;
        }

        // Register serializer callbacks for one concrete component type
        template<typename T>
        static void RegisterComponent(const char* name) {
            const uint32_t typeHash = Serialization::FNV1aHash(name);

            // Capture the concrete component id so serialization does not depend on hash registration order
            const ECS::ComponentTypeId componentId = ECS::ComponentRegistry::Type<T>();

            // Special cases: For prefab metadata, always use the captured component ID
            const bool useCapturedIdOnly = (name &&
                (std::strcmp(name, "PrefabInstanceMetadata") == 0 || std::strcmp(name, "PrefabLink") == 0));

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

        // Serialize one entity, including registered native components and discovered managed components
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
                }
                else {
                    json compJson = json::object();
                    entityJson["Components"].push_back({{"TypeName", componentName}, {"Data", compJson}});
                }
            }

            return entityJson;
        }

        // Deserialize one entity and rebuild native and managed component state from JSON
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

            // Ensure every entity has a Layer so editor picking and rendering remain functional
            if (!world.Has<ECS::Components::Layer>(e)) {
                world.Set<ECS::Components::Layer>(e, ECS::Components::Layer{ 0 });
            }

            // Ensure every entity has Active so enable/disable state is explicit and editable
            if (!world.Has<ECS::Components::Active>(e)) {
                world.Set<ECS::Components::Active>(e, ECS::Components::Active{ true });
            }
            return e;
        }

        // Serialize one entity and recursively append all children beneath it
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
                // For each one
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

        // Deserialize one entity hierarchy and reattach parent-child relationships
        static ECS::Entity DeserializeEntityHierarchy(ECS::World& world, const json& entityJson, EntityId parentId = ECS::Entity::NPOS32) {
            // Create this entity from JSON
            ECS::Entity entity = DeserializeEntity(world, entityJson);

            // Creation failed
            if (entity.IsNull() || !world.IsAlive(entity)) {
                return ECS::NULL_ENTITY;
            }

            // If this entity has a parent
            if (parentId != ECS::Entity::NPOS32) {
                // Connect this entity to its parent in the hierarchy
                ECS::Entity parent = world.Resolve(parentId);
                if (!parent.IsNull() && world.IsAlive(parent)) {
                    world.Attach(entity, parent);
                }
            }

            // If this entity has children in the JSON
            if (entityJson.contains("Children") && entityJson["Children"].is_array()) {
                // Recursively call itself to create each child
                // Pass THIS entity's ID as the parent
                for (const auto& childJson : entityJson["Children"]) {
                    DeserializeEntityHierarchy(world, childJson, entity.Index);
                }
            }

            return entity;
        }
    };

    // Register all component serializers
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
    REGISTER_COMPONENT_SERIALIZER(GUIRenderMode, ECS::Components::GUIRenderMode, "GUIRenderMode")
    REGISTER_COMPONENT_SERIALIZER(GUIElement, ECS::Components::GUIElement, "GUIElement")
    REGISTER_COMPONENT_SERIALIZER(GUIPanel, ECS::Components::GUIPanel, "GUIPanel");
    REGISTER_COMPONENT_SERIALIZER(GUIText, ECS::Components::GUIText, "GUIText");
    REGISTER_COMPONENT_SERIALIZER(GUIImage, ECS::Components::GUIImage, "GUIImage");
    REGISTER_COMPONENT_SERIALIZER(GUIInput, ECS::Components::GUIInput, "GUIInput");
    REGISTER_COMPONENT_SERIALIZER(GUIStateStyle, ECS::Components::GUIStateStyle, "GUIStateStyle");
    REGISTER_COMPONENT_SERIALIZER(GUIButton, ECS::Components::GUIButton, "GUIButton");
    REGISTER_COMPONENT_SERIALIZER(GUISlider, ECS::Components::GUISlider, "GUISlider");
    REGISTER_COMPONENT_SERIALIZER(BoidFlock, ECS::Components::BoidFlock, "BoidFlock");
    REGISTER_COMPONENT_SERIALIZER(ParticleEmitter, ECS::Components::ParticleEmitter, "ParticleEmitter");
}

#endif
