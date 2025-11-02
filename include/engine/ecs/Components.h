#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "math/Vector2D.h"
#include "ecs/IComponent.h"
#include <nlohmann/json.hpp>
#include <string>
#include "Color.h"
#include "graphics/texture.hpp"
#include "graphics/SpriteMetaData.hpp"
// #include "graphics/TextureCache.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>
#include "services/ResourceManager.h"

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


namespace Component {
    struct Transform : IComponent {
        Vector2D Position {0, 0};
        float Rotation = 0;
        Vector2D Scale{ 1, 1};

        Transform(const float x = 0, const float y = 0, const float rotation = 0, const float scaleX = 1.f, const float scaleY = 1.f)
            : Position({ x, y }), Rotation(rotation), Scale({ scaleX, scaleY }) {}

        const char* GetTypeName() const override { return "Transform"; }
    };

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

	// TODO: SpriteShapeRenderer for splining shapes

    struct ShapeRenderer2D final : IComponent {
        enum class ShapeType : std::uint8_t { Rectangle, Circle, Polygon };

        ShapeType Type = ShapeType::Rectangle;

        // General properties
        Color FillColor{ 1.f, 1.f, 1.f, 1.f };
        Color OutlineColor{ 0.f, 0.f, 0.f, 1.f };
        float OutlineThickness = 1.f;

        // Shape-specific data
        Vector2D Size{ 100.f, 100.f };      // For rectangle
        float Radius = 50.f;                      // For circle
        std::vector<Vector2D> Points;             // For polygon
        bool Closed = true;                       // For polygons

        // Constructors
        ShapeRenderer2D() = default;

        static ShapeRenderer2D Rectangle(const Vector2D& size, const Color& fill = { 1.f,1.f,1.f,1.f }) {
            ShapeRenderer2D s;
            s.Type = ShapeType::Rectangle;
            s.Size = size;
            s.FillColor = fill;
            return s;
        }

        static ShapeRenderer2D Circle(const float radius, const Color& fill = { 1.f,1.f,1.f,1.f }) {
            ShapeRenderer2D s;
            s.Type = ShapeType::Circle;
            s.Radius = radius;
            s.FillColor = fill;
            return s;
        }

        static ShapeRenderer2D Polygon(const std::vector<Vector2D>& points, const Color& fill = { 1.f,1.f,1.f,1.f }, const bool closed = true) {
            ShapeRenderer2D s;
            s.Type = ShapeType::Polygon;
            s.Points = points;
            s.FillColor = fill;
            s.Closed = closed;
            return s;
        }

        const char* GetTypeName() const override { return "ShapeRenderer2D"; }
    };

    struct Rigidbody2D : IComponent {
        Vector2D LinearVelocity{ 0, 0 };
             float Inertia = 0,
    		  AngularVelocity = 0,
			  AngularDamping = 0.05f,
			  LinearDamping = 0;
    	Vector2D CenterOfMass{ 0, 0 };
        float Mass = 1.0f;
        float GravityScale = 1.0f;
        bool FreezeRotation = false;

        enum BodyType { Dynamic = 0, Kinematic = 1, Static = 2 };
        BodyType BodyType = Dynamic;

        Rigidbody2D(const float mass = 1.0f) : Mass(mass) {}

        const char* GetTypeName() const override { return "Rigidbody2D"; }
    };

    struct Collider2D : IComponent {
        bool IsTrigger = false;
        Vector2D Offset{ 0, 0 };           // Offset from transform position
        int Layer = 0;                          // Physics layer

        Collider2D() = default;

        const char* GetTypeName() const override { return "Collider2D"; }
    };

    struct BoxCollider2D : Collider2D {
        Vector2D Size{ 1.0f, 1.0f };       // Box dimensions

        BoxCollider2D(const float width = 1.f, const float height = 1.f) : Size(width, height) {}

        const char* GetTypeName() const override { return "BoxCollider2D"; }
    };

    struct CircleCollider2D : Collider2D {
        float Radius = 0.5f;

        CircleCollider2D(const float radius = 0.5f) : Radius(radius) {}

        const char* GetTypeName() const override { return "CircleCollider2D"; }
    };

    // Line renderer for static geometry
    struct LineRenderer : IComponent {
        Vector2D Start{ 0, 0 };
        Vector2D End{ 0, 0 };
        float Thickness = 1.f;
        Color Color{ 1.f, 1.f, 1.f, 1.f };

        LineRenderer(const Vector2D& start = { 0,0 }, const Vector2D& end = { 0,0 }, const float thickness = 1.f)
            : Start(start), End(end), Thickness(thickness) {}

        const char* GetTypeName() const override { return "LineRenderer"; }
    };

    // Stores reference to prefab file that entity was originally instantiated from
    // We do this so the editor can trace the entity back to its source prefab asset
    struct PrefabLink : IComponent {
        std::string prefabPath;  // Path to the prefab file this entity was created from

        PrefabLink() = default;
        PrefabLink(const std::string& path) : prefabPath(path) {}

        const char* GetTypeName() const override { return "PrefabLink"; }
    };
}

#endif
