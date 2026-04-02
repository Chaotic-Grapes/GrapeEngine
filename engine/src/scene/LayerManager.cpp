/* Start Header *****************************************************************/
/*!
\file    LayerManager.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\brief
Out-of-line implementations for LayerManager methods that depend on ECS::World.
*/
/* End Header
********************************************************************************/

#include "scene/LayerManager.h"
#include "ecs/World.h"
#include "ecs/Components.h"

namespace Scenes {
    void LayerManager::PruneDeadEntities(ECS::World& world) {
        for (auto& layer : m_layers) {
            // Erase-while-iterating is linear for unordered_set and does not require assignable keys.
            for (auto it = layer.entities.begin(); it != layer.entities.end(); ) {
                if (!world.IsAlive(*it)) {
                    it = layer.entities.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void LayerManager::_syncCollidersForLayer(uint16_t layerId, ECS::World* world) {
        if (!world || layerId >= m_layers.size())
            return;

        // Get collision mask for this layer
        uint32_t layerMask = m_layers[layerId].collisionMask;

        // Update all entities in this layer
        for (ECS::Entity entity : m_layers[layerId].entities) {
            if (!world->IsAlive(entity))
                continue;

            // Update colliders' LayerMask

            // CircleCollider2D
            if (auto* circle = world->TryGet<ECS::Components::CircleCollider2D>(entity)) {
                circle->LayerMask = layerMask;
            }

            // BoxCollider2D
            if (auto* box = world->TryGet<ECS::Components::BoxCollider2D>(entity)) {
                box->LayerMask = layerMask;
            }
        }
    }
}
