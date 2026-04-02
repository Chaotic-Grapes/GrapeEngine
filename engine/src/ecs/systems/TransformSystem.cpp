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
#include "physics2d/internal/ParallelFor.h"
#include <unordered_set>

namespace {
    constexpr uint32_t kTransformMaxWorkers = 8u;
    constexpr size_t kTransformParallelThreshold = 64u;
}

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

    /**
     * @brief Update world transforms for active hierarchy roots.
     * @param world ECS world containing transform and hierarchy state.
     * @return void
     * @note Uses layer-first traversal when LayerManager is present to amortize per-entity layer checks.
     * @note Entities without a Layer component are still handled through a fallback pass.
     * @complexity O(sum(layer entities) + unlayered local entities)
     */
    void TransformSystem::OnUpdate(World& world) {
        std::vector<Entity> roots;
        std::vector<Entity> needsWorldTransform;
        std::unordered_set<EntityId> visited;

        auto* layerManager = world.GetLayerManager();

        auto collectMissingWorldTransform = [&](const Entity e) {
            if (!world.IsAlive(e)) {
                return;
            }
            if (!visited.insert(e.Index).second) {
                return;
            }
            if (!world.TryGet<Components::LocalTransform>(e)) {
                return;
            }
            if (!world.Has<Components::WorldTransform>(e)) {
                needsWorldTransform.push_back(e);
            }
        };

        if (layerManager) {
            for (const auto layerId : layerManager->DrawOrder()) {
                for (const Entity e : layerManager->EntitiesIn(layerId)) {
                    collectMissingWorldTransform(e);
                }
            }

            // Fallback for entities that have LocalTransform but are not tracked in LayerManager.
            world.Each<Components::LocalTransform>([&](const Entity e, Components::LocalTransform&) {
                if (world.TryGet<Components::Layer>(e)) {
                    return;
                }
                collectMissingWorldTransform(e);
            });
        } else {
            // First pass: collect entities with LocalTransform that are missing WorldTransform.
            world.Each<Components::LocalTransform>([&](const Entity e, Components::LocalTransform&) {
                collectMissingWorldTransform(e);
            });
        }

        // Add missing WorldTransform components
        for (Entity e : needsWorldTransform) {
            Components::WorldTransform wt{};
            wt.Dirty = true;
            world.Add<Components::WorldTransform>(e, wt);
        }

        visited.clear();
        auto collectRoots = [&](const Entity e) {
            if (!world.IsAlive(e)) {
                return;
            }
            if (!visited.insert(e.Index).second) {
                return;
            }
            if (!world.TryGet<Components::LocalTransform>(e) || !world.TryGet<Components::WorldTransform>(e)) {
                return;
            }

            Entity parent = world.ParentOf(e);
            if (parent.IsNull() || !world.IsAlive(parent)) {
                roots.push_back(e);
            }
        };

        if (layerManager) {
            for (const auto layerId : layerManager->DrawOrder()) {
                if (!layerManager->IsUpdateEnabled(layerId)) {
                    continue;
                }
                for (const Entity e : layerManager->EntitiesIn(layerId)) {
                    collectRoots(e);
                }
            }

            // Keep unlayered roots in update flow.
            world.Each<Components::LocalTransform, Components::WorldTransform>([&](const Entity e, Components::LocalTransform&, Components::WorldTransform&) {
                if (world.TryGet<Components::Layer>(e)) {
                    return;
                }
                collectRoots(e);
            });
        } else {
            // Find roots (entities without a parent in the world's index).
            world.Each<Components::LocalTransform, Components::WorldTransform>([&](const Entity e, Components::LocalTransform&, Components::WorldTransform&) {
                collectRoots(e);
            });
        }

        // Update each independent root subtree; parallelize only when root count is large enough.
        if (roots.size() < kTransformParallelThreshold) {
            for (Entity r : roots) {
                _updateSubtree(world, r, std::nullopt);
            }
            return;
        }

        Engine::Physics2D::Internal::ParallelForStatic(roots.size(), kTransformMaxWorkers,
            [this, &world, &roots](size_t begin, size_t end, uint32_t) {
                for (size_t i = begin; i < end; ++i) {
                    _updateSubtree(world, roots[i], std::nullopt);
                }
            });
    }

    void TransformSystem::_updateSubtree(World& world, const Entity e, const std::optional<Matrix4x4>& parentWorld) {
        const auto& lt = world.Get<Components::LocalTransform>(e);
        auto& wt = world.Get<Components::WorldTransform>(e);

        // Layer check: if layer updates are disabled, skip updating this subtree
        auto* layerManager = world.GetLayerManager();
        if (layerManager) {
            const auto* layer = world.TryGet<Components::Layer>(e);
            
            // If the layer is disabled for updates, skip updating this subtree
            if (layer && !layerManager->IsUpdateEnabled(layer->Id)) {
                const std::optional<Matrix4x4> frozenWorld = wt.Matrix;
                world.ForChildren(e, [&](const Entity c) {
                    _updateSubtree(world, c, frozenWorld);
                });
                return;
            }
        }

        const Matrix4x4 local = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);
        Matrix4x4 worldM = parentWorld.has_value() ? (parentWorld.value() * local) : local;

        wt.Matrix = worldM;
        wt.Dirty = false;

        world.ForChildren(e, [&](const Entity c) {
            _updateSubtree(world, c, worldM);
        });
    }
}
