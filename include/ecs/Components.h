#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "Math/Vector2D.h"
#include "ecs/IComponent.h"
#include "../include/nlohmann/json.hpp"
#include <string>
#include "Color.h"
#include "graphics/texture.hpp"
#include "graphics/SpriteMetaData.hpp"
#include "../include/graphics/TextureCache.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>

namespace Component {
    // TODO: Replace with a math library vector type
    struct Transform : IComponent {
        Vector2D Position {0, 0};
        float Rotation = 0;
        Vector2D Scale{ 1, 1};

        Transform(const float x = 0, const float y = 0, const float rotation = 0, const float scaleX = 1.f, const float scaleY = 1.f)
            : Position({ x, y }), Rotation(rotation), Scale({ scaleX, scaleY }) {}

        // Add serialization methods
        json Serialize() const override {
            return {
                {"PositionX", Position.X},
                {"PositionY", Position.Y},
                {"Rotation", Rotation},
                {"ScaleX", Scale.X},
                {"ScaleY", Scale.Y}
            };
        }

        void Deserialize(const json& data) override {
            Position.X = data.value("PositionX", 0.0f);
            Position.Y = data.value("PositionY", 0.0f);
            Rotation = data.value("Rotation", 0.0f);
            Scale.X = data.value("ScaleX", 1.0f);
            Scale.Y = data.value("ScaleY", 1.0f);
        }

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
                const Texture& tex = TextureCache::Load(spritePath);
                TextureId = tex.ID();
                Width = tex.Width();
                Height = tex.Height();

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

        json Serialize() const override {
            return {
                {"TexturePath", TexturePath},
                {"Sprite", Sprite},
                {"ColorR", Color.R},
                {"ColorG", Color.G},
                {"ColorB", Color.B},
                {"ColorA", Color.A},
                {"FlipX", FlipX},
                {"FlipY", FlipY},
                {"SortingOrder", SortingOrder},
                {"SortingLayerName", SortingLayerName}
            };
        }

        void Deserialize(const json& data) override {
            TexturePath = data.value("TexturePath", "");
            Sprite = data.value("Sprite", "");

            if (!TexturePath.empty()) {
                const Texture& tex = TextureCache::Load(TexturePath);
                TextureId = tex.ID();
                Width = tex.Width();
                Height = tex.Height();
            }

            Color.R = data.value("ColorR", 1.0f);
            Color.G = data.value("ColorG", 1.0f);
            Color.B = data.value("ColorB", 1.0f);
            Color.A = data.value("ColorA", 1.0f);
            FlipX = data.value("FlipX", false);
            FlipY = data.value("FlipY", false);
            SortingOrder = data.value("SortingOrder", 0);
            SortingLayerName = data.value("SortingLayerName", "Default");
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

        json Serialize() const override {
            json j = {
                {"Type", static_cast<int>(Type)},
                {"FillColorR", FillColor.R},
                {"FillColorG", FillColor.G},
                {"FillColorB", FillColor.B},
                {"FillColorA", FillColor.A},
                {"OutlineColorR", OutlineColor.R},
                {"OutlineColorG", OutlineColor.G},
                {"OutlineColorB", OutlineColor.B},
                {"OutlineColorA", OutlineColor.A},
                {"OutlineThickness", OutlineThickness},
                {"SizeX", Size.X},
                {"SizeY", Size.Y},
                {"Radius", Radius},
                {"Closed", Closed}
            };

            json pointsArray = json::array();
            for (const auto& pt : Points) {
                pointsArray.push_back({ {"X", pt.X}, {"Y", pt.Y} });
            }
            j["Points"] = pointsArray;

            return j;
        }

        void Deserialize(const json& data) override {
            Type = static_cast<ShapeType>(data.value("Type", 0));
            FillColor.R = data.value("FillColorR", 1.0f);
            FillColor.G = data.value("FillColorG", 1.0f);
            FillColor.B = data.value("FillColorB", 1.0f);
            FillColor.A = data.value("FillColorA", 1.0f);
            OutlineColor.R = data.value("OutlineColorR", 0.0f);
            OutlineColor.G = data.value("OutlineColorG", 0.0f);
            OutlineColor.B = data.value("OutlineColorB", 0.0f);
            OutlineColor.A = data.value("OutlineColorA", 1.0f);
            OutlineThickness = data.value("OutlineThickness", 1.0f);
            Size.X = data.value("SizeX", 100.0f);
            Size.Y = data.value("SizeY", 100.0f);
            Radius = data.value("Radius", 50.0f);
            Closed = data.value("Closed", true);

            Points.clear();
            if (data.contains("Points") && data["Points"].is_array()) {
                for (const auto& ptJson : data["Points"]) {
                    Points.push_back({
                        ptJson.value("X", 0.0f),
                        ptJson.value("Y", 0.0f)
                        });
                }
            }
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

        json Serialize() const override {
            return {
                {"LinearVelocityX", LinearVelocity.X},
                {"LinearVelocityY", LinearVelocity.Y},
                {"AngularVelocity", AngularVelocity},
                {"Mass", Mass},
                {"LinearDamping", LinearDamping},
                {"AngularDamping", AngularDamping},
                {"GravityScale", GravityScale},
                {"FreezeRotation", FreezeRotation},
                {"BodyType", static_cast<int>(BodyType)}
            };
        }

        void Deserialize(const json& data) override {
            LinearVelocity.X = data.value("LinearVelocityX", 0.0f);
            LinearVelocity.Y = data.value("LinearVelocityY", 0.0f);
            AngularVelocity = data.value("AngularVelocity", 0.0f);
            Mass = data.value("Mass", 1.0f);
            LinearDamping = data.value("LinearDamping", 0.0f);
            AngularDamping = data.value("AngularDamping", 0.05f);
            GravityScale = data.value("GravityScale", 1.0f);
            FreezeRotation = data.value("FreezeRotation", false);
            BodyType = static_cast<enum BodyType>(data.value("BodyType", 0));
        }

        const char* GetTypeName() const override { return "Rigidbody2D"; }
    };

    struct Collider2D : IComponent {
        bool IsTrigger = false;
        Vector2D Offset{ 0, 0 };           // Offset from transform position
        int Layer = 0;                          // Physics layer

        Collider2D() = default;

        json Serialize() const override {
            return {
                {"IsTrigger", IsTrigger},
                {"OffsetX", Offset.X},
                {"OffsetY", Offset.Y},
                {"Layer", Layer}
            };
        }

        void Deserialize(const json& data) override {
            IsTrigger = data.value("IsTrigger", false);
            Offset.X = data.value("OffsetX", 0.0f);
            Offset.Y = data.value("OffsetY", 0.0f);
            Layer = data.value("Layer", 0);
        }

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

        json Serialize() const override {
            json j = Collider2D::Serialize();
            j["Radius"] = Radius;
            return j;
        }

        void Deserialize(const json& data) override {
            Collider2D::Deserialize(data);
            Radius = data.value("Radius", 0.5f);
        }

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

        json Serialize() const override {
            return {
                {"StartX", Start.X},
                {"StartY", Start.Y},
                {"EndX", End.X},
                {"EndY", End.Y},
                {"Thickness", Thickness},
                {"ColorR", Color.R},
                {"ColorG", Color.G},
                {"ColorB", Color.B},
                {"ColorA", Color.A}
            };
        }

        void Deserialize(const json& data) override {
            Start.X = data.value("StartX", 0.0f);
            Start.Y = data.value("StartY", 0.0f);
            End.X = data.value("EndX", 0.0f);
            End.Y = data.value("EndY", 0.0f);
            Thickness = data.value("Thickness", 1.0f);
            Color.R = data.value("ColorR", 1.0f);
            Color.G = data.value("ColorG", 1.0f);
            Color.B = data.value("ColorB", 1.0f);
            Color.A = data.value("ColorA", 1.0f);
        }

        const char* GetTypeName() const override { return "LineRenderer"; }
    };
}

#endif
