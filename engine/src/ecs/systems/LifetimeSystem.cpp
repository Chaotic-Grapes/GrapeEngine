/* Start Header *****************************************************************/
/*!
\file   LifetimeSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th October 2025
\brief
Implements the LifetimeSystem which manages entity lifetimes in the ECS framework.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/LifetimeSystem.h"

namespace ECS {
    SystemMetadata LifetimeSystem::GetMetadata() const {
        SystemMetadata metadata;
        metadata.Name = "Lifetime";
        metadata.ReadComponents = {ComponentRegistry::Type<Components::Lifetime>(), ComponentRegistry::Type<Components::Active>()};
        metadata.WriteComponents = {ComponentRegistry::Type<Components::Lifetime>()};
        metadata.ExecutionOrder = 100;
        return metadata;
    }

    void LifetimeSystem::OnUpdate(World& world, const float dt) {
        std::vector<Entity> toDestroy;
        world.Each<Components::Lifetime>([&](const Entity entity, Components::Lifetime& life) {
            // Treat entities without Active as enabled by default
            if (const auto* active = world.TryGet<Components::Active>(entity)) {
                if (!active->Enabled)
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
            if (const auto* active = world.TryGet<Components::Active>(entity)) {
                if (!active->Enabled)
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
