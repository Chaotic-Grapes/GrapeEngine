#include "ecs/systems/LifetimeSystem.h"

namespace ECS {
    // Decrease Lifetime and destroy expired entities (safe: destroys after iteration)
    void LifetimeSystem::Update(World& world, const float dt) {
        std::vector<Entity> toDestroy;
        world.Each<Components::Lifetime, Components::Active>([&](const Entity entity, Components::Lifetime& life, const Components::Active& active) {
            if (!active.Enabled)
                return;

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
