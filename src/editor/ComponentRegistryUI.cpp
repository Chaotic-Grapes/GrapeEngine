/* Start Header *****************************************************************/
/*!
\file   ComponentRegistryUI.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   15th November 2025
\brief
Implements UI component registry with all component metadata.

To add a new component:
1. Add entry to m_registry below
2. Implement RenderXXX() function in ComponentInspectorUI.cpp
3. Done! No need to modify InspectorPanel.cpp
*/
/* End Header *******************************************************************/

#include "../editor/ComponentRegistryUI.h"
#include "../editor/ComponentInspectorUI.h"

// Helper macro to reduce repetition
#define COMPONENT_OPS(T) \
    static_cast<std::function<bool(ECS::World*, ECS::Entity)>>([](ECS::World* w, ECS::Entity e) { return w->Has<T>(e); }), \
    static_cast<std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)>>([](ECS::World* w, ECS::Entity e, const nlohmann::json& d) { \
        if (!w->Has<T>(e)) w->Add<T>(e); \
        auto& c = w->Get<T>(e); \
        from_json(d, c); \
    }), \
    static_cast<std::function<void(ECS::World*, ECS::Entity)>>([](ECS::World* w, ECS::Entity e) { w->Remove<T>(e); }), \
    static_cast<std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)>>([](ECS::World* w, ECS::Entity e, const nlohmann::json& d) { \
        if (!w->Has<T>(e)) w->Add<T>(e); \
        auto& c = w->Get<T>(e); \
        from_json(d, c); \
    })

// Initialize the registry: ONE PLACE for all components
static std::vector<ComponentUIMetadata> m_registry = {
    // Transform: cannot be deleted
    {
        "Transform", "LocalTransform", "ECS::Components::LocalTransform", false,
        [](ComponentUI& ui, nlohmann::json& d) { ui.RenderLocalTransform(d); },
        []() { return nlohmann::json{
            {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
            {"Rotation", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 1.0f}}},
            {"Scale", {{"X", 1.0f}, {"Y", 1.0f}, {"Z", 1.0f}}}
        }; },
        COMPONENT_OPS(ECS::Components::LocalTransform)
    },

    // Camera 3D
    {
        "Camera 3D", "Camera3D", "ECS::Components::Camera3D", true,
        [](ComponentUI& ui, nlohmann::json& d) { ui.RenderCamera3D(d); },
        []() { return nlohmann::json{
            {"UsePerspective", false}, {"FOV", 45.0f}, {"NearPlane", 0.1f},
            {"FarPlane", 100.0f}, {"OrthoSize", 10.0f},
            {"AspectRatio", 16.0f / 9.0f}, {"Active", false}
        }; },
        COMPONENT_OPS(ECS::Components::Camera3D)
    },

    // Sprite Renderer 2D
    {
        "Sprite Renderer", "SpriteRenderer2D", "ECS::Components::SpriteRenderer2D", true,
        [](ComponentUI& ui, nlohmann::json& d) { ui.RenderSpriteRenderer2D(d); },
        []() { return nlohmann::json{
            {"TextureId", 0},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Tiling", {{"X", 1.0f}, {"Y", 1.0f}}},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Width", 0}, {"Height", 0}
        }; },
        COMPONENT_OPS(ECS::Components::SpriteRenderer2D)
    },

    // Rigidbody 2D
    {
        "Rigidbody 2D", "Rigidbody2D", "ECS::Components::Rigidbody2D", true,
        [](ComponentUI& ui, nlohmann::json& d) { ui.RenderRigidbody2D(d); },
        []() { return nlohmann::json{
            {"Mass", 1.0f}, {"InverseMass", 1.0f},
            {"LinearDamping", 0.0f}, {"AngularDamping", 0.0f},
            {"GravityScale", 1.0f}, {"Flags", 0}
        }; },
        COMPONENT_OPS(ECS::Components::Rigidbody2D)
    },

    // Circle Collider 2D
    {
        "Circle Collider 2D", "CircleCollider2D", "ECS::Components::CircleCollider2D", true,
        [](ComponentUI& ui, nlohmann::json& d) { ui.RenderCircleCollider2D(d); },
        []() { return nlohmann::json{
            {"Radius", 0.5f},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
        }; },
        COMPONENT_OPS(ECS::Components::CircleCollider2D)
    },

    // Box Collider 2D
    {
        "Box Collider 2D", "BoxCollider2D", "ECS::Components::BoxCollider2D", true,
        [](ComponentUI& ui, nlohmann::json& d) { ui.RenderBoxCollider2D(d); },
        []() { return nlohmann::json{
            {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Rotation", 0.0f},
            {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
        }; },
        COMPONENT_OPS(ECS::Components::BoxCollider2D)
    },

    // Shape Circle 2D
    {
        "Shape Circle", "ShapeCircle2D", "ECS::Components::ShapeCircle2D", true,
        [](ComponentUI& ui, nlohmann::json& d) { ui.RenderShapeCircle2D(d); },
        []() { return nlohmann::json{
            {"Radius", 0.5f},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Thickness", 1.0f}, {"Filled", false}
        }; },
        COMPONENT_OPS(ECS::Components::ShapeCircle2D)
    },

    // Shape Box 2D
    {
        "Shape Box", "ShapeBox2D", "ECS::Components::ShapeBox2D", true,
        [](ComponentUI& ui, nlohmann::json& d) { ui.RenderShapeBox2D(d); },
        []() { return nlohmann::json{
            {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Thickness", 1.0f}, {"Filled", false}
        }; },
        COMPONENT_OPS(ECS::Components::ShapeBox2D)
    },

    // Shape Line 2D
    {
        "Shape Line", "ShapeLine2D", "ECS::Components::ShapeLine2D", true,
        [](ComponentUI& ui, nlohmann::json& d) { ui.RenderShapeLine2D(d); },
        []() { return nlohmann::json{
            {"A", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"B", {{"X", 1.0f}, {"Y", 0.0f}}},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Thickness", 1.0f}
        }; },
        COMPONENT_OPS(ECS::Components::ShapeLine2D)
    }
};

const std::vector<ComponentUIMetadata>& ComponentRegistryUI::GetAll() {
    return m_registry;
}

const ComponentUIMetadata* ComponentRegistryUI::Find(const std::string& typeName) {
    LOG_INFO("ComponentRegistryUI::Find looking for: " << typeName);

    for (const auto& meta : m_registry) {
        bool match = (meta.TypeName == typeName || meta.FullTypeName == typeName);
        if (match) {
            LOG_INFO("FOUND: " << meta.DisplayName << " (TypeName: " << meta.TypeName << ", FullTypeName: " << meta.FullTypeName << ")");
            return &meta;
        }
    }

    LOG_INFO("NOT FOUND: " << typeName);
    return nullptr;
}
