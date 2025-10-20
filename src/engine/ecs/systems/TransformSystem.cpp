/* Start Header *****************************************************************/
/*!
\file   TransformSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th October 2025
\brief
Implements the TransformSystem which handles hierarchical transform propagation.
This system computes WorldTransform for all entities that have both LocalTransform
and WorldTransform components, taking parent-child relationships into account.
*/
/* End Header *******************************************************************/

#include "ecs/systems/TransformSystem.h"
#include "ecs/Components.h"
#include "helpers/TransformUtils.h"
#include "core/Logger.h"

namespace ECS {
    void TransformSystem::Update(World& world, float dt) {
        (void)dt; // Unused parameter
        
        // Strategy: Process all entities with WorldTransform.
        // For entities without a Parent component (root entities), compute world transform from local.
        // For entities with a Parent component, compute world transform as parent * local.
        // Since we iterate in arbitrary order, we may process children before parents.
        // Solution: Mark WorldTransform as dirty initially, then iterate until all are clean.
        
        // First pass: Mark all as dirty and update root entities
        world.Each<Components::LocalTransform, Components::WorldTransform>(
            [&](Entity entity, const Components::LocalTransform& localTransform, Components::WorldTransform& worldTransform) {
                // Check if this entity has a parent
                if (!world.Has<Parent>(entity)) {
                    // Root entity - no parent, so world transform = local transform
                    worldTransform.Matrix = TransformUtils::MakeTRS(
                        localTransform.Position,
                        localTransform.Rotation,
                        localTransform.Scale
                    );
                    worldTransform.Dirty = false;
                } else {
                    // Has a parent - mark as dirty for next pass
                    worldTransform.Dirty = true;
                }
            }
        );
        
        // Second pass: Iteratively update children until all are clean
        // This handles hierarchies of any depth
        const int MAX_ITERATIONS = 100; // Safety limit to prevent infinite loops
        int iteration = 0;
        bool anyDirty = true;
        
        while (anyDirty && iteration < MAX_ITERATIONS) {
            anyDirty = false;
            
            world.Each<Components::LocalTransform, Components::WorldTransform, Parent>(
                [&](Entity entity, const Components::LocalTransform& localTransform, 
                    Components::WorldTransform& worldTransform, const Parent& parent) {
                    
                    if (!worldTransform.Dirty) {
                        return; // Already updated
                    }
                    
                    // Check if parent is alive and has WorldTransform
                    if (!world.IsAlive(parent.ParentEntity)) {
                        // Parent is dead, treat as root entity
                        worldTransform.Matrix = TransformUtils::MakeTRS(
                            localTransform.Position,
                            localTransform.Rotation,
                            localTransform.Scale
                        );
                        worldTransform.Dirty = false;
                        return;
                    }
                    
                    if (!world.Has<Components::WorldTransform>(parent.ParentEntity)) {
                        // Parent doesn't have WorldTransform, treat as root
                        worldTransform.Matrix = TransformUtils::MakeTRS(
                            localTransform.Position,
                            localTransform.Rotation,
                            localTransform.Scale
                        );
                        worldTransform.Dirty = false;
                        return;
                    }
                    
                    const auto& parentWorldTransform = world.Get<Components::WorldTransform>(parent.ParentEntity);
                    
                    if (parentWorldTransform.Dirty) {
                        // Parent not yet updated, skip this entity for now
                        anyDirty = true;
                        return;
                    }
                    
                    // Parent is clean, we can update this entity
                    Matrix4x4 localMatrix = TransformUtils::MakeTRS(
                        localTransform.Position,
                        localTransform.Rotation,
                        localTransform.Scale
                    );
                    
                    worldTransform.Matrix = parentWorldTransform.Matrix * localMatrix;
                    worldTransform.Dirty = false;
                }
            );
            
            iteration++;
        }
        
        if (iteration >= MAX_ITERATIONS) {
            LOG_ERROR("TransformSystem: Maximum iteration limit reached. Possible circular parent-child relationship.");
        }
    }
    
    void TransformSystem::UpdateEntityRecursive(World& world, Entity entity, const Matrix4x4& parentMatrix) {
        // This method is no longer used with the new iterative approach
        // Keeping it for potential future use or removal
        (void)world;
        (void)entity;
        (void)parentMatrix;
    }
}
