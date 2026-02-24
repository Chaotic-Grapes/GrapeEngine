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
}
