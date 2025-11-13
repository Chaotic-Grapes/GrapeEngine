/* Start Header *****************************************************************/
/*!
\file    Hierarchy.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the Hierarchy
class, responsible for updating the world transforms of entities. It provides
methods for traversing the entity hierarchy and computing world transforms.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef HIERARCHY_H
#define HIERARCHY_H

#include "ecs/World.h"
#include "ecs/Entity.h"
#include "math/Matrix4x4.h"
#include "helpers/TransformUtils.h"
#include <vector>
#include <optional>

namespace ECS {
    // Hierarchy class for managing entity transforms in a parent-child hierarchy
    class Hierarchy {
    public:
        /**
         * @brief Update the world transforms of all entities in the hierarchy.
         * This function traverses the entity hierarchy starting from root entities
         * (entities without a parent) and updates their world transforms based on
         * their local transforms and the transforms of their parents.
         * @param world The ECS world containing the entities and their components.
         */
        static void UpdateTransforms(World& world) {
            std::vector<Entity> roots; // Entities without parents

            // Find all root entities
            // Then recursively process their subtrees
            world.Each<Components::LocalTransform, Components::WorldTransform>([&](const Entity e, Components::LocalTransform&, Components::WorldTransform&) {
                if (!world.Has<Parent>(e) || world.Get<Parent>(e).ParentEntity.IsNull()) {
                    roots.push_back(e); // No parent, it's a root
                }
            });
            
            // Recursively update each root entity and its subtree
            // Starting with no parent world transform
            // This will propagate down the hierarchy correctly
            for (Entity r : roots) {
                _updateSubtree(world, r, std::nullopt);
            }
        }

    private:
        // Recursive helper to update an entity and its children
        // parentWorld is optional; if not provided, entity is root
        // This function computes the world transform for the entity
        // and then recurses for its children
        static void _updateSubtree(World& world, const Entity e, const std::optional<Matrix4x4>& parentWorld) {
            // Get local and world transform components
            const auto &lt = world.Get<Components::LocalTransform>(e);
            auto &wt = world.Get<Components::WorldTransform>(e);

            // Compute world transform
            const Matrix4x4 local = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);
            Matrix4x4 worldM = parentWorld.has_value()  // If we have a parent world transform
                ? (parentWorld.value() * local)         // Combine parent world with local
                : local;                                // No parent, local is world

            // Update world transform component
            // Mark as clean so that the system knows it is unchanged
            wt.Matrix = worldM; 
            wt.Dirty = false;
            
            // Recurse for children
            // For each child entity, call _updateSubtree with current world transform
            // This propagates the transform down the hierarchy
            world.ForChildren(e, [&](const Entity c) {
                _updateSubtree(world, c, worldM);
            });
        }
    };
}

#endif
