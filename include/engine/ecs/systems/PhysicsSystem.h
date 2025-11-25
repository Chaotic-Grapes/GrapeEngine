/**
* @Name: Dalton koh, 2403250
* @email: d.koh@digipen.edu
* @file PhysicsSystem.h
* @brief Broad/narrow-phase utilities and per-frame 2D physics update loop.
*
* @details
* Might be edited later on if required to change structure of how physicSystem 
* and a more foolproof version of broad-narrow phase collision is implemented 
* with more specific shape handling or better optimisation.
*/
#ifndef PHYSICS2D_H
#define PHYSICS2D_H

#include "ecs/World.h"
#include <vector>
#include <unordered_map>

namespace ECS {
    class PhysicsSystem {
    public:
        static void Update(World& world, float dt);
        static std::unordered_set<uint64_t> s_previousCollisions;
    private: 

    };
}

#endif