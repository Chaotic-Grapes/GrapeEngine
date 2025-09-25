#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "ecs/IComponent.h"
#include <string>
#include "Math/Vector2D.h"
#include "Color.h"
#include "graphics/texture.hpp"
#include <vector>

namespace Component {
    // TODO: Replace with a math library vector type
    struct Transform : IComponent {
        Vector2D Position {0, 0};
        float Rotation = 0;
        Vector2D Scale{ 1, 1};

        Transform(const float x = 0, const float y = 0, const float rotation = 0, const float scaleX = 1.f, const float scaleY = 1.f)
            : Position({ x, y }), Rotation(rotation), Scale({ scaleX, scaleY }) {}
    };

    struct SpriteRenderer : IComponent {
        Texture Source;
        Color Color{ 1.f, 1.f, 1.f, 1.f };
        bool FlipX = false;
        bool FlipY = false;
        int SortingOrder = 0;           // Rendering layer order
        std::string SortingLayerName = "Default";

        SpriteRenderer(const std::string& spritePath = "") : Source(spritePath) {}
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
    };

    struct Rigidbody2D : IComponent {
        Vector2D Velocity{ 0, 0 };
        float AngularVelocity = 0.0f;
        float Mass = 1.0f;
        float Drag = 0.0f;              // Linear drag
        float AngularDrag = 0.05f;
        float GravityScale = 1.0f;
        bool FreezeRotation = false;

        enum BodyType { Dynamic = 0, Kinematic = 1, Static = 2 };
        BodyType BodyType = Dynamic;

        Rigidbody2D(const float mass = 1.0f) : Mass(mass) {}
    };

    struct Collider2D : IComponent {
        bool IsTrigger = false;
        Vector2D Offset{ 0, 0 };           // Offset from transform position
        int Layer = 0;                          // Physics layer

        Collider2D() = default;
    };

    struct BoxCollider2D : Collider2D {
        Vector2D Size{ 1.0f, 1.0f };       // Box dimensions

        BoxCollider2D(const float width = 1.f, const float height = 1.f) : Size(width, height) {}
    };

    struct CircleCollider2D : Collider2D {
        float Radius = 0.5f;

        CircleCollider2D(const float radius = 0.5f) : Radius(radius) {}
    };

    // Line renderer for static geometry
    struct LineRenderer : IComponent {
        Vector2D Start{ 0, 0 };
        Vector2D End{ 0, 0 };
        float Thickness = 1.f;
        Color Color{ 1.f, 1.f, 1.f, 1.f };

        LineRenderer(const Vector2D& start = { 0,0 }, const Vector2D& end = { 0,0 }, const float thickness = 1.f)
            : Start(start), End(end), Thickness(thickness) {}
    };
}

#endif
