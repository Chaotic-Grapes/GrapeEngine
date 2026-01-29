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
#include "ecs/Entity.h"
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
#if defined(_DEBUG) || defined(DEBUG)
    namespace {
        template<typename T>
        void LogComponentTypeIds(const char* name, uint32_t hash) {
            const ComponentTypeId registryId = ComponentRegistry::GetComponentIdFromHash(hash);
            const ComponentTypeId typeId = TypeIdOf<T>();

            if (registryId == NULL_COMPONENT_ID) {
                LOG_WARNING("[ComponentTypeRegistry] Missing registry ID for " << name << " (hash=0x" << std::hex << hash << std::dec << ")");
            } else if (registryId != typeId) {
                LOG_WARNING("[ComponentTypeRegistry] TypeId mismatch for " << name << " (hash=0x" << std::hex << hash << std::dec
                    << ", registryId=" << registryId << ", typeIdOf=" << typeId << ")");
            } else {
                LOG_INFO("[ComponentTypeRegistry] TypeId match for " << name << " (hash=0x" << std::hex << hash << std::dec
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
        ComponentRegistry::RegisterWithHash<Components::Name>(FNV1a_Hash("Name"));
        ComponentRegistry::RegisterWithHash<Components::TagMask>(FNV1a_Hash("TagMask"));
        ComponentRegistry::RegisterWithHash<Components::Active>(FNV1a_Hash("Active"));
        ComponentRegistry::RegisterWithHash<Components::Parent>(FNV1a_Hash("Parent"));
        ComponentRegistry::RegisterWithHash<Components::PrefabLink>(FNV1a_Hash("PrefabLink"));
        ComponentRegistry::RegisterWithHash<Components::PrefabInstanceMetadata>(FNV1a_Hash("PrefabInstanceMetadata"));
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

#if defined(_DEBUG) || defined(DEBUG)
        // Log the registered component types for debugging
        LogComponentTypeIds<Components::Name>("Name", FNV1a_Hash("Name"));
        LogComponentTypeIds<Components::TagMask>("TagMask", FNV1a_Hash("TagMask"));
        LogComponentTypeIds<Components::Active>("Active", FNV1a_Hash("Active"));
        LogComponentTypeIds<Components::Parent>("Parent", FNV1a_Hash("Parent"));
        LogComponentTypeIds<Components::PrefabLink>("PrefabLink", FNV1a_Hash("PrefabLink"));
        LogComponentTypeIds<Components::PrefabInstanceMetadata>("PrefabInstanceMetadata", FNV1a_Hash("PrefabInstanceMetadata"));
        LogComponentTypeIds<Components::Layer>("Layer", FNV1a_Hash("Layer"));

        LogComponentTypeIds<Components::LocalTransform>("LocalTransform", FNV1a_Hash("LocalTransform"));
        LogComponentTypeIds<Components::WorldTransform>("WorldTransform", FNV1a_Hash("WorldTransform"));

        LogComponentTypeIds<Components::Velocity>("Velocity", FNV1a_Hash("Velocity"));
        LogComponentTypeIds<Components::Acceleration>("Acceleration", FNV1a_Hash("Acceleration"));
        LogComponentTypeIds<Components::AngularVelocity>("AngularVelocity", FNV1a_Hash("AngularVelocity"));
        LogComponentTypeIds<Components::Rigidbody>("Rigidbody", FNV1a_Hash("Rigidbody"));
        LogComponentTypeIds<Components::PhysicsMaterial2D>("PhysicsMaterial2D", FNV1a_Hash("PhysicsMaterial2D"));
        LogComponentTypeIds<Components::BoxCollider>("BoxCollider", FNV1a_Hash("BoxCollider"));
        LogComponentTypeIds<Components::SphereCollider>("SphereCollider", FNV1a_Hash("SphereCollider"));

        LogComponentTypeIds<Components::LinearVelocity2D>("LinearVelocity2D", FNV1a_Hash("LinearVelocity2D"));
        LogComponentTypeIds<Components::Acceleration2D>("Acceleration2D", FNV1a_Hash("Acceleration2D"));
        LogComponentTypeIds<Components::AngularVelocity2D>("AngularVelocity2D", FNV1a_Hash("AngularVelocity2D"));
        LogComponentTypeIds<Components::Rigidbody2D>("Rigidbody2D", FNV1a_Hash("Rigidbody2D"));
        LogComponentTypeIds<Components::BoxCollider2D>("BoxCollider2D", FNV1a_Hash("BoxCollider2D"));
        LogComponentTypeIds<Components::CircleCollider2D>("CircleCollider2D", FNV1a_Hash("CircleCollider2D"));

        LogComponentTypeIds<Components::SpriteRenderer2D>("SpriteRenderer2D", FNV1a_Hash("SpriteRenderer2D"));
        LogComponentTypeIds<Components::SpriteFlip2D>("SpriteFlip2D", FNV1a_Hash("SpriteFlip2D"));
        LogComponentTypeIds<Components::SpriteShader2D>("SpriteShader2D", FNV1a_Hash("SpriteShader2D"));

        LogComponentTypeIds<Components::SpriteSheetAnimation2D>("SpriteSheetAnimation2D", FNV1a_Hash("SpriteSheetAnimation2D"));
        LogComponentTypeIds<Components::AnimationState2D>("AnimationState2D", FNV1a_Hash("AnimationState2D"));

        LogComponentTypeIds<Components::ShapeCircle2D>("ShapeCircle2D", FNV1a_Hash("ShapeCircle2D"));
        LogComponentTypeIds<Components::ShapeBox2D>("ShapeBox2D", FNV1a_Hash("ShapeBox2D"));
        LogComponentTypeIds<Components::ShapeLine2D>("ShapeLine2D", FNV1a_Hash("ShapeLine2D"));
        LogComponentTypeIds<Components::ZIndex2D>("ZIndex2D", FNV1a_Hash("ZIndex2D"));

        LogComponentTypeIds<Components::Camera3D>("Camera3D", FNV1a_Hash("Camera3D"));
        LogComponentTypeIds<Components::CameraEditor3D>("CameraEditor3D", FNV1a_Hash("CameraEditor3D"));
        LogComponentTypeIds<Components::CameraMatrices>("CameraMatrices", FNV1a_Hash("CameraMatrices"));

        LogComponentTypeIds<Components::Light2D>("Light2D", FNV1a_Hash("Light2D"));
        
        LogComponentTypeIds<Components::AudioSource>("AudioSource", FNV1a_Hash("AudioSource"));
#endif
    }
}
