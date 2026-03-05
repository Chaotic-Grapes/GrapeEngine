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
4. Uses a generic JSON renderer for unknown managed components
5. Rebuilds dynamically when managed components are registered

Single source of truth: ECS::ComponentRegistry for what components exist.
This registry is only responsible for UI presentation.
*/
/* End Header *******************************************************************/

#include "EditorComponentRegistry.h"
#include "ComponentPropertyEditor.h"
#include "EditorECSUtils.h"
#include "serialization/EntitySerializer.h"
#include "ecs/ComponentRegistry.h"
#include <mutex>
#include <cstring>
#include <nlohmann/json.hpp>
#include <imgui.h> 

namespace {
    // Return component id from hash or warn.
    ECS::ComponentTypeId GetComponentIdFromHashOrWarn(uint32_t hash, const char* name) {
        const ECS::ComponentTypeId id = ECS::ComponentRegistry::GetComponentIdFromHash(hash);
        if (id == ECS::NULL_COMPONENT_ID) {
            LOG_WARNING("[EditorComponentRegistry] Missing component ID for '" << name << "' (hash: 0x"
                << std::hex << hash << std::dec << ")");
        }
        return id;
    }

    const uint32_t kHashLocalTransform = Editor::ECSUtils::FNV1aHash("LocalTransform");
    const uint32_t kHashName = Editor::ECSUtils::FNV1aHash("Name");
    const uint32_t kHashActive = Editor::ECSUtils::FNV1aHash("Active");
    const uint32_t kHashTagMask = Editor::ECSUtils::FNV1aHash("TagMask");
    const uint32_t kHashCamera3D = Editor::ECSUtils::FNV1aHash("Camera3D");
    const uint32_t kHashSpriteRenderer2D = Editor::ECSUtils::FNV1aHash("SpriteRenderer2D");
    const uint32_t kHashSpriteSheetAnimation2D = Editor::ECSUtils::FNV1aHash("SpriteSheetAnimation2D");
    const uint32_t kHashTileMapComponent = Editor::ECSUtils::FNV1aHash("TileMapComponent");
    const uint32_t kHashZIndex2D = Editor::ECSUtils::FNV1aHash("ZIndex2D");
    const uint32_t kHashRigidbody2D = Editor::ECSUtils::FNV1aHash("Rigidbody2D");
    const uint32_t kHashLinearVelocity2D = Editor::ECSUtils::FNV1aHash("LinearVelocity2D");
    const uint32_t kHashAngularVelocity2D = Editor::ECSUtils::FNV1aHash("AngularVelocity2D");
    const uint32_t kHashAcceleration2D = Editor::ECSUtils::FNV1aHash("Acceleration2D");
    const uint32_t kHashPhysicsMaterial2D = Editor::ECSUtils::FNV1aHash("PhysicsMaterial2D");
    const uint32_t kHashCircleCollider2D = Editor::ECSUtils::FNV1aHash("CircleCollider2D");
    const uint32_t kHashBoxCollider2D = Editor::ECSUtils::FNV1aHash("BoxCollider2D");
    const uint32_t kHashShapeCircle2D = Editor::ECSUtils::FNV1aHash("ShapeCircle2D");
    const uint32_t kHashShapeBox2D = Editor::ECSUtils::FNV1aHash("ShapeBox2D");
    const uint32_t kHashShapeLine2D = Editor::ECSUtils::FNV1aHash("ShapeLine2D");
    const uint32_t kHashLight2D = Editor::ECSUtils::FNV1aHash("Light2D");
    const uint32_t kHashAnimationState2D = Editor::ECSUtils::FNV1aHash("AnimationState2D");
    const uint32_t kHashAudioSource = Editor::ECSUtils::FNV1aHash("AudioSource");
    const uint32_t kHashLayer = Editor::ECSUtils::FNV1aHash("Layer");
    const uint32_t kHashMaterial2D = Editor::ECSUtils::FNV1aHash("Material2D");
    const uint32_t kHashBoidFlock = Editor::ECSUtils::FNV1aHash("BoidFlock");
    const uint32_t kHashGUICanvas = Editor::ECSUtils::FNV1aHash("GUICanvas");
    const uint32_t kHashGUIRenderMode = Editor::ECSUtils::FNV1aHash("GUIRenderMode");
    const uint32_t kHashGUIElement = Editor::ECSUtils::FNV1aHash("GUIElement");
    const uint32_t kHashGUIPanel = Editor::ECSUtils::FNV1aHash("GUIPanel");
    const uint32_t kHashGUIText = Editor::ECSUtils::FNV1aHash("GUIText");
    const uint32_t kHashGUIImage = Editor::ECSUtils::FNV1aHash("GUIImage");
    const uint32_t kHashGUIInput = Editor::ECSUtils::FNV1aHash("GUIInput");
    const uint32_t kHashGUIStateStyle = Editor::ECSUtils::FNV1aHash("GUIStateStyle");
    const uint32_t kHashGUIButton = Editor::ECSUtils::FNV1aHash("GUIButton");
    const uint32_t kHashGUISlider = Editor::ECSUtils::FNV1aHash("GUISlider");
    
    template<typename T>
    // Build default JSON payload for components.
    nlohmann::json MakeDefaultJson() {
        T value{};
        nlohmann::json j;
        // Use ADL to_json overloads when available.
        Serialization::to_json_adl(j, value);
        return j;
    }
}

// Callback function pointer for deserializing managed components from JSON
// Called when editor applies component property changes to entities at runtime
using DeserializeComponentCallback = void(*)(ECS::ComponentTypeId id, void* componentPtr, int size, const char* jsonStr);
static DeserializeComponentCallback s_deserializeComponentCallback = nullptr;

// Function to register the deserialize callback from managed code
extern "C" void RegisterComponentDeserializeCallback(DeserializeComponentCallback callback)
{
    s_deserializeComponentCallback = callback;
    LOG_INFO("[EditorComponentRegistry] Component deserialize callback registered");
}

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
#define COMPONENT_OPS_HASH(T, HASH) \
    /* Check if entity already has component T; the lambda returns a bool */ \
    /* BUT the lambda has no name and no fixed type which is why we wrap it into a standard function object */ \
    /* static_cast converts the lambda into a std::function by wrapping it */ \
    static_cast<std::function<bool(ECS::World*, ECS::Entity)>>([](ECS::World* w, ECS::Entity e) { \
        if (!w) return false; \
        const ECS::ComponentTypeId id = GetComponentIdFromHashOrWarn(HASH, #T); \
        if (id == ECS::NULL_COMPONENT_ID) return false; \
        return w->HasById(e, id); \
    }), \
    \
    /* Apply component data from JSON; if entity doesn't have T, add it then load values from JSON */ \
    /* into the component using from_json(d, c) */ \
    static_cast<std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)>>([](ECS::World* w, ECS::Entity e, const nlohmann::json& d) { \
        if (!w) return; \
        const ECS::ComponentTypeId id = GetComponentIdFromHashOrWarn(HASH, #T); \
        if (id == ECS::NULL_COMPONENT_ID) return; \
        T value = Serialization::from_json_adl<T>(d); \
        void* ptr = w->GetRawComponentPtr(e, id); \
        if (ptr) { \
            *static_cast<T*>(ptr) = value; \
        } else { \
            w->AddComponentById(e, id, &value, sizeof(T)); \
        } \
    }), \
    \
    /* Remove component T from the entity */ \
    static_cast<std::function<void(ECS::World*, ECS::Entity)>>([](ECS::World* w, ECS::Entity e) { \
        if (!w) return; \
        const ECS::ComponentTypeId id = GetComponentIdFromHashOrWarn(HASH, #T); \
        if (id == ECS::NULL_COMPONENT_ID) return; \
        w->RemoveById(e, id); \
    }), \
    \
    /* Apply component data again (used for prefab instance overrides) */ \
    /* Same logic as above */ \
    static_cast<std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)>>([](ECS::World* w, ECS::Entity e, const nlohmann::json& d) { \
        if (!w) return; \
        const ECS::ComponentTypeId id = GetComponentIdFromHashOrWarn(HASH, #T); \
        if (id == ECS::NULL_COMPONENT_ID) return; \
        T value = Serialization::from_json_adl<T>(d); \
        void* ptr = w->GetRawComponentPtr(e, id); \
        if (ptr) { \
            *static_cast<T*>(ptr) = value; \
        } else { \
            w->AddComponentById(e, id, &value, sizeof(T)); \
        } \
    })

// =============================================================================
// Hardcoded C++ Component Renderers
// =============================================================================

// These lambdas provide specialized UI rendering for known C++ components
// They're stored in a map keyed by component type ID for fast lookup
[[maybe_unused]] static auto& _getCppComponentRenderers() {
    static std::unordered_map<ECS::ComponentTypeId, std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>> renderers;
    
    // Initialize on first call
    if (renderers.empty()) {
        using namespace ECS::Components;
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashLocalTransform, "LocalTransform"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLocalTransform(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashName, "Name"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderName(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashActive, "Active"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderActive(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashTagMask, "TagMask"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderTagMask(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashCamera3D, "Camera3D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderCamera3D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashSpriteRenderer2D, "SpriteRenderer2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderSpriteRenderer2D(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashTileMapComponent, "TileMapComponent"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderTileMapComponent(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashSpriteSheetAnimation2D, "SpriteSheetAnimation2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderSpriteSheetAnimation2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashZIndex2D, "ZIndex2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderZIndex2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashRigidbody2D, "Rigidbody2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderRigidbody2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashLinearVelocity2D, "LinearVelocity2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLinearVelocity2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashAngularVelocity2D, "AngularVelocity2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAngularVelocity2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashAcceleration2D, "Acceleration2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAcceleration2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashPhysicsMaterial2D, "PhysicsMaterial2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderPhysicsMaterial2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashCircleCollider2D, "CircleCollider2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderCircleCollider2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashBoxCollider2D, "BoxCollider2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderBoxCollider2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashShapeCircle2D, "ShapeCircle2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeCircle2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashShapeBox2D, "ShapeBox2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeBox2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashShapeLine2D, "ShapeLine2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeLine2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashLight2D, "Light2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLight2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashAnimationState2D, "AnimationState2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAnimationState2D(d, e, w); };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashAudioSource, "AudioSource"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& data, ECS::Entity e, ECS::World* w) { ui.RenderAudioSource(data, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashLayer, "Layer"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLayer2D(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashMaterial2D, "Material2D"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderMaterial2D(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUICanvas, "GUICanvas"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUICanvas(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIRenderMode, "GUIRenderMode"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIRenderMode(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIElement, "GUIElement"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIElement(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIPanel, "GUIPanel"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIPanel(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIText, "GUIText"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIText(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIImage, "GUIImage"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIImage(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIInput, "GUIInput"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIInput(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIStateStyle, "GUIStateStyle"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIStateStyle(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIButton, "GUIButton"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIButton(d, e, w); };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUISlider, "GUISlider"); id != ECS::NULL_COMPONENT_ID) {
            renderers[id] = [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUISlider(d, e, w); };
        }
    }
    
    return renderers;
}

// Hardcoded default values for known C++ components
[[maybe_unused]] static auto& _getCppComponentDefaults() {
    static std::unordered_map<ECS::ComponentTypeId, std::function<nlohmann::json()>> defaults;
    
    if (defaults.empty()) {
        using namespace ECS::Components;
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashLocalTransform, "LocalTransform"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
            return nlohmann::json{
                {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
                {"Rotation", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 1.0f}}},
                {"Scale", {{"X", 1.0f}, {"Y", 1.0f}, {"Z", 1.0f}}}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashName, "Name"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{{"Value", "Entity"}}; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashActive, "Active"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{{"Enabled", true}}; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashTagMask, "TagMask"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{{"Mask", 0}}; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashCamera3D, "Camera3D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"UsePerspective", false}, {"FOV", 45.0f}, {"NearPlane", 0.1f},
                {"FarPlane", 100.0f}, {"OrthoSize", 10.0f},
                {"AspectRatio", 16.0f / 9.0f}, {"Active", false}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashSpriteRenderer2D, "SpriteRenderer2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"TextureId", 0},
                {"NormalTextureId", 0},
                {"EmissiveTextureId", 0},
                {"EmissiveStrength", 5.0f},
                {"EmissiveTexturePath", ""},
                {"TextureFilter", 0},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Tiling", {{"X", 1.0f}, {"Y", 1.0f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Width", 0}, {"Height", 0}
            }; 
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashTileMapComponent, "TileMapComponent"); id != ECS::NULL_COMPONENT_ID) {
            // Provide sane defaults for new tilemap components.
            defaults[id] = []() {
                return nlohmann::json{
                    {"TileMapPath", ""},
                    {"TilesetTexturePath", ""},
                    {"TileWorldSize", 1.0f},
                    {"TilePixelSize", 32},
                    {"DefaultWidth", 64},
                    {"DefaultHeight", 64},
                    {"LayerIndex", 0},
                    {"Visible", true}
                };
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashSpriteSheetAnimation2D, "SpriteSheetAnimation2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"TextureId", 0}, {"NormalTextureId", 0}, {"FrameWidth", 32}, {"FrameHeight", 32},
                {"SheetWidth", 256}, {"SheetHeight", 256},
                {"StartFrame", 0}, {"FrameCount", 1}, {"FramesPerSecond", 10.0f},
                {"Row", 0}, {"FrameOffset", 0}, {"FrameLength", 0},
                {"Loop", true}, {"Playing", false}, {"UseRow", false}, {"UseSegments", false}, {"Segments", nlohmann::json::array()},
                {"TextureFilter", 0}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashZIndex2D, "ZIndex2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{{"ZOrder", 0}}; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashRigidbody2D, "Rigidbody2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"Mass", 1.0f}, {"InverseMass", 1.0f},
                {"LinearDamping", 0.0f}, {"AngularDamping", 0.0f},
                {"GravityScale", 1.0f}, {"Flags", 0}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashLinearVelocity2D, "LinearVelocity2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{{"Value", {{"X", 0.0f}, {"Y", 0.0f}}}}; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashAngularVelocity2D, "AngularVelocity2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{{"Value", 0.0f}}; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashAcceleration2D, "Acceleration2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{{"Value", {{"X", 0.0f}, {"Y", 0.0f}}}}; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashPhysicsMaterial2D, "PhysicsMaterial2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"Friction", 0.5f}, {"Restitution", 0.0f}, {"PositionCorrectPercent", 0.2f}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashCircleCollider2D, "CircleCollider2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"Radius", 0.5f},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashBoxCollider2D, "BoxCollider2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Rotation", 0.0f},
                {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashShapeCircle2D, "ShapeCircle2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"Radius", 0.5f},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 1.0f}, {"Filled", false}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashShapeBox2D, "ShapeBox2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 0.1f}, {"Filled", false}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashShapeLine2D, "ShapeLine2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"A", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"B", {{"X", 1.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 1.0f}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashLight2D, "Light2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"LightType", 0},
                {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
                {"Direction", {{"X", 0.0f}, {"Y", -1.0f}, {"Z", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Intensity", 1.0f}, {"Range", 10.0f}, {"CastsShadows", false}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashAnimationState2D, "AnimationState2D"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"CurrentFrame", 0}, {"TimeAccumulator", 0.0f}, {"Finished", false}
            }; 
            };
        }
        
        if (const auto id = GetComponentIdFromHashOrWarn(kHashAudioSource, "AudioSource"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{
                {"CueId", 0 },
                {"CuePath", "" },
                {"Volume", 1.0f },
                {"Pitch", 1.0f },
                {"Loop", false },
                {"PlayOnStart", false },
                {"Spatial3D", true },
                {"Bus", 2 },
                {"Pan", 0.0f },
                {"EnableLowPass", false },
                {"LowPassGain", 1.0f },
                {"EnableFadeIn", false },
                {"EnableFadeOut", false },
                {"FadeInDuration", 1.0f },
                {"FadeOutDuration", 1.0f }
            }; 
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashLayer, "Layer"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() { 
                return nlohmann::json{{"Id", 0}}; 
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashMaterial2D, "Material2D"); id != ECS::NULL_COMPONENT_ID) {
            // Register Material2D type metadata.
            defaults[ECS::ComponentRegistry::Type<Material2D>()] = []() {
                return nlohmann::json{
                {"NormalTextureId", 0},
                {"MRATextureId", 0},
                {"Metallic", 0.0f},
                {"Smoothness", 0.5f},
                {"AOStrength", 1.0f},
                {"NormalStrength", 1.0f},
                {"AlphaCutoff", 0.5f},
                {"Flags", 0}
            };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUICanvas, "GUICanvas"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                    {"ReferenceSize", {{"X", 1920.0f}, {"Y", 1080.0f}}},
                    {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                    {"ScaleMode", 0}
                };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIRenderMode, "GUIRenderMode"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                    {"Space", 0}
                };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIElement, "GUIElement"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                {"Position", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Size", {{"X", 100.0f}, {"Y", 100.0f}}},
                {"Visible", true},
                {"Alignment", 0},
                {"ZOrder", 0},
                {"Margin", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 0.0f}}},
                {"Padding", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 0.0f}}}
            };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIPanel, "GUIPanel"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                {"Color", {{"R", 0.2f}, {"G", 0.2f}, {"B", 0.2f}, {"A", 1.0f}}},
                {"CornerRadius", 0.0f}
            };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIText, "GUIText"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                {"Text", "Text"},
                {"FontPath", ""},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"FontSize", 24.0f},
                {"Wrap", false},
                {"HAlign", 0},
                {"VAlign", 0}
            };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIImage, "GUIImage"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                {"TexturePath", ""},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"UVRect", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 1.0f}, {"W", 1.0f}}},
                {"ScaleMode", 0},
                {"TextureFilter", 0},
                {"UseSlicing", false},
                {"SliceBorder", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 0.0f}}}
            };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIInput, "GUIInput"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                {"Hovered", false},
                {"Pressed", false},
                {"Clicked", false},
                {"Released", false},
                {"Dragging", false},
                {"Entered", false},
                {"Exited", false}
            };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIStateStyle, "GUIStateStyle"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                {"NormalColor", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"HoverColor", {{"R", 0.9f}, {"G", 0.9f}, {"B", 0.9f}, {"A", 1.0f}}},
                {"PressedColor", {{"R", 0.8f}, {"G", 0.8f}, {"B", 0.8f}, {"A", 1.0f}}},
                {"DisabledColor", {{"R", 0.6f}, {"G", 0.6f}, {"B", 0.6f}, {"A", 0.6f}}}
            };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUIButton, "GUIButton"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                    {"Text", "Button"},
                    {"FontPath", ""},
                    {"IconPath", ""},
                    {"TextColor", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                    {"IconColor", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                    {"FontSize", 24.0f},
                {"CornerRadius", 0.0f},
                {"IconSize", {{"X", 24.0f}, {"Y", 24.0f}}},
                {"IconOffset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Padding", {{"X", 8.0f}, {"Y", 6.0f}, {"Z", 8.0f}, {"W", 6.0f}}},
                {"Disabled", false},
                {"Toggle", false},
                {"Toggled", false}
            };
            };
        }

        if (const auto id = GetComponentIdFromHashOrWarn(kHashGUISlider, "GUISlider"); id != ECS::NULL_COMPONENT_ID) {
            defaults[id] = []() {
                return nlohmann::json{
                {"Value", 0.0f},
                {"Min", 0.0f},
                {"Max", 1.0f},
                {"Step", 0.0f},
                {"TrackColor", {{"R", 0.2f}, {"G", 0.2f}, {"B", 0.2f}, {"A", 1.0f}}},
                {"FillColor", {{"R", 0.4f}, {"G", 0.4f}, {"B", 0.4f}, {"A", 1.0f}}},
                {"KnobColor", {{"R", 0.9f}, {"G", 0.9f}, {"B", 0.9f}, {"A", 1.0f}}},
                {"CornerRadius", 0.0f},
                {"KnobSize", {{"X", 16.0f}, {"Y", 16.0f}}},
                {"Padding", {{"X", 6.0f}, {"Y", 6.0f}, {"Z", 6.0f}, {"W", 6.0f}}},
                {"Horizontal", true},
                {"Disabled", false},
                {"ValueChanged", false}
            };
            };
        }
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

// Initialize the default component editor registry.
static void _initializeDefaultRegistry() {
    if (!s_registry.empty()) return;  // Already initialized
    
    using namespace ECS::Components;
    
    // Add all hardcoded C++ components with their specialized operations
    s_registry = {
        // Transform: cannot be deleted (every entity has one)
        {
            "Transform", "LocalTransform", "ECS::Components::LocalTransform",
            GetComponentIdFromHashOrWarn(kHashLocalTransform, "LocalTransform"), kHashLocalTransform, false, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLocalTransform(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
                {"Rotation", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 1.0f}}},
                {"Scale", {{"X", 1.0f}, {"Y", 1.0f}, {"Z", 1.0f}}}
            }; }),
            COMPONENT_OPS_HASH(LocalTransform, kHashLocalTransform)
        },
        // Name
        {
            "Name", "Name", "ECS::Components::Name",
            GetComponentIdFromHashOrWarn(kHashName, "Name"), kHashName, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderName(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{{"Value", "Entity"}}; }),
            COMPONENT_OPS_HASH(Name, kHashName)
        },
        // Active
        {
            "Active", "Active", "ECS::Components::Active",
            GetComponentIdFromHashOrWarn(kHashActive, "Active"), kHashActive, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderActive(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{{"Enabled", true}}; }),
            COMPONENT_OPS_HASH(Active, kHashActive)
        },
        // Tag Mask
        {
            "Tag Mask", "TagMask", "ECS::Components::TagMask",
            GetComponentIdFromHashOrWarn(kHashTagMask, "TagMask"), kHashTagMask, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderTagMask(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{{"Mask", 0}}; }),
            COMPONENT_OPS_HASH(TagMask, kHashTagMask)
        },
        // Camera 3D
        {
            "Camera 3D", "Camera3D", "ECS::Components::Camera3D",
            GetComponentIdFromHashOrWarn(kHashCamera3D, "Camera3D"), kHashCamera3D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderCamera3D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"UsePerspective", false}, {"FOV", 45.0f}, {"NearPlane", 0.1f},
                {"FarPlane", 100.0f}, {"OrthoSize", 10.0f},
                {"AspectRatio", 16.0f / 9.0f}, {"Active", false}
            }; }),
            COMPONENT_OPS_HASH(Camera3D, kHashCamera3D)
        },
        // Sprite Renderer 2D
        {
            "Sprite Renderer 2D", "SpriteRenderer2D", "ECS::Components::SpriteRenderer2D",
            GetComponentIdFromHashOrWarn(kHashSpriteRenderer2D, "SpriteRenderer2D"), kHashSpriteRenderer2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderSpriteRenderer2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"TextureId", 0},
                {"NormalTextureId", 0},
                {"EmissiveTextureId", 0},
                {"EmissiveStrength", 5.0f},
                {"EmissiveTexturePath", ""},
                {"TextureFilter", 0},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Tiling", {{"X", 1.0f}, {"Y", 1.0f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Width", 0}, {"Height", 0}
            }; }),
            COMPONENT_OPS_HASH(SpriteRenderer2D, kHashSpriteRenderer2D)
        },
        // Map component types to editor metadata.
        // Tile Map (stores asset paths + sizing; editing handled by Tile Palette)
        {
            "Tile Map", "TileMapComponent", "ECS::Components::TileMapComponent",
            GetComponentIdFromHashOrWarn(kHashTileMapComponent, "TileMapComponent"), kHashTileMapComponent, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderTileMapComponent(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"TileMapPath", ""},
                {"TilesetTexturePath", ""},
                {"TileWorldSize", 1.0f},
                {"TilePixelSize", 32},
                {"DefaultWidth", 64},
                {"DefaultHeight", 64},
                {"LayerIndex", 0},
                {"Visible", true}
            }; }),
            COMPONENT_OPS_HASH(TileMapComponent, kHashTileMapComponent)
        },
        // Sprite Sheet Animation 2D
        {
            "Sprite Sheet Animation 2D", "SpriteSheetAnimation2D", "ECS::Components::SpriteSheetAnimation2D",
            GetComponentIdFromHashOrWarn(kHashSpriteSheetAnimation2D, "SpriteSheetAnimation2D"), kHashSpriteSheetAnimation2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderSpriteSheetAnimation2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"TextureId", 0}, {"NormalTextureId", 0}, {"FrameWidth", 32}, {"FrameHeight", 32},
                {"SheetWidth", 256}, {"SheetHeight", 256},
                {"StartFrame", 0}, {"FrameCount", 1}, {"FramesPerSecond", 10.0f},
                {"Row", 0}, {"FrameOffset", 0}, {"FrameLength", 0},
                {"Loop", true}, {"Playing", false}, {"UseRow", false}, {"UseSegments", false}, {"Segments", nlohmann::json::array()},
                {"TextureFilter", 0}
            }; }),
            COMPONENT_OPS_HASH(SpriteSheetAnimation2D, kHashSpriteSheetAnimation2D)
        },
        // Z-Index 2D
        {
            "Z-Index 2D", "ZIndex2D", "ECS::Components::ZIndex2D",
            GetComponentIdFromHashOrWarn(kHashZIndex2D, "ZIndex2D"), kHashZIndex2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderZIndex2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{{"ZOrder", 0}}; }),
            COMPONENT_OPS_HASH(ZIndex2D, kHashZIndex2D)
        },
        // Rigidbody 2D
        {
            "Rigidbody 2D", "Rigidbody2D", "ECS::Components::Rigidbody2D",
            GetComponentIdFromHashOrWarn(kHashRigidbody2D, "Rigidbody2D"), kHashRigidbody2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderRigidbody2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"Mass", 1.0f}, {"InverseMass", 1.0f},
                {"LinearDamping", 0.0f}, {"AngularDamping", 0.0f},
                {"GravityScale", 1.0f}, {"Flags", 0}
            }; }),
            COMPONENT_OPS_HASH(Rigidbody2D, kHashRigidbody2D)
        },
        // Linear Velocity 2D
        {
            "Linear Velocity 2D", "LinearVelocity2D", "ECS::Components::LinearVelocity2D",
            GetComponentIdFromHashOrWarn(kHashLinearVelocity2D, "LinearVelocity2D"), kHashLinearVelocity2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLinearVelocity2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{{"Value", {{"X", 0.0f}, {"Y", 0.0f}}}}; }),
            COMPONENT_OPS_HASH(LinearVelocity2D, kHashLinearVelocity2D)
        },
        // Angular Velocity 2D
        {
            "Angular Velocity 2D", "AngularVelocity2D", "ECS::Components::AngularVelocity2D",
            GetComponentIdFromHashOrWarn(kHashAngularVelocity2D, "AngularVelocity2D"), kHashAngularVelocity2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAngularVelocity2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{{"Value", 0.0f}}; }),
            COMPONENT_OPS_HASH(AngularVelocity2D, kHashAngularVelocity2D)
        },
        // Acceleration 2D
        {
            "Acceleration 2D", "Acceleration2D", "ECS::Components::Acceleration2D",
            GetComponentIdFromHashOrWarn(kHashAcceleration2D, "Acceleration2D"), kHashAcceleration2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAcceleration2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{{"Value", {{"X", 0.0f}, {"Y", 0.0f}}}}; }),
            COMPONENT_OPS_HASH(Acceleration2D, kHashAcceleration2D)
        },
        // Physics Material 2D
        {
            "Physics Material 2D", "PhysicsMaterial2D", "ECS::Components::PhysicsMaterial2D",
            GetComponentIdFromHashOrWarn(kHashPhysicsMaterial2D, "PhysicsMaterial2D"), kHashPhysicsMaterial2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderPhysicsMaterial2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"Friction", 0.5f}, {"Restitution", 0.0f}, {"PositionCorrectPercent", 0.2f}
            }; }),
            COMPONENT_OPS_HASH(PhysicsMaterial2D, kHashPhysicsMaterial2D)
        },
        // Circle Collider 2D
        {
            "Circle Collider 2D", "CircleCollider2D", "ECS::Components::CircleCollider2D",
            GetComponentIdFromHashOrWarn(kHashCircleCollider2D, "CircleCollider2D"), kHashCircleCollider2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderCircleCollider2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"Radius", 0.5f},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
            }; }),
            COMPONENT_OPS_HASH(CircleCollider2D, kHashCircleCollider2D)
        },
        // Box Collider 2D
        {
            "Box Collider 2D", "BoxCollider2D", "ECS::Components::BoxCollider2D",
            GetComponentIdFromHashOrWarn(kHashBoxCollider2D, "BoxCollider2D"), kHashBoxCollider2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderBoxCollider2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Rotation", 0.0f},
                {"LayerMask", 0xFFFFFFFFu}, {"Flags", 0}
            }; }),
            COMPONENT_OPS_HASH(BoxCollider2D, kHashBoxCollider2D)
        },
        // Shape Circle 2D
        {
            "Shape Circle", "ShapeCircle2D", "ECS::Components::ShapeCircle2D",
            GetComponentIdFromHashOrWarn(kHashShapeCircle2D, "ShapeCircle2D"), kHashShapeCircle2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeCircle2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"Radius", 0.5f},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 1.0f}, {"Filled", false}
            }; }),
            COMPONENT_OPS_HASH(ShapeCircle2D, kHashShapeCircle2D)
        },
        // Shape Box 2D
        {
            "Shape Box", "ShapeBox2D", "ECS::Components::ShapeBox2D",
            GetComponentIdFromHashOrWarn(kHashShapeBox2D, "ShapeBox2D"), kHashShapeBox2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeBox2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
                {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 0.1f}, {"Filled", false}
            }; }),
            COMPONENT_OPS_HASH(ShapeBox2D, kHashShapeBox2D)
        },
        // Shape Line 2D
        {
            "Shape Line", "ShapeLine2D", "ECS::Components::ShapeLine2D",
            GetComponentIdFromHashOrWarn(kHashShapeLine2D, "ShapeLine2D"), kHashShapeLine2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderShapeLine2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"A", {{"X", 0.0f}, {"Y", 0.0f}}},
                {"B", {{"X", 1.0f}, {"Y", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Thickness", 1.0f}
            }; }),
            COMPONENT_OPS_HASH(ShapeLine2D, kHashShapeLine2D)
        },
        // Light 2D
        {
            "Light 2D", "Light2D", "ECS::Components::Light2D",
            GetComponentIdFromHashOrWarn(kHashLight2D, "Light2D"), kHashLight2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLight2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"LightType", 0},
                {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
                {"Direction", {{"X", 0.0f}, {"Y", -1.0f}, {"Z", 0.0f}}},
                {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
                {"Intensity", 1.0f}, {"Range", 10.0f}, {"CastsShadows", false}
            }; }),
            COMPONENT_OPS_HASH(Light2D, kHashLight2D)
        },
        // Animation State 2D
        {
            "Animation State 2D", "AnimationState2D", "ECS::Components::AnimationState2D",
            GetComponentIdFromHashOrWarn(kHashAnimationState2D, "AnimationState2D"), kHashAnimationState2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderAnimationState2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                {"CurrentFrame", 0}, {"TimeAccumulator", 0.0f}, {"Finished", false}
            }; }),
            COMPONENT_OPS_HASH(AnimationState2D, kHashAnimationState2D)
        },
        // Audio Source
        {
            "Audio Source", "AudioSource", "ECS::Components::AudioSource",
            GetComponentIdFromHashOrWarn(kHashAudioSource, "AudioSource"), kHashAudioSource, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& data, ECS::Entity e, ECS::World* w) { ui.RenderAudioSource(data, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "CueId", 0 },
                { "CuePath", "" },
                { "Volume", 1.0f },
                { "Pitch", 1.0f },
                { "Loop", false },
                { "PlayOnStart", false },
                { "Spatial3D", true },
                { "Bus", 2 },
                { "Pan", 0.0f },
                { "EnableLowPass", false },
                { "LowPassGain", 1.0f },
                { "EnableFadeIn", false },
                { "EnableFadeOut", false },
                { "FadeInDuration", 1.0f },
                { "FadeOutDuration", 1.0f }
            }; }),
            COMPONENT_OPS_HASH(AudioSource, kHashAudioSource)
        },
        // Layer
        {
            "Layer 2D", "Layer", "ECS::Components::Layer",
            GetComponentIdFromHashOrWarn(kHashLayer, "Layer"), kHashLayer, false, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderLayer2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{{"Id", 0}}; }),
            COMPONENT_OPS_HASH(Layer, kHashLayer)
        },
        // Material2D
        {
			"Material 2D", "Material2D", "ECS::Components::Material2D",
			GetComponentIdFromHashOrWarn(kHashMaterial2D, "Material2D"), kHashMaterial2D, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderMaterial2D(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "NormalTextureId", 0 },
                { "MRA_TextureId", 0 },
                { "NormalTexturePath", "" },
                { "MRA_TexturePath", "" },
                { "Metallic", 0.0f },
                { "Smoothness", 0.5f },
                { "AOStrength", 1.0f },
                { "NormalStrength", 1.0f },
                { "AlphaCutoff", 0.5f },
                { "Flags", 0 }
            }; }),
            COMPONENT_OPS_HASH(Material2D, kHashMaterial2D)
        },
        // Boid Flock
        {
            "Boid Flock",                    // DisplayName
            "BoidFlock",                     // TypeName
            "ECS::Components::BoidFlock",    // FullTypeName
            GetComponentIdFromHashOrWarn(kHashBoidFlock, "BoidFlock"),
            kHashBoidFlock,
            true,                            // CanDelete
            true,                            // IsBuiltin (C++ component)
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>(
                [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) {
                    ui.RenderBoidFlock(d, e, w);
                }
            ),
            static_cast<std::function<nlohmann::json()>>([]() {
                return nlohmann::json{
                    {"count", 5000},
                    {"separationWeight", 1.5f},
                    {"alignmentWeight", 1.0f},
                    {"cohesionWeight", 1.0f},
                    {"visualRange", 50.0f},
                    {"maxSpeed", 200.0f},
                    {"maxForce", 10.0f},
                    {"boidSize", 0.3f},
                    {"TexturePath", ""}
                };
            }),
            COMPONENT_OPS_HASH(BoidFlock, kHashBoidFlock)
        },
        // GUI Canvas
        {
            "GUI Canvas", "GUICanvas", "ECS::Components::GUICanvas",
            GetComponentIdFromHashOrWarn(kHashGUICanvas, "GUICanvas"), kHashGUICanvas, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUICanvas(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "ReferenceSize", {{"X", 1920.0f}, {"Y", 1080.0f}} },
                { "Offset", {{"X", 0.0f}, {"Y", 0.0f}} },
                { "ScaleMode", 0 }
            }; }),
            COMPONENT_OPS_HASH(GUICanvas, kHashGUICanvas)
        },
        // GUI Render Mode
        {
            "GUI Render Mode", "GUIRenderMode", "ECS::Components::GUIRenderMode",
            GetComponentIdFromHashOrWarn(kHashGUIRenderMode, "GUIRenderMode"), kHashGUIRenderMode, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIRenderMode(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "Space", 0 }
            }; }),
            COMPONENT_OPS_HASH(GUIRenderMode, kHashGUIRenderMode)
        },
        // GUI Element
        {
            "GUI Element", "GUIElement", "ECS::Components::GUIElement",
            GetComponentIdFromHashOrWarn(kHashGUIElement, "GUIElement"), kHashGUIElement, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIElement(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "Position", {{"X", 0.0f}, {"Y", 0.0f}} },
                { "Size", {{"X", 100.0f}, {"Y", 100.0f}} },
                { "Visible", true },
                { "Alignment", 0 },
                { "ZOrder", 0 },
                { "Margin", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 0.0f}} },
                { "Padding", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 0.0f}} }
            }; }),
            COMPONENT_OPS_HASH(GUIElement, kHashGUIElement)
        },
        // GUI Panel
        {
            "GUI Panel", "GUIPanel", "ECS::Components::GUIPanel",
            GetComponentIdFromHashOrWarn(kHashGUIPanel, "GUIPanel"), kHashGUIPanel, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIPanel(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "Color", {{"R", 0.2f}, {"G", 0.2f}, {"B", 0.2f}, {"A", 1.0f}} },
                { "CornerRadius", 0.0f }
            }; }),
            COMPONENT_OPS_HASH(GUIPanel, kHashGUIPanel)
        },
        // GUI Text
        {
            "GUI Text", "GUIText", "ECS::Components::GUIText",
            GetComponentIdFromHashOrWarn(kHashGUIText, "GUIText"), kHashGUIText, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIText(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "Text", "Text" },
                { "FontPath", "" },
                { "Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}} },
                { "FontSize", 24.0f },
                { "Wrap", false },
                { "HAlign", 0 },
                { "VAlign", 0 }
            }; }),
            COMPONENT_OPS_HASH(GUIText, kHashGUIText)
        },
        // GUI State Style
        {
            "GUI State Style", "GUIStateStyle", "ECS::Components::GUIStateStyle",
            GetComponentIdFromHashOrWarn(kHashGUIStateStyle, "GUIStateStyle"), kHashGUIStateStyle, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIStateStyle(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "NormalColor", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}} },
                { "HoverColor", {{"R", 0.9f}, {"G", 0.9f}, {"B", 0.9f}, {"A", 1.0f}} },
                { "PressedColor", {{"R", 0.8f}, {"G", 0.8f}, {"B", 0.8f}, {"A", 1.0f}} },
                { "DisabledColor", {{"R", 0.6f}, {"G", 0.6f}, {"B", 0.6f}, {"A", 0.6f}} }
            }; }),
            COMPONENT_OPS_HASH(GUIStateStyle, kHashGUIStateStyle)
        },
        // GUI Image
        {
            "GUI Image", "GUIImage", "ECS::Components::GUIImage",
            GetComponentIdFromHashOrWarn(kHashGUIImage, "GUIImage"), kHashGUIImage, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIImage(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "TexturePath", "" },
                { "Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}} },
                { "UVRect", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 1.0f}, {"W", 1.0f}} },
                { "ScaleMode", 0 },
                { "TextureFilter", 0 },
                { "UseSlicing", false },
                { "SliceBorder", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 0.0f}} }
            }; }),
            COMPONENT_OPS_HASH(GUIImage, kHashGUIImage)
        },
        // GUI Input
        {
            "GUI Input", "GUIInput", "ECS::Components::GUIInput",
            GetComponentIdFromHashOrWarn(kHashGUIInput, "GUIInput"), kHashGUIInput, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIInput(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "Hovered", false },
                { "Pressed", false },
                { "Clicked", false },
                { "Released", false },
                { "Dragging", false },
                { "Entered", false },
                { "Exited", false }
            }; }),
            COMPONENT_OPS_HASH(GUIInput, kHashGUIInput)
        },
        // GUI Button
        {
            "GUI Button", "GUIButton", "ECS::Components::GUIButton",
            GetComponentIdFromHashOrWarn(kHashGUIButton, "GUIButton"), kHashGUIButton, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUIButton(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "Text", "Button" },
                { "FontPath", "" },
                { "IconPath", "" },
                { "TextColor", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}} },
                { "IconColor", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}} },
                { "FontSize", 24.0f },
                { "CornerRadius", 0.0f },
                { "IconSize", {{"X", 24.0f}, {"Y", 24.0f}} },
                { "IconOffset", {{"X", 0.0f}, {"Y", 0.0f}} },
                { "Padding", {{"X", 8.0f}, {"Y", 6.0f}, {"Z", 8.0f}, {"W", 6.0f}} },
                { "Disabled", false },
                { "Toggle", false },
                { "Toggled", false }
            }; }),
            COMPONENT_OPS_HASH(GUIButton, kHashGUIButton)
        },
        // GUI Slider
        {
            "GUI Slider", "GUISlider", "ECS::Components::GUISlider",
            GetComponentIdFromHashOrWarn(kHashGUISlider, "GUISlider"), kHashGUISlider, true, true,
            static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>([](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGUISlider(d, e, w); }),
            // Serialize component JSON.
            static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json{
                { "Value", 0.0f },
                { "Min", 0.0f },
                { "Max", 1.0f },
                { "Step", 0.0f },
                { "TrackColor", {{"R", 0.2f}, {"G", 0.2f}, {"B", 0.2f}, {"A", 1.0f}} },
                { "FillColor", {{"R", 0.4f}, {"G", 0.4f}, {"B", 0.4f}, {"A", 1.0f}} },
                { "KnobColor", {{"R", 0.9f}, {"G", 0.9f}, {"B", 0.9f}, {"A", 1.0f}} },
                { "CornerRadius", 0.0f },
                { "KnobSize", {{"X", 16.0f}, {"Y", 16.0f}} },
                { "Padding", {{"X", 6.0f}, {"Y", 6.0f}, {"Z", 6.0f}, {"W", 6.0f}} },
                { "Horizontal", true },
                { "Disabled", false },
                { "ValueChanged", false }
            }; }),
            COMPONENT_OPS_HASH(GUISlider, kHashGUISlider)
        },
    };
}

// Rebuild the editor registry from the native ECS registry
// For managed components discovered after startup, add them with generic operations
void ComponentRegistryUI::RebuildFromNativeRegistry() {
    std::lock_guard<std::mutex> lock(s_registryLock);
    
    // First, remove all managed components (those with IsBuiltin = false)
    // Keep only hardcoded C++ components (IsBuiltin = true)
    auto newEnd = std::remove_if(s_registry.begin(), s_registry.end(),
        [](const ComponentUIMetadata& meta) {
            return !meta.IsBuiltin;  // Remove non-builtin (C#) components
        }
    );
    s_registry.erase(newEnd, s_registry.end());
    
    LOG_INFO("[EditorComponentRegistry] RebuildFromNativeRegistry: Cleared old managed components, keeping " << s_registry.size() << " hardcoded C++ components");
    
    // Initialize with hardcoded C++ components on first call
    if (s_registry.empty()) {
        _initializeDefaultRegistry();
    }
    
    // Get all registered component IDs from the native registry
    auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
    LOG_DEBUG("[EditorComponentRegistry] RebuildFromNativeRegistry: Found " << allIds.size() << " total component IDs in native registry");
    
    // Add C++ components that aren't hardcoded
    int newComponentsFound = 0;
    for (ECS::ComponentTypeId id : allIds) {
        const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
        
        // Skip invalid/uninitialized entries (hash 0x0 is impossible for real components)
        if (nativeMeta.TypeHash == 0) {
            continue;
        }
        
        // Check if this component is already in our registry by comparing hash or ComponentTypeId
        // Also update builtin entries that were initialized before the native registry was ready.
        bool alreadyExists = false;
        for (auto& meta : s_registry) {
            if (meta.TypeHash != 0 && meta.TypeHash == nativeMeta.TypeHash) {
                if (meta.ComponentId != id) {
                    meta.ComponentId = id;
                    LOG_DEBUG("[EditorComponentRegistry] Updated component ID for hash 0x" << std::hex << nativeMeta.TypeHash << std::dec << " to ID " << id);
                }
                alreadyExists = true;
                break;
            }
            if (meta.ComponentId == id) {
                alreadyExists = true;
                break;
            }
        }
        
        if (alreadyExists) {
            continue;  // Skip components that are already in the registry
        }

        if (!nativeMeta.IsManaged) {
            LOG_DEBUG("[EditorComponentRegistry] Skipping unmanaged component without editor metadata: ID " << id << ", hash 0x" << std::hex << nativeMeta.TypeHash << std::dec);
            continue;
        }

        // This is a new managed component - add it with generic operations
        LOG_DEBUG("[EditorComponentRegistry] Found new managed component: ID " << id << ", hash 0x" << std::hex << nativeMeta.TypeHash << std::dec << ", size " << nativeMeta.Size);
        newComponentsFound++;
        
        std::string displayName;
        std::string typeName;
        
        // Try to get the component name from the native registry
        std::string nativeName = ECS::ComponentRegistry::GetComponentNameFromHash(nativeMeta.TypeHash);
        if (!nativeName.empty()) {
            // Use the actual component name if available
            displayName = nativeName;
            // Extract just the class name from the full type name (e.g., "Project.Scripts.Health" -> "Health")
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
        
        // Create generic operations for managed components
        auto hasComponentFunc = [id](ECS::World* w, ECS::Entity e) {
            if (!w) return false;
            return w->HasById(e, id);
        };
        
        auto addComponentFunc = [id](ECS::World* w, ECS::Entity e, const nlohmann::json& data) {
            (void)data; // Unused parameter
            
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
                LOG_INFO("[EditorComponentRegistry] Successfully added managed component ID " << id << " to entity (ptr=" << result << ", size=" << meta.Size << ")");
                
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
            
            // If component doesn't exist, add it first
            if (!componentPtr) {
                std::vector<uint8_t> buffer(meta.Size, 0);
                w->AddComponentById(e, id, buffer.data(), meta.Size);
                componentPtr = w->GetRawComponentPtr(e, id);
                
                if (!componentPtr) {
                    LOG_ERROR("[EditorComponentRegistry] Failed to add component for deserialization (ID " << id << ")");
                    return;
                }
            }
            
            // Serialize the JSON to a string for the managed deserializer
            std::string jsonStr = data.dump();
            
            // Call the managed deserializer to populate component fields from JSON
            // Pass the type hash (not the component ID) so C# can look up the type
            if (s_deserializeComponentCallback) {
                s_deserializeComponentCallback(meta.TypeHash, componentPtr, static_cast<int>(meta.Size), jsonStr.c_str());
                // LOG_INFO("[EditorComponentRegistry] Applied managed component (ID " << id << ", hash 0x" << std::hex << meta.TypeHash << std::dec << ") from JSON data");
            }
            else {
                LOG_WARNING("[EditorComponentRegistry] No deserialize callback available for managed component (ID " << id << ")");
            }
        };
        
        // Add to registry with generic renderer (or specialized overrides)
        auto renderFunc = static_cast<std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)>>(
            [](ComponentUI& ui, nlohmann::json& d, ECS::Entity e, ECS::World* w) { ui.RenderGenericComponent(d, e, w); }
        );
        auto defaultFunc = static_cast<std::function<nlohmann::json()>>([]() { return nlohmann::json::object(); });

        s_registry.emplace_back(
            displayName,
            typeName,
            typeName,
            id,
            nativeMeta.TypeHash,
            true,  // Managed components can be deleted
            false,  // IsBuiltin - false because this is a dynamically discovered managed component
            renderFunc,
            defaultFunc,
            static_cast<std::function<bool(ECS::World*, ECS::Entity)>>(hasComponentFunc),
            static_cast<std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)>>(addComponentFunc),
            static_cast<std::function<void(ECS::World*, ECS::Entity)>>(removeComponentFunc),
            static_cast<std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)>>(applyComponentFunc)
        );
        
        LOG_DEBUG("[EditorComponentRegistry] Added managed component to editor registry: " << displayName);
    }
    
    LOG_INFO("[EditorComponentRegistry] RebuildFromNativeRegistry complete: Found " << newComponentsFound << " new managed components. Total in editor registry = " << s_registry.size());
}

// Returns the full component metadata list (C++ + managed components)
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
    // LOG_WARNING("[EditorComponentRegistry] Find: No metadata for component type '" << typeName << "'. Registry has " << s_registry.size() << " components.");
    return nullptr;
}
