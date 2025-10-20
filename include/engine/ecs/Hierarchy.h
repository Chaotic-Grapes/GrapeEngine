#ifndef HIERARCHY_H
#define HIERARCHY_H

#include "ecs/World.h"
#include "ecs/Entity.h"
#include "math/Matrix4x4.h"
#include "helpers/TransformUtils.h"
#include <vector>
#include <optional>

namespace ECS {
    class Hierarchy {
    public:
        static void UpdateTransforms(World& world) {
            std::vector<Entity> roots;
            world.Each<Components::LocalTransform, Components::WorldTransform>([&](const Entity e, Components::LocalTransform&, Components::WorldTransform&) {
                if (!world.Has<Parent>(e) || world.Get<Parent>(e).ParentEntity.IsNull()) {
                    roots.push_back(e);
                }
            });
            
            for (Entity r : roots) {
                _updateSubtree(world, r, std::nullopt);
            }
        }

    private:
        static void _updateSubtree(World& world, const Entity e, const std::optional<Matrix4x4>& parentWorld) {
            const auto &lt = world.Get<Components::LocalTransform>(e);
            auto &wt = world.Get<Components::WorldTransform>(e);

            const Matrix4x4 local = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);
            Matrix4x4 worldM = parentWorld.has_value() ? (parentWorld.value() * local) : local;

            wt.Matrix = worldM; wt.Dirty = false;
            world.ForChildren(e, [&](const Entity c){
                _updateSubtree(world, c, worldM);
            });
        }
    };
}

#endif
