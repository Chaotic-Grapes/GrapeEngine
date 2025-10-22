/* Start Header *****************************************************************/
/*!
\file    Components.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of various ECS components used in the engine.
These components are plain data structures that can be attached to entities
to define their properties and behaviors. Several components include padding bytes
to ensure proper alignment and maintain trivially copyable status. Furthermore,
component serialization is handled centrally in `EntitySerializer.h`. Components
should be registered there for correct JSON (de)serialization.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "Color.h"
#include "math/Vector2D.h"
#include "math/Vector3D.h"
#include "math/Vector4D.h"
#include "math/Quaternion.h"
#include "math/Matrix4x4.h"
#include <nlohmann/json.hpp>

/*
================================================================================
NOTE FOR DEVELOPERS:
--------------------------------------------------------------------------------
Component serialization for the ECS is handled centrally in
`include/engine/serialization/EntitySerializer.h`.

Key rules you must follow when adding or changing Components:
    - All `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` macros for component types and
        nested POD types (e.g. `Vector2D`, `Color`, `Matrix4x4`) live in
        `EntitySerializer.h`. Do NOT add those macros to `Components.h`.
    - After adding a new component struct (or changing its member list), add or
        update its corresponding `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` entry in
        `EntitySerializer.h` so JSON (de)serialisation remains correct.
    - Register the component with the serializer registry by adding a
        `REGISTER_COMPONENT_SERIALIZER(<ShortName>, ECS::Components::<Type>)`
        invocation in `EntitySerializer.h`. The `REGISTER_COMPONENT_SERIALIZER`
        macro creates a static registration that maps the engine TypeId to the
        (de)serialisation callbacks used when persisting entities.
    - Template/component containers (e.g. `ShapePolygon2D<TCapacity>`) cannot
        be registered directly — provide a concrete typedef or custom handling in
        `EntitySerializer.h` if you need to serialise them.

Minimal example:
    // In Components.h
    struct MyComponent {
            int Value = 0;
            float Factor = 1.0f;
    };

    // In EntitySerializer.h
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ECS::Components::MyComponent, Value, Factor)
    REGISTER_COMPONENT_SERIALIZER(MyComponent, ECS::Components::MyComponent)

Notes:
    - Keep nested POD types serialisable by defining their macros in
        `EntitySerializer.h` as well.
    - When adding/removing fields, update the matching `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`
        macro immediately to avoid silent (de)serialisation bugs.

This centralised approach keeps component headers lightweight and avoids
duplicate macro definitions across the codebase.
================================================================================
*/

/*
================================================================================
NOTE FOR DEVELOPERS:
--------------------------------------------------------------------------------
There are padding bytes in some components to ensure proper alignment and
trivially copyable status. These padding bytes are NOT meant to be used and
should be ignored in all logic.

Why is this important?
    - Trivially copyable types can be safely copied with `memcpy`, which is
      crucial for performance in an ECS architecture.
    - Proper alignment ensures that the CPU can access the data efficiently,
      avoiding potential performance penalties.

When adding new members to a component, please ensure that the overall size
remains a multiple of 4 bytes (the size of the largest primitive type).
This helps maintain alignment and performance characteristics.
================================================================================
*/

namespace ECS {
    namespace Components {
        // ---------------------------------- Core utility/tag components ----------------------------------

        // Lightweight name (fixed-size). Avoids std::string to remain trivially copyable.
        struct Name {
        public:
            // UTF-8 bytes, null-terminated if shorter than buffer. Keep small for cache.
            char Value[64] = {0};
        };
        static_assert(std::is_trivially_copyable_v<Name>, "Name must be trivially copyable");

        // Bitmask-based tag component (32 customizable tags).
        struct TagMask {
        public:
            uint32_t Mask = 0;
        };
        static_assert(std::is_trivially_copyable_v<TagMask>, "TagMask must be trivially copyable");

        // Enabled/disabled flag for quick filtering.
        struct Active {
        public:
            bool Enabled = true;
            // padding explicit to keep trivially copyable and predictable size
            // !!!! These are not meant to be used. !!!!
            uint8_t _Pad0 = 0, _Pad1 = 0, _Pad2 = 0;
        };
        static_assert(std::is_trivially_copyable_v<Active>, "Active must be trivially copyable");

        // Lifetime in seconds; entities with Time <= 0 can be culled by a system.
        struct Lifetime {
        public:
            float Time = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<Lifetime>, "Lifetime must be trivially copyable");

        // ---------------------------------- Layers and Transforms ----------------------------------
        // Layers: one component holding a small integer id per entity
        struct Layer { 
        public:
            uint16_t Id = 0; 
        };
        static_assert(std::is_trivially_copyable_v<Layer>, "Layer must be trivially copyable");

        // Local transform is relative to parent entity (if any)
        struct LocalTransform { 
        public:
            Vector3D Position{0,0,0};
            Quaternion Rotation{0,0,0,1.f};
            Vector3D Scale{1.f,1.f,1.f};
        };
        static_assert(std::is_trivially_copyable_v<LocalTransform>, "LocalTransform must be trivially copyable");

        // World transform is relative to world origin
        struct WorldTransform { 
        public:
            Matrix4x4 Matrix{};
            bool Dirty = true;
        };
        static_assert(std::is_trivially_copyable_v<WorldTransform>, "WorldTransform must be trivially copyable");

        // ---------------------------------- 3D kinematics/physics ----------------------------------

        // Kinematics
        struct Velocity {
        public:
            Vector3D Value{0.0f, 0.0f, 0.0f};
        };
        static_assert(std::is_trivially_copyable_v<Velocity>, "Velocity must be trivially copyable");

        struct Acceleration {
        public:
            Vector3D Value{0.0f, 0.0f, 0.0f};
        };
        static_assert(std::is_trivially_copyable_v<Acceleration>, "Acceleration must be trivially copyable");

        struct AngularVelocity {
        public:
            // Radians per second around local axes
            Vector3D Value{0.0f, 0.0f, 0.0f};
        };
        static_assert(std::is_trivially_copyable_v<AngularVelocity>, "AngularVelocity must be trivially copyable");

        // Rigidbody placeholder
        struct Rigidbody {
        public:
            float Mass = 1.0f;       // Mass <= 0 implies static
            float InverseMass = 1.0f;    // Precompute for speed
            float LinearDrag = 0.0f;
            float AngularDrag = 0.0f;
            uint32_t Flags = 0;      // bit 0: UseGravity, bit 1: Kinematic, etc.
            uint32_t _Pad = 0;
        };
        static_assert(std::is_trivially_copyable_v<Rigidbody>, "Rigidbody must be trivially copyable");

        struct PhysicsMaterial2D {
        public:
            float Friction = 0.2f;               // 0..1
            float Restitution = 0.0f;            // 0..1 (bounciness)
			float PositionCorrectPercent = 0.2f; // 0..1
            float _Pad0 = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<PhysicsMaterial2D>, "PhysicsMaterial2D must be trivially copyable");

        struct BoxCollider {
        public:
            Vector3D HalfExtents{0.5f, 0.5f, 0.5f};
            uint32_t LayerMask = 0xFFFFFFFFu;
            uint32_t _Pad = 0;
        };
        static_assert(std::is_trivially_copyable_v<BoxCollider>, "BoxCollider must be trivially copyable");

        struct SphereCollider {
        public:
            float Radius = 0.5f;
            float _Pad0 = 0.0f, _Pad1 = 0.0f, _Pad2 = 0.0f;
            uint32_t LayerMask = 0xFFFFFFFFu;
            uint32_t _Pad3 = 0;
        };
        static_assert(std::is_trivially_copyable_v<SphereCollider>, "SphereCollider must be trivially copyable");

        // ---------------------------------- 2D kinematics/physics ----------------------------------

        // 2D linear velocity for X/Y; systems should update LocalTransform.Position.X/Y
        // Velocity is the rate of change of position per second.
        struct LinearVelocity2D {
        public:
            Vector2D Value{0.0f, 0.0f};
        };
        static_assert(std::is_trivially_copyable_v<LinearVelocity2D>, "LinearVelocity2D must be trivially copyable");

        // 2D acceleration for X/Y
        // Acceleration is the rate of change of velocity per second.
        struct Acceleration2D {
        public:
            Vector2D Value{0.0f, 0.0f};
        };
        static_assert(std::is_trivially_copyable_v<Acceleration2D>, "Acceleration2D must be trivially copyable");

        // 2D angular velocity around Z axis (radians/sec); systems rotate LocalTransform.Rotation about Z
        struct AngularVelocity2D {
        public:
            float Value = 0.0f;
            float _Pad0 = 0.0f, _Pad1 = 0.0f, _Pad2 = 0.0f; // keep 16B size/alignment simple
        };
        static_assert(std::is_trivially_copyable_v<AngularVelocity2D>, "AngularVelocity2D must be trivially copyable");

        // 2D rigidbody
        struct Rigidbody2D {
        public:
            float Mass = 1.0f;          // Mass <= 0 => static
            float InverseMass = 1.0f;   // Precomputed
            float LinearDamping = 0.0f; // Damping per second
            float AngularDamping = 0.0f;
            float GravityScale = 1.0f;  // Scale world gravity
            uint32_t Flags = 0;         // bit 0: Kinematic, bit 1: UseGravity, bit 2: FixedRotation
        };
        static_assert(std::is_trivially_copyable_v<Rigidbody2D>, "Rigidbody2D must be trivially copyable");

        // Axis-aligned or oriented rectangle collider in 2D
        struct BoxCollider2D {
        public:
            Vector2D HalfExtents{0.5f, 0.5f}; // half-size
            Vector2D Offset{0.0f, 0.0f};      // local center offset
            float Rotation = 0.0f;            // local rotation in radians (around Z)
            uint32_t LayerMask = 0xFFFFFFFFu; // collision layer mask
            uint32_t Flags = 0;               // bit 0: IsTrigger
            uint32_t _Pad = 0;
        };
        static_assert(std::is_trivially_copyable_v<BoxCollider2D>, "BoxCollider2D must be trivially copyable");

        // Circle collider in 2D
        struct CircleCollider2D {
        public:
            float Radius = 0.5f;
            Vector2D Offset{0.0f, 0.0f};      // local center offset
            uint32_t LayerMask = 0xFFFFFFFFu; // collision layer mask
            uint32_t Flags = 0;               // bit 0: IsTrigger
        };
        static_assert(std::is_trivially_copyable_v<CircleCollider2D>, "CircleCollider2D must be trivially copyable");

        // ---------------------------------- Rendering ----------------------------------

        // 2D sprite renderer (for UI/2D layers)
        struct SpriteRenderer2D {   
        public:
            uint32_t TextureId = 0;
            Color Color{1.0f, 1.0f, 1.0f, 1.0f};
            Vector2D Tiling{1.0f, 1.0f};
            Vector2D Offset{0.0f, 0.0f};
            uint32_t _Pad = 0;          // keep 8-byte alignment
        };
        static_assert(std::is_trivially_copyable_v<SpriteRenderer2D>, "SpriteRenderer2D must be trivially copyable");
        
        // Optional: sprite flipping flags for atlases
        struct SpriteFlip2D {
        public:
            bool FlipX = false;
            bool FlipY = false;
            uint8_t _Pad0 = 0, _Pad1 = 0, _Pad2 = 0;
        };
        static_assert(std::is_trivially_copyable_v<SpriteFlip2D>, "SpriteFlip2D must be trivially copyable");

        // TODO: Add Shader components

        // ---------- Minimal 2D shape data for debug rendering ----------
        // Keep these POD to be fast and compatible with archetype moves.

        struct ShapeCircle2D {
        public:
            float Radius = 0.5f;
            Vector2D Offset{0.0f, 0.0f}; // local offset
            Color Color{1.f,1.f,1.f,1.f};
            float Thickness = 1.0f;      // for wireframe; ignored if Filled
            bool Filled = false;
            uint8_t _Pad0 = 0, _Pad1 = 0, _Pad2 = 0;
        };
        static_assert(std::is_trivially_copyable_v<ShapeCircle2D>, "ShapeCircle2D must be trivially copyable");

        struct ShapeBox2D {
        public:
            Vector2D HalfExtents{0.5f, 0.5f};
            Vector2D Offset{0.0f, 0.0f};
            Color Color{1.f,1.f,1.f,1.f};
            float Thickness = 1.0f;
            bool Filled = false;
            uint8_t _Pad0 = 0, _Pad1 = 0, _Pad2 = 0;
        };
        static_assert(std::is_trivially_copyable_v<ShapeBox2D>, "ShapeBox2D must be trivially copyable");

        struct ShapeLine2D {
        public:
            Vector2D A{ 0.0f, 0.0f };     // local-space endpoints
            Vector2D B{ 1.0f, 0.0f };
            Color Color{ 1.f,1.f,1.f,1.f };
            float Thickness = 1.0f;
            float _Pad0 = 0.0f, _Pad1 = 0.0f, _Pad2 = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<ShapeLine2D>, "ShapeLine2D must be trivially copyable");

        // Fixed-capacity polyline/polygon for debug; avoids heap
        template<size_t TCapacity = 16>
        struct ShapePolygon2D {
        public:
            Vector2D Points[TCapacity]{}; // don't use std::aray<T>
            uint8_t Count = 0;          // number of valid points
            Color FillColor{ 1.f, 1.f, 1.f, 1.f };
            Color OutlineColor{ 1.f, 1.f, 1.f, 1.f };
            float OutlineThickness = 1.f;
            uint8_t _Pad0 = 0, _Pad1 = 0, _Pad2 = 0;
        };
        static_assert(std::is_trivially_copyable_v<ShapePolygon2D<>>, "ShapePolygon2D must be trivially copyable");

        // Optional: simple 2D sorting hint for renderer (e.g. painter's algorithm)
        struct ZIndex2D {
        public:
            int16_t ZOrder = 0;  // smaller drawn first
            int16_t _Pad0 = 0;
            int32_t _Pad1 = 0;
        };
        static_assert(std::is_trivially_copyable_v<ZIndex2D>, "ZIndex2D must be trivially copyable");

        // ---------- Cameras ----------

        struct Camera {
        public:
            bool IsOrthographic = false;
            uint8_t _Pad0 = 0, _Pad1 = 0, _Pad2 = 0;

            float FovY = 60.0f;         // degrees (used when IsOrthographic == false)
            float OrthoHeight = 10.0f;  // world units half-height (used when IsOrthographic == true)
            float Near = 0.1f;
            float Far = 1000.0f;
            float Aspect = 16.0f / 9.0f; // width / height
        };
        static_assert(std::is_trivially_copyable_v<Camera>, "Camera must be trivially copyable");

        // Optional matrices output for cameras (computed by CameraSystem)
        struct CameraMatrices {
        public:
            Matrix4x4 View{};
            Matrix4x4 Projection{};
            Matrix4x4 ViewProjection{};
        };
        static_assert(std::is_trivially_copyable_v<CameraMatrices>, "CameraMatrices must be trivially copyable");

        // ---------- Scripting / Audio (kept minimal) ----------

        struct ScriptId {
        public:
            uint32_t Id = 0;
        };
        static_assert(std::is_trivially_copyable_v<ScriptId>, "ScriptId must be trivially copyable");

        // C# Script instance component for CoreCLR hosting
        struct ScriptInstance {
        public:
            uint64_t ManagedHandle = 0;   // Handle to C# object instance
            uint32_t TypeHash = 0;        // Hash of script type name
            bool Initialized = false;     // Whether OnStart() has been called
            uint8_t _Pad0 = 0, _Pad1 = 0, _Pad2 = 0;
            char TypeName[128] = {0};     // Script class name (e.g., "MyGame.PlayerController")
        };
        static_assert(std::is_trivially_copyable_v<ScriptInstance>, "ScriptInstance must be trivially copyable");

        struct AudioSource {
        public:
            uint32_t CueId = 0;
            uint32_t _Pad = 0;
            float Volume = 1.0f;
            float Pitch = 1.0f;
            bool Loop = false;
            uint8_t _Pad0 = 0, _Pad1 = 0, _Pad2 = 0;
        };
        static_assert(std::is_trivially_copyable_v<AudioSource>, "AudioSource must be trivially copyable");
    }
}


#endif
