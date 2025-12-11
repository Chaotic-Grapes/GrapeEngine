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
        ComponentRegistry::RegisterWithHash<Components::Name>(FNV1a_Hash("Name"));
        ComponentRegistry::RegisterWithHash<Components::TagMask>(FNV1a_Hash("TagMask"));
        ComponentRegistry::RegisterWithHash<Components::Active>(FNV1a_Hash("Active"));
        ComponentRegistry::RegisterWithHash<Components::PrefabLink>(FNV1a_Hash("PrefabLink"));
        ComponentRegistry::RegisterWithHash<Components::PrefabInstanceMetadata>(FNV1a_Hash("PrefabInstanceMetadata"));
        ComponentRegistry::RegisterWithHash<Components::Lifetime>(FNV1a_Hash("Lifetime"));
        ComponentRegistry::RegisterWithHash<Components::Layer>(FNV1a_Hash("Layer"));

        // Transforms
        ComponentRegistry::RegisterWithHash<Components::LocalTransform>(FNV1a_Hash("LocalTransform"));
        ComponentRegistry::RegisterWithHash<Components::WorldTransform>(FNV1a_Hash("WorldTransform"));

        // 3D Kinematics/Physics
        ComponentRegistry::RegisterWithHash<Components::Velocity>(FNV1a_Hash("Velocity"));
        ComponentRegistry::RegisterWithHash<Components::Acceleration>(FNV1a_Hash("Acceleration"));
        ComponentRegistry::RegisterWithHash<Components::AngularVelocity>(FNV1a_Hash("AngularVelocity"));
        ComponentRegistry::RegisterWithHash<Components::Rigidbody>(FNV1a_Hash("Rigidbody"));
        ComponentRegistry::RegisterWithHash<Components::PhysicsMaterial2D>(FNV1a_Hash("PhysicsMaterial2D"));
        ComponentRegistry::RegisterWithHash<Components::BoxCollider>(FNV1a_Hash("BoxCollider"));
        ComponentRegistry::RegisterWithHash<Components::SphereCollider>(FNV1a_Hash("SphereCollider"));

        // 2D Kinematics/Physics
        ComponentRegistry::RegisterWithHash<Components::LinearVelocity2D>(FNV1a_Hash("LinearVelocity2D"));
        ComponentRegistry::RegisterWithHash<Components::Acceleration2D>(FNV1a_Hash("Acceleration2D"));
        ComponentRegistry::RegisterWithHash<Components::AngularVelocity2D>(FNV1a_Hash("AngularVelocity2D"));
        ComponentRegistry::RegisterWithHash<Components::Rigidbody2D>(FNV1a_Hash("Rigidbody2D"));
        ComponentRegistry::RegisterWithHash<Components::BoxCollider2D>(FNV1a_Hash("BoxCollider2D"));
        ComponentRegistry::RegisterWithHash<Components::CircleCollider2D>(FNV1a_Hash("CircleCollider2D"));

        // Rendering
        ComponentRegistry::RegisterWithHash<Components::SpriteRenderer2D>(FNV1a_Hash("SpriteRenderer2D"));
        ComponentRegistry::RegisterWithHash<Components::SpriteFlip2D>(FNV1a_Hash("SpriteFlip2D"));
        ComponentRegistry::RegisterWithHash<Components::SpriteShader2D>(FNV1a_Hash("SpriteShader2D"));

        // Animation
        ComponentRegistry::RegisterWithHash<Components::SpriteSheetAnimation2D>(FNV1a_Hash("SpriteSheetAnimation2D"));
        ComponentRegistry::RegisterWithHash<Components::AnimationState2D>(FNV1a_Hash("AnimationState2D"));

        // Debug Shapes
        ComponentRegistry::RegisterWithHash<Components::ShapeCircle2D>(FNV1a_Hash("ShapeCircle2D"));
        ComponentRegistry::RegisterWithHash<Components::ShapeBox2D>(FNV1a_Hash("ShapeBox2D"));
        ComponentRegistry::RegisterWithHash<Components::ShapeLine2D>(FNV1a_Hash("ShapeLine2D"));
        ComponentRegistry::RegisterWithHash<Components::ZIndex2D>(FNV1a_Hash("ZIndex2D"));

        // Cameras
        ComponentRegistry::RegisterWithHash<Components::Camera3D>(FNV1a_Hash("Camera3D"));
        ComponentRegistry::RegisterWithHash<Components::CameraEditor3D>(FNV1a_Hash("CameraEditor3D"));
        ComponentRegistry::RegisterWithHash<Components::CameraMatrices>(FNV1a_Hash("CameraMatrices"));

        // Lighting
        ComponentRegistry::RegisterWithHash<Components::Light2D>(FNV1a_Hash("Light2D"));

        // Audio
        ComponentRegistry::RegisterWithHash<Components::AudioSource>(FNV1a_Hash("AudioSource"));
    }
}
