/* Start Header *****************************************************************/
/*!
\file   TransformSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implements the TransformSystem which updates world transforms for entity hierarchies.
*/
/* End Header *******************************************************************/

#include "ecs/systems/TransformSystem.h"

namespace ECS {
    SystemMetadata TransformSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Transform");
        // Read accesses
        builder.ReadComponent<Components::LocalTransform>();
        // Write accesses
        builder.WriteComponent<Components::WorldTransform>();
        // Execution parameters
        builder.SetExecutionOrder(50);
        builder.SetGroup(SystemGroup::PrePhysics);
        builder.SetRunMode(SystemRunMode::Always);
        return builder.Build();
    }

    void TransformSystem::OnUpdate(World& world, float /*dt*/) {
        std::vector<Entity> roots;
        std::vector<Entity> needsWorldTransform;

        // First pass: collect entities with LocalTransform that are missing WorldTransform
        world.Each<Components::LocalTransform>([&](const Entity e, Components::LocalTransform&) {
            if (!world.Has<Components::WorldTransform>(e)) {
                needsWorldTransform.push_back(e);
            }
        });

        // Add missing WorldTransform components
        for (Entity e : needsWorldTransform) {
            Components::WorldTransform wt{};
            wt.Dirty = true;
            world.Add<Components::WorldTransform>(e, wt);
        }

        // Find roots (entities without a parent in the world's index)
        world.Each<Components::LocalTransform, Components::WorldTransform>([&](const Entity e, Components::LocalTransform&, Components::WorldTransform&) {
            Entity parent = world.ParentOf(e);
            if (parent.IsNull()) {
                roots.push_back(e);
            }
        });

        // Update subtree for each root
        for (Entity r : roots) {
            _updateSubtree(world, r, std::nullopt);
        }
    }

    void TransformSystem::_updateSubtree(World& world, const Entity e, const std::optional<Matrix4x4>& parentWorld) {
        const auto& lt = world.Get<Components::LocalTransform>(e);
        auto& wt = world.Get<Components::WorldTransform>(e);

        const Matrix4x4 local = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);
        Matrix4x4 worldM = parentWorld.has_value() ? (parentWorld.value() * local) : local;

        wt.Matrix = worldM;
        wt.Dirty = false;

        world.ForChildren(e, [&](const Entity c) {
            _updateSubtree(world, c, worldM);
        });
    }
}
