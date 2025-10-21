#ifndef LIFETIMESYSTEM_H
#define LIFETIMESYSTEM_H

#include <cstdint>
#include <vector>
#include "ecs/World.h"
#include "ecs/Components.h"

namespace ECS {
    class LifetimeSystem {
    public:
        // Decrease Lifetime and destroy expired entities (safe: destroys after iteration)
        static void Update(World& world, float dt);

        // Same, for a specific layer
        static void UpdateForLayer(World& world, float dt, uint16_t layerId);
    };
}

#endif
