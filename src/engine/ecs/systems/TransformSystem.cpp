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

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/TransformSystem.h"
#include "ecs/Components.h"
#include "helpers/TransformUtils.h"

namespace ECS {
    void TransformSystem::Update(World& world, const float dt) {
        (void)dt;

        // Depth-first propagation from roots to leaves in one pass to avoid repeated passes and reduces lookups on parents
        // Helper DFS to propagate transforms down the hierarchy
        auto propagate = [&](auto&& self, Entity parent, const Matrix4x4& parentMatrix) -> void {
            world.ForChildren(parent, [&](Entity child) {
                if (!world.IsAlive(child))
                    return;
                
                
                auto [lt, wt] = world.TryGetComponents<Components::LocalTransform, Components::WorldTransform>(child);
                if (!lt || !wt) {
                    // Still traverse deeper even if this child doesn't have both components
                    self(self, child, parentMatrix);
                    return;
                }

                const Matrix4x4 localM = TransformUtils::MakeTRS(lt->Position, lt->Rotation, lt->Scale);
                wt->Matrix = parentMatrix * localM;
                wt->Dirty = false;

                self(self, child, wt->Matrix);
            });
        };

        // Initialize roots (no Parent or invalid Parent) and propagate
        world.Each<Components::LocalTransform, Components::WorldTransform>(
            [&](const Entity e, const Components::LocalTransform& lt, Components::WorldTransform& wt) {
                bool isRoot = true;
                
                if (const auto* p = world.TryGet<Parent>(e)) {
                    // Orphan or parent missing required data -> treat as root
                    if (world.IsAlive(p->ParentEntity)) {
                        if (const auto* parentWT = world.TryGet<Components::WorldTransform>(p->ParentEntity)) {
                            isRoot = false;
                        }
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
}
