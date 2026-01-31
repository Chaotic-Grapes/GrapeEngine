/* Start Header *****************************************************************/
/*!
\file    ComponentTypeRegistry.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Central registration of all engine component types with hash mapping for C# interop.
This file registers all C++ component types with their FNV-1a hash values to enable
component access from C# scripts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/ComponentRegistry.h"
#include "ecs/Components.h"
#include "ecs/events/EventComponents.h"
#include <cstdint>

namespace {
    // FNV-1a hash implementation matching C# side
    constexpr uint32_t FNV1a_Hash(const char* str) {
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint32_t>(*str);
            hash *= 16777619u;
            ++str;
        }
        return hash;
    }
}

namespace ECS {
#define REGISTER_COMPONENT(T) \
    ComponentRegistry::RegisterWithHash<Components::T>(FNV1a_Hash(#T), #T); \
    ComponentRegistry::EnsureHashMapping(FNV1a_Hash(#T), ComponentRegistry::Type<Components::T>(), #T)
#define REGISTER_EVENT_COMPONENT(T) \
    ComponentRegistry::RegisterWithHash<Events::T>(FNV1a_Hash(#T), #T); \
    ComponentRegistry::EnsureHashMapping(FNV1a_Hash(#T), ComponentRegistry::Type<Events::T>(), #T)

#if defined(_DEBUG) || defined(DEBUG)
    namespace {
        void LogComponentTypeIds(const char* name, uint32_t hash) {
            const ComponentTypeId registryId = ComponentRegistry::GetComponentIdFromHash(hash);

            if (registryId == NULL_COMPONENT_ID) {
                LOG_WARNING("[ComponentTypeRegistry] Missing registry ID for " << name << " (hash=0x" << std::hex << hash << std::dec << ")");
            } else {
                LOG_INFO("[ComponentTypeRegistry] Registered " << name << " (hash=0x" << std::hex << hash << std::dec
                    << ", id=" << registryId << ")");
            }
        }
    }
#endif

    /**
     * @brief Register all engine component types with their hash values.
     * 
     * This function must be called during engine initialization BEFORE any
     * C# scripts attempt to access components. The hash values are computed
     * using FNV-1a algorithm and must match the C# side implementation.
     * 
     * Component names MUST match exactly between C++ and C#:
     * - C++: ECS::Components::Position
     * - C#:  Position (without namespace)
     */
    void RegisterAllComponentTypes() {
        // Core utility/tag components
        REGISTER_COMPONENT(Name);
        REGISTER_COMPONENT(TagMask);
        REGISTER_COMPONENT(Active);
        REGISTER_COMPONENT(Parent);
        REGISTER_COMPONENT(PrefabLink);
        REGISTER_COMPONENT(PrefabInstanceMetadata);
        REGISTER_COMPONENT(Layer);

        // Transforms
        REGISTER_COMPONENT(LocalTransform);
        REGISTER_COMPONENT(WorldTransform);

        // 3D Kinematics/Physics
        REGISTER_COMPONENT(Velocity);
        REGISTER_COMPONENT(Acceleration);
        REGISTER_COMPONENT(AngularVelocity);
        REGISTER_COMPONENT(Rigidbody);
        REGISTER_COMPONENT(PhysicsMaterial2D);
        REGISTER_COMPONENT(BoxCollider);
        REGISTER_COMPONENT(SphereCollider);

        // 2D Kinematics/Physics
        REGISTER_COMPONENT(LinearVelocity2D);
        REGISTER_COMPONENT(Acceleration2D);
        REGISTER_COMPONENT(AngularVelocity2D);
        REGISTER_COMPONENT(Rigidbody2D);
        REGISTER_COMPONENT(BoxCollider2D);
        REGISTER_COMPONENT(CircleCollider2D);

        // Rendering
        REGISTER_COMPONENT(SpriteRenderer2D);
        REGISTER_COMPONENT(SpriteFlip2D);
        REGISTER_COMPONENT(SpriteShader2D);

        // Animation
        REGISTER_COMPONENT(SpriteSheetAnimation2D);
        REGISTER_COMPONENT(AnimationState2D);

        // Debug Shapes
        REGISTER_COMPONENT(ShapeCircle2D);
        REGISTER_COMPONENT(ShapeBox2D);
        REGISTER_COMPONENT(ShapeLine2D);
        REGISTER_COMPONENT(ZIndex2D);

        // Cameras
        REGISTER_COMPONENT(Camera3D);
        REGISTER_COMPONENT(CameraEditor3D);
        REGISTER_COMPONENT(CameraMatrices);

        // Lighting
        REGISTER_COMPONENT(Light2D);

        // Audio
        REGISTER_COMPONENT(AudioSource);

        // Material2D
		REGISTER_COMPONENT(Material2D);

        // GUI
        REGISTER_COMPONENT(GUIElement);
        REGISTER_COMPONENT(GUIContainer);
        REGISTER_COMPONENT(GUIPanel);
        REGISTER_COMPONENT(GUIButton);
        REGISTER_COMPONENT(GUIInputField);
        REGISTER_COMPONENT(GUISlider);
        REGISTER_COMPONENT(GUICheckbox);
        REGISTER_COMPONENT(GUIDropdown);
        REGISTER_COMPONENT(GUIScrollView);
        REGISTER_COMPONENT(GUISeparator);
        REGISTER_COMPONENT(GUIText);
        REGISTER_COMPONENT(GUILayoutGroup);
        REGISTER_COMPONENT(GUITooltip);

        // Event buffers (physics events)
        REGISTER_EVENT_COMPONENT(CollisionEventBuffer);
        REGISTER_EVENT_COMPONENT(TriggerEventBuffer);
        REGISTER_EVENT_COMPONENT(CollisionExitEventBuffer);
        REGISTER_EVENT_COMPONENT(TriggerExitEventBuffer);

#if defined(_DEBUG) || defined(DEBUG)
        // Log the registered component types for debugging
        LogComponentTypeIds("Name", FNV1a_Hash("Name"));
        LogComponentTypeIds("TagMask", FNV1a_Hash("TagMask"));
        LogComponentTypeIds("Active", FNV1a_Hash("Active"));
        LogComponentTypeIds("Parent", FNV1a_Hash("Parent"));
        LogComponentTypeIds("PrefabLink", FNV1a_Hash("PrefabLink"));
        LogComponentTypeIds("PrefabInstanceMetadata", FNV1a_Hash("PrefabInstanceMetadata"));
        LogComponentTypeIds("Layer", FNV1a_Hash("Layer"));

        LogComponentTypeIds("LocalTransform", FNV1a_Hash("LocalTransform"));
        LogComponentTypeIds("WorldTransform", FNV1a_Hash("WorldTransform"));

        LogComponentTypeIds("Velocity", FNV1a_Hash("Velocity"));
        LogComponentTypeIds("Acceleration", FNV1a_Hash("Acceleration"));
        LogComponentTypeIds("AngularVelocity", FNV1a_Hash("AngularVelocity"));
        LogComponentTypeIds("Rigidbody", FNV1a_Hash("Rigidbody"));
        LogComponentTypeIds("PhysicsMaterial2D", FNV1a_Hash("PhysicsMaterial2D"));
        LogComponentTypeIds("BoxCollider", FNV1a_Hash("BoxCollider"));
        LogComponentTypeIds("SphereCollider", FNV1a_Hash("SphereCollider"));

        LogComponentTypeIds("LinearVelocity2D", FNV1a_Hash("LinearVelocity2D"));
        LogComponentTypeIds("Acceleration2D", FNV1a_Hash("Acceleration2D"));
        LogComponentTypeIds("AngularVelocity2D", FNV1a_Hash("AngularVelocity2D"));
        LogComponentTypeIds("Rigidbody2D", FNV1a_Hash("Rigidbody2D"));
        LogComponentTypeIds("BoxCollider2D", FNV1a_Hash("BoxCollider2D"));
        LogComponentTypeIds("CircleCollider2D", FNV1a_Hash("CircleCollider2D"));

        LogComponentTypeIds("SpriteRenderer2D", FNV1a_Hash("SpriteRenderer2D"));
        LogComponentTypeIds("SpriteFlip2D", FNV1a_Hash("SpriteFlip2D"));
        LogComponentTypeIds("SpriteShader2D", FNV1a_Hash("SpriteShader2D"));

        LogComponentTypeIds("SpriteSheetAnimation2D", FNV1a_Hash("SpriteSheetAnimation2D"));
        LogComponentTypeIds("AnimationState2D", FNV1a_Hash("AnimationState2D"));

        LogComponentTypeIds("ShapeCircle2D", FNV1a_Hash("ShapeCircle2D"));
        LogComponentTypeIds("ShapeBox2D", FNV1a_Hash("ShapeBox2D"));
        LogComponentTypeIds("ShapeLine2D", FNV1a_Hash("ShapeLine2D"));
        LogComponentTypeIds("ZIndex2D", FNV1a_Hash("ZIndex2D"));

        LogComponentTypeIds("Camera3D", FNV1a_Hash("Camera3D"));
        LogComponentTypeIds("CameraEditor3D", FNV1a_Hash("CameraEditor3D"));
        LogComponentTypeIds("CameraMatrices", FNV1a_Hash("CameraMatrices"));

        LogComponentTypeIds("Light2D", FNV1a_Hash("Light2D"));
        LogComponentTypeIds("AudioSource", FNV1a_Hash("AudioSource"));
        LogComponentTypeIds("Material2D", FNV1a_Hash("Material2D"));
        LogComponentTypeIds("GUIElement", FNV1a_Hash("GUIElement"));
        LogComponentTypeIds("GUIContainer", FNV1a_Hash("GUIContainer"));
        LogComponentTypeIds("GUIPanel", FNV1a_Hash("GUIPanel"));
        LogComponentTypeIds("GUIButton", FNV1a_Hash("GUIButton"));
        LogComponentTypeIds("GUIInputField", FNV1a_Hash("GUIInputField"));
        LogComponentTypeIds("GUISlider", FNV1a_Hash("GUISlider"));
        LogComponentTypeIds("GUICheckbox", FNV1a_Hash("GUICheckbox"));
        LogComponentTypeIds("GUIDropdown", FNV1a_Hash("GUIDropdown"));
        LogComponentTypeIds("GUIScrollView", FNV1a_Hash("GUIScrollView"));
        LogComponentTypeIds("GUISeparator", FNV1a_Hash("GUISeparator"));
        LogComponentTypeIds("GUIText", FNV1a_Hash("GUIText"));
        LogComponentTypeIds("GUILayoutGroup", FNV1a_Hash("GUILayoutGroup"));
        LogComponentTypeIds("GUITooltip", FNV1a_Hash("GUITooltip"));

        LogComponentTypeIds("CollisionEventBuffer", FNV1a_Hash("CollisionEventBuffer"));
        LogComponentTypeIds("TriggerEventBuffer", FNV1a_Hash("TriggerEventBuffer"));
        LogComponentTypeIds("CollisionExitEventBuffer", FNV1a_Hash("CollisionExitEventBuffer"));
        LogComponentTypeIds("TriggerExitEventBuffer", FNV1a_Hash("TriggerExitEventBuffer"));
#endif
    }
}

#undef REGISTER_COMPONENT
#undef REGISTER_EVENT_COMPONENT
