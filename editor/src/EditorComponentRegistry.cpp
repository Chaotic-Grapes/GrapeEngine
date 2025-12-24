/* Start Header *****************************************************************/
/*!
\file   EditorComponentRegistry.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   15th November 2025

\brief
Editor UI registry that queries the native ECS ComponentRegistry.

This implementation:
1. Queries ECS::ComponentRegistry for all registered component IDs
2. For each ID, provides editor-specific UI metadata (display name, render function)
3. Uses hardcoded specialized renderers for known C++ components
4. Uses a generic JSON renderer for unknown C# components
5. Rebuilds dynamically when C# components are registered

Single source of truth: ECS::ComponentRegistry for what components exist.
This registry is only responsible for UI presentation.
*/
/* End Header *******************************************************************/

#include "EditorComponentRegistry.h"
#include "ComponentPropertyEditor.h"
#include "serialization/EntitySerializer.h"
#include "ecs/ComponentRegistry.h"
#include <mutex>
#include <cstring>
#include <nlohmann/json.hpp>
#include <imgui.h> 

/* 
Helper macro to reduce repetition when registering components

This macro expands into FOUR operations for a component type T:
1. canCheck: check if an entity HAS this component
2. apply: add/update the component using JSON data
3. remove: remove the component from the entity
4. applyPrefab: same as apply, used for prefab instances

The backslash at the end of each line tells the compiler that the macro continues on the next line
Without these, the macro would end early and break the expansion
*/
#define COMPONENT_OPS(T) \
    /* Check if entity already has component T; the lambda returns a bool */ \
    /* BUT the lambda has no name and no fixed type which is why we wrap it into a standard function object */ \
    /* static_cast converts the lambda into a std::function by wrapping it */ \
    static_cast<std::function<bool(ECS::World*, ECS::Entity)>>([](ECS::World* w, ECS::Entity e) { return w->Has<T>(e); }), \
    \
    /* Apply component data from JSON; if entity doesn't have T, add it then load values from JSON */ \
    /* into the component using from_json(d, c) */ \
    static_cast<std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)>>([](ECS::World* w, ECS::Entity e, const nlohmann::json& d) { \
        /* Add component if missing */ \
        if (!w->Has<T>(e)) w->Add<T>(e); \
        /* Get reference to component T on this entity and then fill component fields using JSON */ \
        auto& c = w->Get<T>(e); \
        from_json(d, c); \
    }), \
    \
    /* Remove component T from the entity */ \
    static_cast<std::function<void(ECS::World*, ECS::Entity)>>([](ECS::World* w, ECS::Entity e) { w->Remove<T>(e); }), \
    \
    /* Apply component data again (used for prefab instance overrides) */ \
    /* Same logic as above */ \
    static_cast<std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)>>([](ECS::World* w, ECS::Entity e, const nlohmann::json& d) { \
        if (!w->Has<T>(e)) w->Add<T>(e); \
        auto& c = w->Get<T>(e); \
        from_json(d, c); \
    })

// =============================================================================
// Hardcoded C++ Component Renderers
// =============================================================================

// These lambdas provide specialized UI rendering for known C++ components
// They're stored in a map keyed by component type ID for fast lookup
static auto& _getCppComponentRenderers() {
    static std::unordered_map<ECS::ComponentTypeId, std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>> renderers;
    
    // Initialize on first call
    if (renderers.empty()) {
        using namespace ECS::Components;
        
        renderers[ECS::ComponentRegistry::Type<LocalTransform>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLocalTransform(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<Name>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderName(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<Active>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderActive(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<TagMask>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderTagMask(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<Camera3D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderCamera3D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<SpriteRenderer2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderSpriteRenderer2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<SpriteSheetAnimation2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderSpriteSheetAnimation2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<ZIndex2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderZIndex2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<Rigidbody2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderRigidbody2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<LinearVelocity2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLinearVelocity2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<AngularVelocity2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAngularVelocity2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<Acceleration2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAcceleration2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<PhysicsMaterial2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderPhysicsMaterial2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<CircleCollider2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderCircleCollider2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<BoxCollider2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderBoxCollider2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<ShapeCircle2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeCircle2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<ShapeBox2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeBox2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<ShapeLine2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeLine2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<Light2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLight2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<AnimationState2D>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAnimationState2D(d, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<AudioSource>()] = 
            [](ComponentUI& ui, nlohmann::json& data, ECS::Entity e, ECS::World* w) { ui.RenderAudioSource(data, e, w); };
        
        renderers[ECS::ComponentRegistry::Type<Layer>()] = 
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLayer2D(d, e, w); };
    }
    
    return renderers;
}

// Hardcoded default values for known C++ components
static auto& _getCppComponentDefaults() {
    static std::unordered_map<ECS::ComponentTypeId, std::function<nlohmann::json()>> defaults;
    
    if (defaults.empty()) {
        using namespace ECS::Components;
        
        defaults[ECS::ComponentRegistry::Type<LocalTransform>()] = []() { 
            return nlohmann::json{
                {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
                {"Rotation", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 1.0f}}},
                {"Scale", {{"X", 1.0f}, {"Y", 1.0f}, {"Z", 1.0f}}}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<Name>()] = []() { 
            return nlohmann::json{{"Value", "Entity"}}; 
        };
        
        defaults[ECS::ComponentRegistry::Type<Active>()] = []() { 
            return nlohmann::json{{"Enabled", true}}; 
        };
        
        defaults[ECS::ComponentRegistry::Type<TagMask>()] = []() { 
            return nlohmann::json{{"Mask", 0}}; 
        };
        
        defaults[ECS::ComponentRegistry::Type<Camera3D>()] = []() { 
            return nlohmann::json{
                {"UsePerspective", false}, {"FOV", 45.0f}, {"NearPlane", 0.1f},
                {"FarPlane", 100.0f}, {"OrthoSize", 10.0f},
                {"AspectRatio", 16.0f / 9.0f}, {"Active", false}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<SpriteRenderer2D>()] = []() { 
            return nlohmann::json{
                {"TextureId", 0},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Tiling", {{"X", 1.0f}, {"Y", 1.0f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Width", 0}, {"Height", 0}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<SpriteSheetAnimation2D>()] = []() { 
            return nlohmann::json{
                {"TextureId", 0}, {"FrameWidth", 32}, {"FrameHeight", 32},
                {"SheetWidth", 256}, {"SheetHeight", 256},
                {"StartFrame", 0}, {"FrameCount", 1}, {"FramesPerSecond", 10.0f},
                {"Loop", true}, {"Playing", false}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<ZIndex2D>()] = []() { 
            return nlohmann::json{{"ZOrder", 0}}; 
        };
        
        defaults[ECS::ComponentRegistry::Type<Rigidbody2D>()] = []() { 
            return nlohmann::json{
                {"Mass", 1.0f}, {"InverseMass", 1.0f},
                {"LinearDamping", 0.0f}, {"AngularDamping", 0.0f},
                {"GravityScale", 1.0f}, {"Flags", 0}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<LinearVelocity2D>()] = []() { 
            return nlohmann::json{{"Value", {{"X", 0.0f}, {"Y", 0.0f}}}}; 
        };
        
        defaults[ECS::ComponentRegistry::Type<AngularVelocity2D>()] = []() { 
            return nlohmann::json{{"Value", 0.0f}}; 
        };
        
        defaults[ECS::ComponentRegistry::Type<Acceleration2D>()] = []() { 
            return nlohmann::json{{"Value", {{"X", 0.0f}, {"Y", 0.0f}}}}; 
        };
        
        defaults[ECS::ComponentRegistry::Type<PhysicsMaterial2D>()] = []() { 
            return nlohmann::json{
                {"Friction", 0.5f}, {"Restitution", 0.0f}, {"PositionCorrectPercent", 0.2f}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<CircleCollider2D>()] = []() { 
            return nlohmann::json{
                {"Radius", 0.5f},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<BoxCollider2D>()] = []() { 
            return nlohmann::json{
                {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Rotation", 0.0f},
                {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<ShapeCircle2D>()] = []() { 
            return nlohmann::json{
                {"Radius", 0.5f},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 1.0f}, {"Filled", false}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<ShapeBox2D>()] = []() { 
            return nlohmann::json{
                {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 0.1f}, {"Filled", false}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<ShapeLine2D>()] = []() { 
            return nlohmann::json{
                {"A", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"B", {{"X", 1.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 1.0f}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<Light2D>()] = []() { 
            return nlohmann::json{
                {"LightType", 0},
                {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
                {"Direction", {{"X", 0.0f}, {"Y", -1.0f}, {"Z", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Intensity", 1.0f}, {"Range", 10.0f}, {"CastsShadows", false}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<AnimationState2D>()] = []() { 
            return nlohmann::json{
                {"CurrentFrame", 0}, {"TimeAccumulator", 0.0f}, {"Finished", false}
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<AudioSource>()] = []() { 
            return nlohmann::json{
                {"CueId", 0 },
                {"Volume", 1.0f },
                {"Pitch", 1.0f },
                {"Loop", false },
                {"PlayOnStart", false },
                {"Spatial3D", false }
            }; 
        };
        
        defaults[ECS::ComponentRegistry::Type<Layer>()] = []() { 
            return nlohmann::json{{"Id", 0}}; 
        };
    }
    
    return defaults;
}

// =============================================================================
// Dynamic Registry Implementation
// =============================================================================

// The s_registry is built dynamically from the native ECS::ComponentRegistry
// It caches the editor metadata to avoid repeated lookups
static std::vector<ComponentUIMetadata> s_registry;
static std::mutex s_registryLock;

// Initialize registry with hardcoded C++ components
static void _initializeDefaultRegistry() {
    if (!s_registry.empty()) return;  // Already initialized
    
    using namespace ECS::Components;
    
    // Add all hardcoded C++ components with their specialized operations
    s_registry = {
        // Transform: cannot be deleted (every entity has one)
        {
            "Transform", "LocalTransform", "ECS::Components::LocalTransform",
            ECS::ComponentRegistry::Type<LocalTransform>(), false,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLocalTransform(d, e, w); },
            []() { return nlohmann::json{
                {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
                {"Rotation", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 1.0f}}},
                {"Scale", {{"X", 1.0f}, {"Y", 1.0f}, {"Z", 1.0f}}}
            }; },
            COMPONENT_OPS(LocalTransform)
        },
        // Name
        {
            "Name", "Name", "ECS::Components::Name",
            ECS::ComponentRegistry::Type<Name>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderName(d, e, w); },
            []() { return nlohmann::json{{"Value", "Entity"}}; },
            COMPONENT_OPS(Name)
        },
        // Active
        {
            "Active", "Active", "ECS::Components::Active",
            ECS::ComponentRegistry::Type<Active>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderActive(d, e, w); },
            []() { return nlohmann::json{{"Enabled", true}}; },
            COMPONENT_OPS(Active)
        },
        // Tag Mask
        {
            "Tag Mask", "TagMask", "ECS::Components::TagMask",
            ECS::ComponentRegistry::Type<TagMask>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderTagMask(d, e, w); },
            []() { return nlohmann::json{{"Mask", 0}}; },
            COMPONENT_OPS(TagMask)
        },
        // Camera 3D
        {
            "Camera 3D", "Camera3D", "ECS::Components::Camera3D",
            ECS::ComponentRegistry::Type<Camera3D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderCamera3D(d, e, w); },
            []() { return nlohmann::json{
                {"UsePerspective", false}, {"FOV", 45.0f}, {"NearPlane", 0.1f},
                {"FarPlane", 100.0f}, {"OrthoSize", 10.0f},
                {"AspectRatio", 16.0f / 9.0f}, {"Active", false}
            }; },
            COMPONENT_OPS(Camera3D)
        },
        // Sprite Renderer 2D
        {
            "Sprite Renderer 2D", "SpriteRenderer2D", "ECS::Components::SpriteRenderer2D",
            ECS::ComponentRegistry::Type<SpriteRenderer2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderSpriteRenderer2D(d, e, w); },
            []() { return nlohmann::json{
                {"TextureId", 0},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Tiling", {{"X", 1.0f}, {"Y", 1.0f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Width", 0}, {"Height", 0}
            }; },
            COMPONENT_OPS(SpriteRenderer2D)
        },
        // Sprite Sheet Animation 2D
        {
            "Sprite Sheet Animation 2D", "SpriteSheetAnimation2D", "ECS::Components::SpriteSheetAnimation2D",
            ECS::ComponentRegistry::Type<SpriteSheetAnimation2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderSpriteSheetAnimation2D(d, e, w); },
            []() { return nlohmann::json{
                {"TextureId", 0}, {"FrameWidth", 32}, {"FrameHeight", 32},
                {"SheetWidth", 256}, {"SheetHeight", 256},
                {"StartFrame", 0}, {"FrameCount", 1}, {"FramesPerSecond", 10.0f},
                {"Loop", true}, {"Playing", false}
            }; },
            COMPONENT_OPS(SpriteSheetAnimation2D)
        },
        // Z-Index 2D
        {
            "Z-Index 2D", "ZIndex2D", "ECS::Components::ZIndex2D",
            ECS::ComponentRegistry::Type<ZIndex2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderZIndex2D(d, e, w); },
            []() { return nlohmann::json{{"ZOrder", 0}}; },
            COMPONENT_OPS(ZIndex2D)
        },
        // Rigidbody 2D
        {
            "Rigidbody 2D", "Rigidbody2D", "ECS::Components::Rigidbody2D",
            ECS::ComponentRegistry::Type<Rigidbody2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderRigidbody2D(d, e, w); },
            []() { return nlohmann::json{
                {"Mass", 1.0f}, {"InverseMass", 1.0f},
                {"LinearDamping", 0.0f}, {"AngularDamping", 0.0f},
                {"GravityScale", 1.0f}, {"Flags", 0}
            }; },
            COMPONENT_OPS(Rigidbody2D)
        },
        // Linear Velocity 2D
        {
            "Linear Velocity 2D", "LinearVelocity2D", "ECS::Components::LinearVelocity2D",
            ECS::ComponentRegistry::Type<LinearVelocity2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLinearVelocity2D(d, e, w); },
            []() { return nlohmann::json{{"Value", {{"X", 0.0f}, {"Y", 0.0f}}}}; },
            COMPONENT_OPS(LinearVelocity2D)
        },
        // Angular Velocity 2D
        {
            "Angular Velocity 2D", "AngularVelocity2D", "ECS::Components::AngularVelocity2D",
            ECS::ComponentRegistry::Type<AngularVelocity2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAngularVelocity2D(d, e, w); },
            []() { return nlohmann::json{{"Value", 0.0f}}; },
            COMPONENT_OPS(AngularVelocity2D)
        },
        // Acceleration 2D
        {
            "Acceleration 2D", "Acceleration2D", "ECS::Components::Acceleration2D",
            ECS::ComponentRegistry::Type<Acceleration2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAcceleration2D(d, e, w); },
            []() { return nlohmann::json{{"Value", {{"X", 0.0f}, {"Y", 0.0f}}}}; },
            COMPONENT_OPS(Acceleration2D)
        },
        // Physics Material 2D
        {
            "Physics Material 2D", "PhysicsMaterial2D", "ECS::Components::PhysicsMaterial2D",
            ECS::ComponentRegistry::Type<PhysicsMaterial2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderPhysicsMaterial2D(d, e, w); },
            []() { return nlohmann::json{
                {"Friction", 0.5f}, {"Restitution", 0.0f}, {"PositionCorrectPercent", 0.2f}
            }; },
            COMPONENT_OPS(PhysicsMaterial2D)
        },
        // Circle Collider 2D
        {
            "Circle Collider 2D", "CircleCollider2D", "ECS::Components::CircleCollider2D",
            ECS::ComponentRegistry::Type<CircleCollider2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderCircleCollider2D(d, e, w); },
            []() { return nlohmann::json{
                {"Radius", 0.5f},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
            }; },
            COMPONENT_OPS(CircleCollider2D)
        },
        // Box Collider 2D
        {
            "Box Collider 2D", "BoxCollider2D", "ECS::Components::BoxCollider2D",
            ECS::ComponentRegistry::Type<BoxCollider2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderBoxCollider2D(d, e, w); },
            []() { return nlohmann::json{
                {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Rotation", 0.0f},
                {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
            }; },
            COMPONENT_OPS(BoxCollider2D)
        },
        // Shape Circle 2D
        {
            "Shape Circle", "ShapeCircle2D", "ECS::Components::ShapeCircle2D",
            ECS::ComponentRegistry::Type<ShapeCircle2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeCircle2D(d, e, w); },
            []() { return nlohmann::json{
                {"Radius", 0.5f},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 1.0f}, {"Filled", false}
            }; },
            COMPONENT_OPS(ShapeCircle2D)
        },
        // Shape Box 2D
        {
            "Shape Box", "ShapeBox2D", "ECS::Components::ShapeBox2D",
            ECS::ComponentRegistry::Type<ShapeBox2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeBox2D(d, e, w); },
            []() { return nlohmann::json{
                {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 0.1f}, {"Filled", false}
            }; },
            COMPONENT_OPS(ShapeBox2D)
        },
        // Shape Line 2D
        {
            "Shape Line", "ShapeLine2D", "ECS::Components::ShapeLine2D",
            ECS::ComponentRegistry::Type<ShapeLine2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeLine2D(d, e, w); },
            []() { return nlohmann::json{
                {"A", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"B", {{"X", 1.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 1.0f}
            }; },
            COMPONENT_OPS(ShapeLine2D)
        },
        // Light 2D
        {
            "Light 2D", "Light2D", "ECS::Components::Light2D",
            ECS::ComponentRegistry::Type<Light2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLight2D(d, e, w); },
            []() { return nlohmann::json{
                {"LightType", 0},
                {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
                {"Direction", {{"X", 0.0f}, {"Y", -1.0f}, {"Z", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Intensity", 1.0f}, {"Range", 10.0f}, {"CastsShadows", false}
            }; },
            COMPONENT_OPS(Light2D)
        },
        // Animation State 2D
        {
            "Animation State 2D", "AnimationState2D", "ECS::Components::AnimationState2D",
            ECS::ComponentRegistry::Type<AnimationState2D>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAnimationState2D(d, e, w); },
            []() { return nlohmann::json{
                {"CurrentFrame", 0}, {"TimeAccumulator", 0.0f}, {"Finished", false}
            }; },
            COMPONENT_OPS(AnimationState2D)
        },
        // Audio Source
        {
            "Audio Source", "AudioSource", "ECS::Components::AudioSource",
            ECS::ComponentRegistry::Type<AudioSource>(), true,
            [](ComponentUI& ui, nlohmann::json& data, ECS::Entity e, ECS::World* w) { ui.RenderAudioSource(data, e, w); },
            []() { return nlohmann::json{
                { "CueId", 0 },
                { "Volume", 1.0f },
                { "Pitch", 1.0f },
                { "Loop", false },
                { "PlayOnStart", false },
                { "Spatial3D", false }
            }; },
            COMPONENT_OPS(AudioSource)
        },
        // Layer
        {
            "Layer 2D", "Layer", "ECS::Components::Layer",
            ECS::ComponentRegistry::Type<Layer>(), true,
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLayer2D(d, e, w); },
            []() { return nlohmann::json{{"Id", 0}}; },
            COMPONENT_OPS(Layer)
        }
    };
}

// Rebuild the editor registry from the native ECS registry
// For C# components discovered after startup, add them with generic operations
void ComponentRegistryUI::RebuildFromNativeRegistry() {
    std::lock_guard<std::mutex> lock(s_registryLock);
    
    // Initialize with hardcoded C++ components first
    _initializeDefaultRegistry();
    
    // Get all registered component IDs from the native registry
    auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
    LOG_INFO("[EditorComponentRegistry] RebuildFromNativeRegistry: Found " << allIds.size() << " total component IDs in native registry");
    // Dump details for debugging
    for (ECS::ComponentTypeId aid : allIds) {
        const auto& ameta = ECS::ComponentRegistry::Meta(aid);
        std::string aname = ECS::ComponentRegistry::GetComponentNameFromHash(ameta.TypeHash);
        if (aname.empty()) aname = "<no-name>";
        LOG_INFO("[EditorComponentRegistry]   NativeID " << aid << " hash=0x" << std::hex << ameta.TypeHash << std::dec << " size=" << ameta.Size << " name=" << aname);
    }
    
    // Debug: log the hardcoded registry count
    LOG_INFO("[EditorComponentRegistry] Hardcoded C++ components in s_registry: " << s_registry.size());
    
    // Add C++ components that aren't hardcoded
    int newComponentsFound = 0;
    for (ECS::ComponentTypeId id : allIds) {
        const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
        
        // Skip invalid/uninitialized entries (hash 0x0 is impossible for real components)
        if (nativeMeta.TypeHash == 0) {
            continue;
        }
        
        // Check if this component is already in our registry
        bool alreadyExists = false;
        for (const auto& meta : s_registry) {
            if (meta.TypeHash == ECS::ComponentRegistry::Meta(id).TypeHash) {
                alreadyExists = true;
                break;
            }
        }
        
        if (alreadyExists) {
            continue;  // Skip C++ components, they're already added
        }
        
        // This is a new C# component - add it with generic operations
        LOG_INFO("[EditorComponentRegistry] Found new C# component: ID " << id << ", hash 0x" << std::hex << nativeMeta.TypeHash << std::dec << ", size " << nativeMeta.Size);
        newComponentsFound++;
        
        std::string displayName;
        std::string typeName;
        
        // Try to get the component name from the native registry
        std::string nativeName = ECS::ComponentRegistry::GetComponentNameFromHash(nativeMeta.TypeHash);
        if (!nativeName.empty()) {
            // Use the actual component name if available
            displayName = nativeName;
            // Extract just the class name from the full type name (e.g., "EchoesBelow.Scripts.Health" -> "Health")
            size_t lastDot = nativeName.rfind('.');
            if (lastDot != std::string::npos) {
                displayName = nativeName.substr(lastDot + 1);
            }
            typeName = nativeName;
            LOG_INFO("[EditorComponentRegistry]   -> Using native name: " << displayName);
        }
        else {
            // Fallback to hash-based name if no name registered
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "Component_0x%08x", nativeMeta.TypeHash);
            displayName = buffer;
            typeName = displayName;
            LOG_WARNING("[EditorComponentRegistry]   -> No native name found, using hash-based name: " << displayName);
        }
        
        // Create generic operations for C# components
        auto hasComponentFunc = [id](ECS::World* w, ECS::Entity e) {
            if (!w) return false;
            return w->HasById(e, id);
        };
        
        auto addComponentFunc = [id](ECS::World* w, ECS::Entity e, const nlohmann::json& data) {
            if (!w) {
                LOG_WARNING("[EditorComponentRegistry] addComponentFunc called with null world");
                return;
            }
            const auto& meta = ECS::ComponentRegistry::Meta(id);
            if (meta.Size == 0) {
                LOG_WARNING("[EditorComponentRegistry] Component ID " << id << " has zero size");
                return;
            }
            
            // Allocate zero-initialized buffer for the component
            std::vector<uint8_t> buffer(meta.Size, 0);
            void* result = w->AddComponentById(e, id, buffer.data(), meta.Size);
            
            if (result) {
                LOG_INFO("[EditorComponentRegistry] Successfully added C# component ID " << id << " to entity (ptr=" << result << ", size=" << meta.Size << ")");
                
                // Verify the component was actually added
                if (!w->HasById(e, id)) {
                    LOG_ERROR("[EditorComponentRegistry] ERROR: Component was added but HasComponent check failed!");
                }
            } else {
                LOG_ERROR("[EditorComponentRegistry] AddComponentById returned null for component ID " << id);
            }
        };
        
        auto removeComponentFunc = [id](ECS::World* w, ECS::Entity e) {
            if (!w) return;
            w->RemoveById(e, id);
        };
        
        auto applyComponentFunc = [id](ECS::World* w, ECS::Entity e, const nlohmann::json& data) {
            if (!w) return;
            const auto& meta = ECS::ComponentRegistry::Meta(id);
            void* componentPtr = w->GetRawComponentPtr(e, id);
            if (!componentPtr) {
                // Allocate zero-initialized buffer
                std::vector<uint8_t> buffer(meta.Size, 0);
                w->AddComponentById(e, id, buffer.data(), meta.Size);
            }
            // TODO: Parse JSON data and copy into component buffer
        };
        
        // Add to registry with generic renderer
        s_registry.emplace_back(
            displayName,
            typeName,
            typeName,
            id,
            true,  // C# components can be deleted
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGenericComponent(d, e, w); },
            []() { return nlohmann::json::object(); },
            hasComponentFunc,
            addComponentFunc,
            removeComponentFunc,
            applyComponentFunc
        );
        
        LOG_INFO("[EditorComponentRegistry] Added C# component to editor registry: " << displayName << " (hash 0x" << std::hex << nativeMeta.TypeHash << std::dec << ")");
    }
    
    LOG_INFO("[EditorComponentRegistry] RebuildFromNativeRegistry complete: Found " << newComponentsFound << " new C# components. Total in editor registry = " << s_registry.size());
}

// Returns the full component metadata list (C++ + C# components)
const std::vector<ComponentUIMetadata>& ComponentRegistryUI::GetAll() {
    std::lock_guard<std::mutex> lock(s_registryLock);
    
    // Ensure default registry is initialized
    if (s_registry.empty()) {
        _initializeDefaultRegistry();
    }
    
    return s_registry;
}

// Finds a single component type using either its short or full type name
const ComponentUIMetadata* ComponentRegistryUI::Find(const std::string& typeName) {
    std::lock_guard<std::mutex> lock(s_registryLock);
    
    for (const auto& meta : s_registry) {
        if (meta.TypeName == typeName || meta.FullTypeName == typeName) {
            return &meta;
        }
    }
    // LOG_WARNING("[EditorComponentRegistry] Find: No metadata for component type '" << typeName << "'");
    return nullptr;
}