#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "Color.h"
#include "ecs/Entity.h"
#include "ecs/IComponent.h"
#include "graphics/SpriteMetaData.hpp"
#include "graphics/texture.hpp"
#include "math/Vector2D.h"
#include "math/Vector3D.h"
#include "math/Vector4D.h"
#include "services/ResourceManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
// #include "graphics/TextureCache.hpp"

/*
================================================================================
NOTE FOR DEVELOPERS:
--------------------------------------------------------------------------------
This project uses **nlohmann::json** for serialisation of all ECS Components.

IMPORTANT CHANGES:
    - All `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` macros are now defined in
      `EntitySerializer.h` instead of the bottom of this file.
    - If you add a new Component struct, you MUST also add a corresponding macro
      in `EntitySerializer.h` so that the type can be serialised/deserialised
      properly.
    - Nested types (like `Vector2D`, `Color`, etc.) must also have their macros
      defined in `EntitySerializer.h` if they are used inside Components.
    - Likewise, if you add/remove a data member/property to a Component, you
      need to update the macro in `EntitySerializer.h`.

Example:
   struct MyComponent : IComponent {
       int Value = 0;
       std::string Name;
   };

   // In EntitySerializer.h:
   NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Component::MyComponent, Value, Name)

This ensures that `nlohmann::json` can automatically handle conversions like:
   json j = myComponent;                 // serialise
   MyComponent c = j.get<MyComponent>(); // deserialise
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

// TODO: Check if IComponent is really necessary for all components below
// Considerations: IComponent adds a vtable pointer, which may affect trivial copyability
// but provides a common interface for all components. Furthermore, it could increase
// memory usage per component instance as it adds overhead. Evaluate based on usage patterns.

// However, current implementation is good enough for now.
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
            Quaternion Rotation{0,0,0,1};
            Vector3D Scale{1,1,1};
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
            float Friction = 0.2f;
            float Restitution = 0.0f;
            float _Pad0 = 0.0f, _Pad1 = 0.0f;
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

        // Physics material for 2D colliders
        struct PhysicsMaterial2D {
        public:
            float Friction = 0.2f;      // 0..1
            float Restitution = 0.0f;   // 0..1 (bounciness)
            float _Pad0 = 0.0f, _Pad1 = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<PhysicsMaterial2D>, "PhysicsMaterial2D must be trivially copyable");

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
            Vector2D A{0.0f, 0.0f};     // local-space endpoints
            Vector2D B{1.0f, 0.0f};
            Color Color{1.f,1.f,1.f,1.f};
            float Thickness = 1.0f;
            float _Pad0 = 0.0f, _Pad1 = 0.0f, _Pad2 = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<ShapeLine2D>, "ShapeLine2D must be trivially copyable");

        // Fixed-capacity polyline/polygon for debug; avoids heap
        template<size_t TCapacity = 16>
        struct ShapePolyline2D {
        public:
            Vector2D Points[TCapacity]{};
            uint8_t Count = 0;          // number of valid points
            bool Closed = false;        // draw last->first if true
            uint8_t _Pad0 = 0;
            uint8_t _Pad1 = 0;
            Color Color{1,1,1,1};
            float Thickness = 1.0f;
            float _Pad2 = 0.0f, _Pad3 = 0.0f, _Pad4 = 0.0f;
        };
        static_assert(std::is_trivially_copyable_v<ShapePolyline2D<>>, "ShapePolyline2D must be trivially copyable");

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

// namespace Component {
//     struct Transform : IComponent {
//         Vector2D Position {0, 0};
//         float Rotation = 0;
//         Vector2D Scale{ 1, 1};

//         Transform(const float x = 0, const float y = 0, const float rotation = 0, const float scaleX = 1.f, const float scaleY = 1.f)
//             : Position({ x, y }), Rotation(rotation), Scale({ scaleX, scaleY }) {}

//         const char* GetTypeName() const override { return "Transform"; }
//     };

    struct SpriteRenderer : IComponent {
        GLuint TextureId = 0;
        int Width = 0;
        int Height = 0;
        const SpriteMetadata* Meta = nullptr;
        Color Color{ 1.f, 1.f, 1.f, 1.f };
        bool FlipX = false;
        bool FlipY = false;
        int SortingOrder = 0;
        std::string SortingLayerName = "Default";
        std::string TexturePath;
        std::string Sprite;

        SpriteRenderer(const std::string& spritePath = "") : TexturePath(spritePath), Sprite(spritePath) {
            if (!spritePath.empty()) {
                auto tex = RM.Get<Texture>(spritePath);
                TextureId = tex->ID();
                Width = tex->Width();
                Height = tex->Height();

                auto p = std::filesystem::path(spritePath);
                auto filename = p.stem().string() + ".json";

                auto parent = p.parent_path().parent_path(); // "assets/textures"
                auto metadataPath = parent / "test-metadata" / filename;

                std::ifstream in(metadataPath);
                if (in) {
                    nlohmann::json j;
                    in >> j;

                    auto key = p.filename().string(); // for example, "fishBoy.png"
                    if (j.contains(key)) {
                        static std::unordered_map<std::string, SpriteMetadata> cache;
                        cache[key] = loadSingleSpriteMetadata(j[key], 0, 0);
                        Meta = &cache[key];
                    }
                    else {
                        std::cout << "Metadata file " << metadataPath
                            << " missing entry for " << key << "\n";
                    }
                }
                else {
                    std::cout << "Could not open metadata file: "
                        << metadataPath << "\n";
                }
            }
        }

        const char* GetTypeName() const override { return "SpriteRenderer"; }
    };

// 	// TODO: SpriteShapeRenderer for splining shapes

//     struct ShapeRenderer2D final : IComponent {
//         enum class ShapeType : std::uint8_t { Rectangle, Circle, Polygon };

//         ShapeType Type = ShapeType::Rectangle;

//         // General properties
//         Color FillColor{ 1.f, 1.f, 1.f, 1.f };
//         Color OutlineColor{ 0.f, 0.f, 0.f, 1.f };
//         float OutlineThickness = 1.f;

//         // Shape-specific data
//         Vector2D Size{ 100.f, 100.f };      // For rectangle
//         float Radius = 50.f;                      // For circle
//         std::vector<Vector2D> Points;             // For polygon
//         bool Closed = true;                       // For polygons

//         // Constructors
//         ShapeRenderer2D() = default;

//         static ShapeRenderer2D Rectangle(const Vector2D& size, const Color& fill = { 1.f,1.f,1.f,1.f }) {
//             ShapeRenderer2D s;
//             s.Type = ShapeType::Rectangle;
//             s.Size = size;
//             s.FillColor = fill;
//             return s;
//         }

//         static ShapeRenderer2D Circle(const float radius, const Color& fill = { 1.f,1.f,1.f,1.f }) {
//             ShapeRenderer2D s;
//             s.Type = ShapeType::Circle;
//             s.Radius = radius;
//             s.FillColor = fill;
//             return s;
//         }

//         static ShapeRenderer2D Polygon(const std::vector<Vector2D>& points, const Color& fill = { 1.f,1.f,1.f,1.f }, const bool closed = true) {
//             ShapeRenderer2D s;
//             s.Type = ShapeType::Polygon;
//             s.Points = points;
//             s.FillColor = fill;
//             s.Closed = closed;
//             return s;
//         }

//         const char* GetTypeName() const override { return "ShapeRenderer2D"; }
//     };

//     struct Rigidbody2D : IComponent {
//         Vector2D LinearVelocity{ 0, 0 };
//              float Inertia = 0,
//     		  AngularVelocity = 0,
// 			  AngularDamping = 0.05f,
// 			  LinearDamping = 0;
//     	Vector2D CenterOfMass{ 0, 0 };
//         float Mass = 1.0f;
//         float GravityScale = 1.0f;
//         bool FreezeRotation = false;

//         enum BodyType { Dynamic = 0, Kinematic = 1, Static = 2 };
//         BodyType BodyType = Dynamic;

//         Rigidbody2D(const float mass = 1.0f) : Mass(mass) {}

//         const char* GetTypeName() const override { return "Rigidbody2D"; }
//     };

//     struct Collider2D : IComponent {
//         bool IsTrigger = false;
//         Vector2D Offset{ 0, 0 };           // Offset from transform position
//         int Layer = 0;                          // Physics layer

//         Collider2D() = default;

//         const char* GetTypeName() const override { return "Collider2D"; }
//     };

//     struct BoxCollider2D : Collider2D {
//         Vector2D Size{ 1.0f, 1.0f };       // Box dimensions

//         BoxCollider2D(const float width = 1.f, const float height = 1.f) : Size(width, height) {}

//         const char* GetTypeName() const override { return "BoxCollider2D"; }
//     };

//     struct CircleCollider2D : Collider2D {
//         float Radius = 0.5f;

//         CircleCollider2D(const float radius = 0.5f) : Radius(radius) {}

//         const char* GetTypeName() const override { return "CircleCollider2D"; }
//     };

//     // Line renderer for static geometry
//     struct LineRenderer : IComponent {
//         Vector2D Start{ 0, 0 };
//         Vector2D End{ 0, 0 };
//         float Thickness = 1.f;
//         Color Color{ 1.f, 1.f, 1.f, 1.f };

//         LineRenderer(const Vector2D& start = { 0,0 }, const Vector2D& end = { 0,0 }, const float thickness = 1.f)
//             : Start(start), End(end), Thickness(thickness) {}

//         const char* GetTypeName() const override { return "LineRenderer"; }
//     };
// }

#endif
