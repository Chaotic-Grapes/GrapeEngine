#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "IComponent.h"
#include <string>

namespace Component {
    // TODO: Replace with a math library vector type
    struct Transform : IComponent {
        float X = 0, Y = 0;
        float Rotation = 0;
		float ScaleX = 1, ScaleY = 1;

        explicit Transform(const float x = 0, const float y = 0, const float rotation = 0, const float scaleX = 1.f, const float scaleY = 1.f)
			: X(x), Y(y), Rotation(rotation), ScaleX(scaleX), ScaleY(scaleY) { }

        json Serialize()const override {
            return {
                {"X", X},
                {"Y", Y},
                {"Rotation", Rotation},
                {"ScaleX", ScaleX},
                {"ScaleY", ScaleY}
            };
        }

        void Deserialize(const json& data) override {
            X = data.value("X", 0.0f);
            Y = data.value("Y", 0.0f);
            Rotation = data.value("Rotation", 0.0f);
            ScaleX = data.value("ScaleX", 1.0f);
            ScaleY = data.value("ScaleY", 1.0f);
        }

        const char* GetTypeName() const override { return "Transform"; }
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

        json Serialize() const override {
            return {
                {"Sprite", Sprite},
                {"FlipX", FlipX},
                {"FlipY", FlipY},
                {"SortingOrder", SortingOrder},
                {"SortingLayerName", SortingLayerName}
            };
        }

        void Deserialize (const json& data) override {
            Sprite = data.value("Sprite", "");
            FlipX = data.value("FlipX", false);
            FlipY = data.value("FlipY", false);
            SortingOrder = data.value("SortingOrder", 0);
            SortingLayerName = data.value("SortingLayerName", "Default");
        }

        const char* GetTypeName() const override { return "SpriteRenderer"; }
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

        json Serialize() const override {
            return {
                {"AngularVelocity", AngularVelocity},
                {"Mass", Mass},
                {"Drag", Drag},
                {"AngularDrag", AngularDrag},
                {"GravityScale", GravityScale},
                {"FreezeRotation", FreezeRotation},
                {"IsKinematic", IsKinematic},
                {"BodyType", static_cast<int>(BodyType)}
            };
        }

        void Deserialize(const json& data) override {
            AngularVelocity = data.value("AngularVelocity", 0.0f);
            Mass = data.value("Mass", 1.0f);
            Drag = data.value("Drag", 0.0f);
            AngularDrag = data.value("AngularDrag", 0.05f);
            GravityScale = data.value("GravityScale", 1.0f);
            FreezeRotation = data.value("FreezeRotation", false);
            IsKinematic = data.value("IsKinematic", false);
            BodyType = static_cast<enum BodyType>(data.value("BodyType", 0));
        }

        const char* GetTypeName() const override { return "RigidBody2D"; }

        // to add serialization to other components as well if needed
    };

    struct Collider2D : IComponent {
        bool IsTrigger = false;
        // Vector2 Offset{ 0, 0 };           // Offset from transform position
        int Layer = 0;                  // Physics layer

        Collider2D() = default;

        json Serialize() const override {
            return {
                {"IsTrigger", IsTrigger},
                {"Layer", Layer}
            };
        }

        void Deserialize(const json& data) override {
            IsTrigger = data.value("IsTrigger", false);
            Layer = data.value("Layer", 0);
        }

        const char* GetTypeName() const override { return "Collider2D"; }
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
