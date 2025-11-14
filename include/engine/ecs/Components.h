/* Start Header *****************************************************************/
/*!
\file    Components.h
\author  Muhammad Nur Fadzly Bin Zulkifli (~90%), Choi Meng Yew (~10%)
\par     muhammadnurfadzly.b@digipen.edu, choi.m@digipen.edu
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

        // Lightweight name (fixed-size).
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
        };
        static_assert(std::is_trivially_copyable_v<Active>, "Active must be trivially copyable");

        // Tracks source prefab asset for entity instantiation and editor re-import workflows
        struct PrefabLink {
        public:
            // Fixed size buffer for prefab asset path to maintain 
            // trivially copyable status for ECS performance
            static constexpr size_t MaxPathLength = 256;
            char prefabPath[MaxPathLength] = { 0 };     // Path to the prefab file this entity was created from
            PrefabLink() = default;

            // Construct from std::string
            PrefabLink(const std::string& path) { setPath(path); }

            // Safe string copy with null termination guarantee
            void setPath(const std::string& path) {
                strncpy_s(prefabPath, path.c_str(), MaxPathLength - 1);
                prefabPath[MaxPathLength - 1] = '\0'; // Always null-terminate
            }
            // Convert back to std::string for convenience
            std::string getPath() const { return std::string(prefabPath); }
        };
        static_assert(std::is_trivially_copyable_v<PrefabLink>, "PrefabLink must be trivially copyable");

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

        struct Rotator {
            float RotationSpeed;   // degrees per second
            float RotationOffset;  // starting phase
        };
        static_assert(std::is_trivially_copyable_v<Rotator>, "LocalTransform must be trivially copyable");

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
        };
        static_assert(std::is_trivially_copyable_v<Rigidbody>, "Rigidbody must be trivially copyable");

        struct PhysicsMaterial2D {
        public:
            float Friction = 0.2f;               // 0..1
            float Restitution = 0.0f;            // 0..1 (bounciness)
			float PositionCorrectPercent = 0.2f; // 0..1
        };
        static_assert(std::is_trivially_copyable_v<PhysicsMaterial2D>, "PhysicsMaterial2D must be trivially copyable");

        struct BoxCollider {
        public:
            Vector3D HalfExtents{0.5f, 0.5f, 0.5f};
            uint32_t LayerMask = 0xFFFFFFFFu;
        };
        static_assert(std::is_trivially_copyable_v<BoxCollider>, "BoxCollider must be trivially copyable");

        struct SphereCollider {
        public:
            float Radius = 0.5f;
            uint32_t LayerMask = 0xFFFFFFFFu;
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
            int Width = 0;
            int Height = 0;
        };
        static_assert(std::is_trivially_copyable_v<SpriteRenderer2D>, "SpriteRenderer2D must be trivially copyable");
        

        // Optional: sprite flipping flags for atlases
        struct SpriteFlip2D {
        public:
            bool FlipX = false;
            bool FlipY = false;
        };
        static_assert(std::is_trivially_copyable_v<SpriteFlip2D>, "SpriteFlip2D must be trivially copyable");

        // Optional: sprite shader options
        struct SpriteShader2D {
        public:
            bool Bloom = false;
        };

        // TODO: Add Shader components

        // ---------------------------------- Animation ----------------------------------

        // Sprite sheet animation configuration (POD)
        struct SpriteSheetAnimation2D {
        public:
            uint32_t TextureId = 0;           // Texture containing the sprite sheet
            int FrameWidth = 0;               // Width of a single frame in pixels
            int FrameHeight = 0;              // Height of a single frame in pixels
            int SheetWidth = 0;               // Total width of the sprite sheet
            int SheetHeight = 0;              // Total height of the sprite sheet
            int StartFrame = 0;               // First frame index in the animation
            int FrameCount = 0;               // Number of frames in the animation
            float FramesPerSecond = 10.0f;    // Animation speed (FPS)
            bool Loop = true;                 // Whether animation loops
            bool Playing = true;              // Whether animation is currently playing
        };
        static_assert(std::is_trivially_copyable_v<SpriteSheetAnimation2D>, "SpriteSheetAnimation2D must be trivially copyable");

        // Animation state (runtime data, updated by AnimationSystem)
        struct AnimationState2D {
        public:
            int CurrentFrame = 0;             // Current frame index (relative to StartFrame)
            float TimeAccumulator = 0.0f;     // Time accumulated since last frame change
            bool Finished = false;            // True if non-looping animation completed
        };
        static_assert(std::is_trivially_copyable_v<AnimationState2D>, "AnimationState2D must be trivially copyable");

        // ---------- Minimal 2D shape data for debug rendering ----------
        // Keep these POD to be fast and compatible with archetype moves.

        struct ShapeCircle2D {
        public:
            float Radius = 0.5f;
            Vector2D Offset{0.0f, 0.0f}; // local offset
            Color Color{1.f,1.f,1.f,1.f};
            float Thickness = 1.0f;      // for wireframe; ignored if Filled
            bool Filled = false;
        };
        static_assert(std::is_trivially_copyable_v<ShapeCircle2D>, "ShapeCircle2D must be trivially copyable");

        struct ShapeBox2D {
        public:
            Vector2D HalfExtents{0.5f, 0.5f};
            Vector2D Offset{0.0f, 0.0f};
            Color Color{1.f,1.f,1.f,1.f};
            float Thickness = 1.0f;
            bool Filled = false;
        };
        static_assert(std::is_trivially_copyable_v<ShapeBox2D>, "ShapeBox2D must be trivially copyable");

        struct ShapeLine2D {
        public:
            Vector2D A{ 0.0f, 0.0f };     // local-space endpoints
            Vector2D B{ 1.0f, 0.0f };
            Color Color{ 1.f,1.f,1.f,1.f };
            float Thickness = 1.0f;
        };
        static_assert(std::is_trivially_copyable_v<ShapeLine2D>, "ShapeLine2D must be trivially copyable");

        // Fixed-capacity polyline/polygon for debug; avoids heap
        // TODO: Change this to something more flexible AND without class template
        template<size_t TCapacity = 16>
        struct ShapePolygon2D {
        public:
            Vector2D Points[TCapacity]{}; // don't use std::aray<T>
            uint8_t Count = 0;          // number of valid points
            Color FillColor{ 1.f, 1.f, 1.f, 1.f };
            Color OutlineColor{ 1.f, 1.f, 1.f, 1.f };
            float OutlineThickness = 1.f;
        };
        static_assert(std::is_trivially_copyable_v<ShapePolygon2D<>>, "ShapePolygon2D must be trivially copyable");

        // This is the Z-order for 2D rendering; lower values drawn first
        // Can be used with Layer component
        struct ZIndex2D {
        public:
            int16_t ZOrder = 0;  // smaller drawn first
        };
        static_assert(std::is_trivially_copyable_v<ZIndex2D>, "ZIndex2D must be trivially copyable");
        
        // ---------- Cameras ----------

        struct Camera3D {
        public: 
            bool UsePerspective = false;
            float FOV           = 45.f; // Don't use glm functions outside of graphics
            float NearPlane     = 0.1f;
            float FarPlane      = 100.f;
            float OrthoSize     = 10.f;
            float AspectRatio   = 16.f / 9.f; // width / height
            bool  Active        = false;
        };
        static_assert(std::is_trivially_copyable_v<Camera3D>, "Camera3D must be trivially copyable");

        // Optional matrices output for cameras (computed by CameraSystem)
        struct CameraMatrices {
        public:
            Matrix4x4 View{};
            Matrix4x4 Projection{};
            Matrix4x4 ViewProjection{};
        };
        static_assert(std::is_trivially_copyable_v<CameraMatrices>, "CameraMatrices must be trivially copyable");

        struct Light2D {
        public:
            enum class Type : uint8_t {
                Directional = 0,
                Point = 1
            };

            Type      LightType = Type::Directional;    // defaults to directional
            Vector3D  Position{ 0.f, 0.f, 0.f };        // used if Point
            Vector3D  Direction{ 0.f, -1.f, 0.f };      // used if Directional
            Color     Color{ 1.f, 1.f, 1.f, 1.f };      // RGB intensity
            float     Intensity = 1.0f;                 // brightness scalar
            float     Range = 10.0f;                    // used if Point
            bool      CastsShadows = false;             // for later extensions
        };
        static_assert(std::is_trivially_copyable_v<Light2D>, "Light2D must be trivially copyable");

        enum class TextAnchor : uint8_t {
            Absolute = 0,    ///< Position is absolute pixels (no anchoring)
            TopLeft,         ///< Offset from top-left corner
            TopRight,        ///< Offset from top-right corner
            BottomLeft,      ///< Offset from bottom-left corner
            BottomRight,     ///< Offset from bottom-right corner
            Center           ///< Offset from screen center
        };

        struct Text {
            static constexpr size_t MaxTextLength = 256;
            char Content[MaxTextLength] = {};   // Text to display
            char FontPath[128] = {};            // Initialize to empty, not with default path
            float PixelSize = 24.0f;
            Color Color = { 1.0f, 1.0f, 1.0f, 1.0f };
            TextAnchor Anchor = TextAnchor::Absolute;

            Text() {
                // Default constructor sets default font
                setFontPath("assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf");
            }

            Text(const char* content,
                float pixelSize = 24.0f,
                const ::Color& color = { 1.0f, 1.0f, 1.0f, 1.0f },
                TextAnchor anchor = TextAnchor::Absolute,
                const char* fontPath = "assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf")
                : PixelSize(pixelSize)
                , Color(color)
                , Anchor(anchor)
            {
                setContent(content);
                setFontPath(fontPath);  // this initializes the empty array
            }

            void setContent(const char* str) {
                if (str) {
                    strncpy_s(Content, str, MaxTextLength - 1);
                    Content[MaxTextLength - 1] = '\0';
                }
            }

            void setFontPath(const char* path) {
                if (path) {
                    strncpy_s(FontPath, path, 127);
                    FontPath[127] = '\0';
                }
            }

            std::string_view getContent() const { return { Content }; }
            std::string_view getFontPath() const { return { FontPath }; }
        };
        static_assert(std::is_trivially_copyable_v<Text>, "Text must be trivially copyable");

        struct UIButton {
            int ID;                     // Unique button identifier
            float X, Y, W, H;          // Screen-space coordinates (pixels)
            bool Hovered;
            bool Pressed;
            uint32_t ActionID;         // Maps to action in registry
        };
        static_assert(std::is_trivially_copyable_v<UIButton>, "UIButton must be trivially copyable");

        // ---------- Scripting / Audio (kept minimal) ----------

        // C# Script instance component for CoreCLR hosting
        struct ScriptInstance {
        public:
            uint64_t ManagedHandle = 0;   // Handle to C# object instance
            uint32_t TypeHash = 0;        // Hash of script type name
            bool Initialized = false;     // Whether OnStart() has been called
            char TypeName[128] = {0};     // Script class name (e.g., "MyGame.PlayerController")
        };
        static_assert(std::is_trivially_copyable_v<ScriptInstance>, "ScriptInstance must be trivially copyable");

        struct AudioSource {
        public:
            uint32_t CueId = 0;
            float Volume = 1.0f;
            float Pitch = 1.0f;
            bool Loop = false;
            bool PlayOnStart = false;
            bool Spatial3D = true;
        };
        static_assert(std::is_trivially_copyable_v<AudioSource>, "AudioSource must be trivially copyable");
    }
}


#endif
