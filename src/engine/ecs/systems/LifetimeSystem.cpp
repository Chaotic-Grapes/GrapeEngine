#include "ecs/systems/LifetimeSystem.h"

namespace ECS {
    // Decrease Lifetime and destroy expired entities (safe: destroys after iteration)
    void LifetimeSystem::Update(World& world, const float dt) {
        std::vector<Entity> toDestroy;
        world.Each<Components::Lifetime>([&](const Entity entity, Components::Lifetime& life) {
            // Treat entities without Active as enabled by default
            if (world.Has<Components::Active>(entity)) {
                const auto& active = world.Get<Components::Active>(entity);
                if (!active.Enabled)
                    return;
            }

            life.Time -= dt;
            if (life.Time <= 0.0f) {
                toDestroy.push_back(entity);
            }
        });
        for (const Entity entity : toDestroy) {
            world.Destroy(entity);
        }
    }

    [[deprecated("LifetimeSystem::Update() takes all layers into account automatically.")]]
    void LifetimeSystem::UpdateForLayer(World& world, const float dt, const uint16_t layerId) {
        std::vector<Entity> toDestroy;
        world.Each<Components::Lifetime, Components::Layer>([&](const Entity entity, Components::Lifetime& life, const Components::Layer& layer) {
            if (layer.Id != layerId)
                return;

            // Optional-active check: only skip if Active exists and is disabled
            if (world.Has<Components::Active>(entity)) {
                const auto& active = world.Get<Components::Active>(entity);
                if (!active.Enabled)
                    return;
            }

            life.Time -= dt;

            if (life.Time <= 0.0f) {
                toDestroy.push_back(entity);
            }
        });

        for (const Entity entity : toDestroy) {
            world.Destroy(entity);
        }
    }
}
