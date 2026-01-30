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
#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"
#include <vector>
#include <unordered_map>

namespace ECS {
    /**
     * @brief System that handles 2D physics simulation with broad/narrow-phase collision
     * Executes in Physics phase with executionOrder=0
     */
    class PhysicsSystem : public ISystem {
    public:
        PhysicsSystem() = default;
        ~PhysicsSystem() override = default;

        // ISystem interface
        void OnCreate(World& world) override {}
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;
        
        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Physics; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }

    private:
        std::unordered_set<uint64_t> m_previousCollisions;
        std::unordered_set<uint64_t> m_previousTriggerOverlaps;
    };
}

#endif
