#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "ecs/IComponent.h"
#include <string>

namespace Component {
    // TODO: Replace with a math library vector type
    struct Transform : IComponent {
        float X = 0, Y = 0;
        float Rotation = 0;
		float ScaleX = 1, ScaleY = 1;

        explicit Transform(const float x = 0, const float y = 0, const float rotation = 0, const float scaleX = 1.f, const float scaleY = 1.f)
			: X(x), Y(y), Rotation(rotation), ScaleX(scaleX), ScaleY(scaleY) { }
    };

    // TODO: Add Color struct
    struct SpriteRenderer : IComponent {
        std::string Sprite;
        // Color Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        bool FlipX = false;
        bool FlipY = false;
        int SortingOrder = 0;           // Rendering layer order
        std::string SortingLayerName = "Default";

        SpriteRenderer(const std::string& spritePath = "") : Sprite(spritePath) {}
    };

    struct Rigidbody2D : IComponent {
        // Vector2 Velocity{ 0, 0 };
        float AngularVelocity = 0.0f;
        float Mass = 1.0f;
        float Drag = 0.0f;              // Linear drag
        float AngularDrag = 0.05f;
        float GravityScale = 1.0f;
        bool FreezeRotation = false;
        bool IsKinematic = false;       // If true, not affected by physics forces

        // Body type enum like Unity
        enum BodyType { Dynamic = 0, Kinematic = 1, Static = 2 };
        BodyType BodyType = Dynamic;

        Rigidbody2D(float m = 1.0f) : Mass(m) {}
    };

    struct Collider2D : IComponent {
        bool IsTrigger = false;
        // Vector2 Offset{ 0, 0 };           // Offset from transform position
        int Layer = 0;                  // Physics layer

        Collider2D() = default;
    };

    struct BoxCollider2D : Collider2D {
        // Vector2 Size{ 1.0f, 1.0f };       // Box dimensions

        // BoxCollider2D(float width = 1.0f, float height = 1.0f) : Size(width, height) {}
    };

    struct CircleCollider2D : Collider2D {
        float Radius = 0.5f;

        CircleCollider2D(float r = 0.5f) : Radius(r) {}
    };
}

#endif
