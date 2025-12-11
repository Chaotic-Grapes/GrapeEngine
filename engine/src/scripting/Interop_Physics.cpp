/* Start Header *****************************************************************/
/*!
\file   Interop_Physics.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
C API exports for physics operations in the ECS World.
This provides physics control for managed C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "helpers/EntityUtils.h"
#include "core/Logger.h"
#include "ecs/systems/PhysicsSystem.h"

// ============================================================================
// Physics Control API
// ============================================================================

/**
 * @brief Set the global gravity for the physics world
 * @param worldPtr Pointer to the ECS World
 * @param x Gravity X component
 */
INTEROP_API void WorldInterop_Physics_SetGravity(void* worldPtr, float x, float y) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return;
    }
    
    // TODO: Implement physics world gravity control
    // This would set gravity in the physics engine (Box2D/Jolt)
}

/**
 * @brief Get the global gravity for the physics world
 * @param worldPtr Pointer to the ECS World
 * @param outX Pointer to receive Gravity X component
 * @param outY Pointer to receive Gravity Y component
 */
INTEROP_API void WorldInterop_Physics_GetGravity(void* worldPtr, float* outX, float* outY) {
    if (!worldPtr || !outX || !outY) {
        LOG_ERROR("[WorldInterop] Invalid parameters");
        return;
    }
    
    // TODO: Implement physics world gravity query
    *outX = 0.0f;
    *outY = -9.81f;
}

/**
 * @brief Enable or disable the physics system
 * @param worldPtr Pointer to the ECS World
 * @param enabled True to enable, false to disable
 */
INTEROP_API void WorldInterop_Physics_SetEnabled(void* worldPtr, bool enabled) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return;
    }
    
    // TODO: Implement physics enable/disable
}

/**
 * @brief Check if the physics system is enabled
 * @param worldPtr Pointer to the ECS World
 * @return true if enabled, false otherwise
 */
INTEROP_API bool WorldInterop_Physics_IsEnabled(void* worldPtr) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return false;
    }
    
    // TODO: Implement physics enabled query
    return true;
}

/**
 * @brief Apply a force to an entity with a Rigidbody2D
 * @param worldPtr Pointer to the ECS World
 * @param entityId Packed entity ID
 * @param forceX Force X component
 * @param forceY Force Y component
 */
INTEROP_API void WorldInterop_Physics_ApplyForce(void* worldPtr, uint64_t entityId, float forceX, float forceY) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return;
    }

    ECS::World* world = static_cast<ECS::World*>(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        LOG_ERROR("[WorldInterop] Entity not alive");
        return;
    }

    auto* rb = world->TryGet<ECS::Components::Rigidbody2D>(entity);
    auto* vel = world->TryGet<ECS::Components::LinearVelocity2D>(entity);
    
    if (!rb || !vel) {
        LOG_WARNING("[WorldInterop] Entity missing Rigidbody2D or LinearVelocity2D");
        return;
    }

    // Apply force (F = ma, so a = F/m, v += a*dt)
    // For immediate force application, add to velocity
    float mass = rb->Mass > 0.0f ? rb->Mass : 1.0f;
    vel->Value.X += forceX / mass;
    vel->Value.Y += forceY / mass;
}

/**
 * @brief Apply an impulse to an entity with a Rigidbody2D
 * @param worldPtr Pointer to the ECS World
 * @param entityId Packed entity ID
 * @param impulseX Impulse X component
 * @param impulseY Impulse Y component
 */
INTEROP_API void WorldInterop_Physics_ApplyImpulse(void* worldPtr, uint64_t entityId, float impulseX, float impulseY) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return;
    }

    ECS::World* world = static_cast<ECS::World*>(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        LOG_ERROR("[WorldInterop] Entity not alive");
        return;
    }

    auto* rb = world->TryGet<ECS::Components::Rigidbody2D>(entity);
    auto* vel = world->TryGet<ECS::Components::LinearVelocity2D>(entity);
    
    if (!rb || !vel) {
        LOG_WARNING("[WorldInterop] Entity missing Rigidbody2D or LinearVelocity2D");
        return;
    }

    // Apply impulse (change in momentum, p = mv, so Δv = impulse/m)
    float mass = rb->Mass > 0.0f ? rb->Mass : 1.0f;
    vel->Value.X += impulseX / mass;
    vel->Value.Y += impulseY / mass;
}

/**
 * @brief Get the velocity of an entity with a Rigidbody2D
 * @param worldPtr Pointer to the ECS World
 * @param entityId Packed entity ID
 * @param outX Pointer to receive Velocity X component
 * @param outY Pointer to receive Velocity Y component
 */
INTEROP_API void WorldInterop_Physics_GetVelocity(void* worldPtr, uint64_t entityId, float* outX, float* outY) {
    if (!worldPtr || !outX || !outY) {
        LOG_ERROR("[WorldInterop] Invalid parameters");
        return;
    }

    ECS::World* world = static_cast<ECS::World*>(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        *outX = 0.0f;
        *outY = 0.0f;
        return;
    }

    auto* vel = world->TryGet<ECS::Components::LinearVelocity2D>(entity);
    if (vel) {
        *outX = vel->Value.X;
        *outY = vel->Value.Y;
    } else {
        *outX = 0.0f;
        *outY = 0.0f;
    }
}

/**
 * @brief Set the velocity of an entity with a Rigidbody2D
 * @param worldPtr Pointer to the ECS World
 * @param entityId Packed entity ID
 * @param x Velocity X component
 * @param y Velocity Y component
 */
INTEROP_API void WorldInterop_Physics_SetVelocity(void* worldPtr, uint64_t entityId, float x, float y) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] Invalid world pointer");
        return;
    }

    ECS::World* world = static_cast<ECS::World*>(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        LOG_ERROR("[WorldInterop] Entity not alive");
        return;
    }

    auto* vel = world->TryGet<ECS::Components::LinearVelocity2D>(entity);
    if (vel) {
        vel->Value.X = x;
        vel->Value.Y = y;
    } else {
        LOG_WARNING("[WorldInterop] Entity missing LinearVelocity2D component");
    }
}
