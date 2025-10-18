#include "ecs/systems/LifetimeSystem.h"

namespace ECS {
    // Decrease Lifetime and destroy expired entities (safe: destroys after iteration)
    void LifetimeSystem::Update(World& world, float dt) {
        std::vector<Entity> toDestroy;
        world.Each<Components::Lifetime>([&](Entity entity, Components::Lifetime& life) {
            life.Time -= dt;
            if (life.Time <= 0.0f) {
                toDestroy.push_back(entity);
            }
        });
        for (Entity entity : toDestroy) {
            world.Destroy(entity);
        }
    }

    // Same, for a specific layer
    void LifetimeSystem::UpdateForLayer(World& world, float dt, uint16_t layerId) {
        std::vector<Entity> toDestroy;
        world.Each<Components::Lifetime, Layer>([&](Entity entity, Components::Lifetime& life, Layer& layer) {
            if (layer.Id != layerId)
                return;

            life.Time -= dt;
            if (life.Time <= 0.0f) {
                toDestroy.push_back(entity);
            }
        });
        for (Entity entity : toDestroy) {
            world.Destroy(entity);
        }
    }
}
