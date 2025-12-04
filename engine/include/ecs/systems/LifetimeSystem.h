/* Start Header *****************************************************************/
/*!
\file   LifetimeSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th October 2025
\brief
Declares the LifetimeSystem which manages entity lifetimes in the ECS framework.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef LIFETIMESYSTEM_H
#define LIFETIMESYSTEM_H

#include <cstdint>
#include <vector>
#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/ISystem.h"

namespace ECS {
    /**
     * @brief System that decreases Lifetime and destroys expired entities
     * Executes in Update phase with executionOrder=100
     */
    class LifetimeSystem : public ISystem {
    public:
        LifetimeSystem() = default;
        ~LifetimeSystem() override = default;

        // ISystem interface
        void OnCreate(World& world) override {}
        void OnUpdate(World& world, float dt) override;
        void OnDestroy(World& world) override {}
        
        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Update; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }

        // Utility: Update for a specific layer
        static void UpdateForLayer(World& world, float dt, uint16_t layerId);
    };
}

#endif
