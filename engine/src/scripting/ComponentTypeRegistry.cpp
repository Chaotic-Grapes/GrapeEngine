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
    namespace {
        template<typename T>
        void RegisterComponentWithName(const char* name) {
            uint32_t hash = FNV1a_Hash(name);
            ComponentRegistry::RegisterWithHash<T>(hash);
            ComponentRegistry::RegisterName(hash, name);
        }
    }

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
        RegisterComponentWithName<Components::Name>("Name");
        RegisterComponentWithName<Components::TagMask>("TagMask");
        RegisterComponentWithName<Components::Active>("Active");
        RegisterComponentWithName<Components::PrefabLink>("PrefabLink");
        RegisterComponentWithName<Components::PrefabInstanceMetadata>("PrefabInstanceMetadata");
        RegisterComponentWithName<Components::Layer>("Layer");

        // Transforms
        RegisterComponentWithName<Components::LocalTransform>("LocalTransform");
        RegisterComponentWithName<Components::WorldTransform>("WorldTransform");

        // 3D Kinematics/Physics
        RegisterComponentWithName<Components::Velocity>("Velocity");
        RegisterComponentWithName<Components::Acceleration>("Acceleration");
        RegisterComponentWithName<Components::AngularVelocity>("AngularVelocity");
        RegisterComponentWithName<Components::Rigidbody>("Rigidbody");
        RegisterComponentWithName<Components::PhysicsMaterial2D>("PhysicsMaterial2D");
        RegisterComponentWithName<Components::BoxCollider>("BoxCollider");
        RegisterComponentWithName<Components::SphereCollider>("SphereCollider");

        // 2D Kinematics/Physics
        RegisterComponentWithName<Components::LinearVelocity2D>("LinearVelocity2D");
        RegisterComponentWithName<Components::Acceleration2D>("Acceleration2D");
        RegisterComponentWithName<Components::AngularVelocity2D>("AngularVelocity2D");
        RegisterComponentWithName<Components::Rigidbody2D>("Rigidbody2D");
        RegisterComponentWithName<Components::BoxCollider2D>("BoxCollider2D");
        RegisterComponentWithName<Components::CircleCollider2D>("CircleCollider2D");

        // Rendering
        RegisterComponentWithName<Components::SpriteRenderer2D>("SpriteRenderer2D");
        RegisterComponentWithName<Components::SpriteFlip2D>("SpriteFlip2D");
        RegisterComponentWithName<Components::SpriteShader2D>("SpriteShader2D");

        // Animation
        RegisterComponentWithName<Components::SpriteSheetAnimation2D>("SpriteSheetAnimation2D");
        RegisterComponentWithName<Components::AnimationState2D>("AnimationState2D");

        // Debug Shapes
        RegisterComponentWithName<Components::ShapeCircle2D>("ShapeCircle2D");
        RegisterComponentWithName<Components::ShapeBox2D>("ShapeBox2D");
        RegisterComponentWithName<Components::ShapeLine2D>("ShapeLine2D");
        RegisterComponentWithName<Components::ZIndex2D>("ZIndex2D");

        // Cameras
        RegisterComponentWithName<Components::Camera3D>("Camera3D");
        RegisterComponentWithName<Components::CameraEditor3D>("CameraEditor3D");
        RegisterComponentWithName<Components::CameraMatrices>("CameraMatrices");

        // Lighting
        RegisterComponentWithName<Components::Light2D>("Light2D");

        // Audio
        RegisterComponentWithName<Components::AudioSource>("AudioSource");

        ComponentRegistry::LogAllComponentHashesCritical("native registry build");
    }
}
