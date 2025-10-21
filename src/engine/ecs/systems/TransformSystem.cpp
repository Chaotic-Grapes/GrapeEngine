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
        (void)dt;

        // Depth-first propagation from roots to leaves in one pass.
        // This avoids repeated passes and reduces lookups on parents.

        // Helper DFS to propagate transforms down the hierarchy
        auto propagate = [&](auto&& self, Entity parent, const Matrix4x4& parentMatrix) -> void {
            world.ForChildren(parent, [&](Entity child) {
                if (!world.IsAlive(child))
                    return;
                
                    if (!world.Has<Components::LocalTransform>(child) || !world.Has<Components::WorldTransform>(child)) {
                    // Still traverse deeper even if this child doesn't have both components
                    self(self, child, parentMatrix);
                    return;
                }

                const auto& lt = world.Get<Components::LocalTransform>(child);
                auto& wt = world.Get<Components::WorldTransform>(child);
                const Matrix4x4 localM = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);
                wt.Matrix = parentMatrix * localM;
                wt.Dirty = false;

                self(self, child, wt.Matrix);
            });
        };

        // 1) Initialize roots (no Parent or invalid Parent) and propagate
        world.Each<Components::LocalTransform, Components::WorldTransform>(
            [&](Entity e, const Components::LocalTransform& lt, Components::WorldTransform& wt) {
                bool isRoot = true;
                if (world.Has<Parent>(e)) {
                    const auto& p = world.Get<Parent>(e);
                    // Orphan or parent missing required data -> treat as root
                    if (world.IsAlive(p.ParentEntity) && world.Has<Components::WorldTransform>(p.ParentEntity)) {
                        isRoot = false;
                    }
                }

                if (isRoot) {
                    wt.Matrix = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);
                    wt.Dirty = false;
                    propagate(propagate, e, wt.Matrix);
                }
                else {
                    // Non-root nodes will be handled during parent propagation
                    wt.Dirty = true;
                }
            }
        );
    }
    
    void TransformSystem::UpdateEntityRecursive(World& world, Entity entity, const Matrix4x4& parentMatrix) {
        // This method is no longer used with the new iterative approach
        // Keeping it for potential future use or removal
        (void)world;
        (void)entity;
        (void)parentMatrix;
    }
}
