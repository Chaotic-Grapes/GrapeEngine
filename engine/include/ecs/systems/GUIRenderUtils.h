/* Start Header *****************************************************************/
/*!
\file   GUIRenderUtils.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.zulkifli@digipen.edu

\brief  
Shared helpers for GUI render/layout systems.
*/
/* End Header *******************************************************************/

#pragma once

#include <cmath>
#include "ecs/Components.h"
#include "ecs/World.h"

namespace ECS {
    /**
     * @brief Resolve the GUI render space for a given entity by traversing up its parent hierarchy.
     * If the entity or any of its ancestors has a GUIRenderMode component, return its Space value.
     * If no GUIRenderMode is found in the hierarchy, default to Screen space.
     * @param world The ECS world containing the entities and components.
     * @param entity The entity for which to resolve the GUI render space.
     * @return The resolved GUIRenderSpace (Screen or World).
     */
    inline Components::GUIRenderSpace ResolveGUIRenderSpace(const World& world, Entity entity) {
        Entity current = entity;
        int depth = 0;
       
        // Traverse up the parent hierarchy to find GUIRenderMode
        while (!current.IsNull() && depth < 32) {
            if (world.Has<Components::GUIRenderMode>(current)) {
                return world.Get<Components::GUIRenderMode>(current).Space;
            }

            // Move to parent entity
            if (!world.Has<Components::Parent>(current)) {
                break;
            }

            // Get parent and continue
            const auto& parent = world.Get<Components::Parent>(current);
            current = parent.ParentEntity;
            if (current.IsNull() || !world.IsAlive(current)) {
                break;
            }
            // Prevent potential infinite loops by limiting depth
            ++depth;
        }

        // Default to Screen space if no GUIRenderMode found
        return Components::GUIRenderSpace::Screen;
    }

    /**
     * @brief Resolve the effective 2D GUI rotation in radians from transform components.
     *
     * Prefers `LocalTransform::Rotation` because GUI authoring stores intent there.
     * Falls back to extracting yaw-from-Z-axis from `WorldTransform` when local transform
     * is unavailable (legacy or special runtime-only entities).
     *
     * @param world ECS world used for component lookup.
     * @param entity Entity whose transform rotation is sampled.
     * @return Rotation around Z axis in radians.
     * @complexity O(1).
     */
    inline float ResolveGUIRotationRadians(const World& world, Entity entity) {
        if (world.Has<Components::LocalTransform>(entity)) {
            Quaternion rotation = world.Get<Components::LocalTransform>(entity).Rotation;
            rotation.Normalize();
            const float sinZ = 2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y);
            const float cosZ = 1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z);
            return std::atan2(sinZ, cosZ);
        }

        if (world.Has<Components::WorldTransform>(entity)) {
            const Matrix4x4& m = world.Get<Components::WorldTransform>(entity).Matrix;
            return std::atan2(m.m10, m.m00);
        }

        return 0.0f;
    }
}
