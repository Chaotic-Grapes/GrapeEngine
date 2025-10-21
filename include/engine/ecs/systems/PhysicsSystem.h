#ifndef PHYSICS2D_H
#define PHYSICS2D_H

#include "ecs/World.h"

namespace ECS {
    class PhysicsSystem {
    public:
        static void Update(World& world, float dt);
    };
}

#endif