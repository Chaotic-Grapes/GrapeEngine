/* Start Header *****************************************************************/
/*!
\file    ScriptAPI_Physics.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    20th November 2025
\brief
C# scripting API exports for physics operations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "physics/Physics.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "helpers/EntityUtils.h"
#include "core/Logger.h"

// Export macro
#ifndef SCRIPT_API
#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#endif
#endif

// External world access (defined in ScriptAPI_Component.cpp)
extern ECS::World* g_scriptWorld;

// ============================================================================
// Physics API - Global Settings
// ============================================================================

/// <summary>
/// Set the global gravity vector
/// </summary>
SCRIPT_API void ScriptAPI_Physics_SetGravity(float x, float y) {
    Engine::Physics::SetGravity(Vector2D(x, y));
}

/// <summary>
/// Get the current global gravity vector
/// </summary>
SCRIPT_API void ScriptAPI_Physics_GetGravity(float* outX, float* outY) {
    Vector2D gravity = Engine::Physics::GetGravity();
    if (outX) *outX = gravity.X;
    if (outY) *outY = gravity.Y;
}

/// <summary>
/// Enable or disable the physics system
/// </summary>
SCRIPT_API void ScriptAPI_Physics_SetEnabled(bool enabled) {
    Engine::Physics::SetEnabled(enabled);
}

/// <summary>
/// Check if the physics system is enabled
/// </summary>
SCRIPT_API bool ScriptAPI_Physics_IsEnabled() {
    return Engine::Physics::IsEnabled();
}

// ============================================================================
// Physics API - Force Application
// ============================================================================

/// <summary>
/// Apply a force to an entity (gradual acceleration)
/// </summary>
SCRIPT_API void ScriptAPI_Physics_ApplyForce(uint64_t entityId, float forceX, float forceY) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (!g_scriptWorld->IsAlive(entity)) {
        LOG_ERROR("[ScriptAPI] Entity not alive");
        return;
    }

    auto* rb = g_scriptWorld->TryGet<ECS::Components::Rigidbody2D>(entity);
    auto* vel = g_scriptWorld->TryGet<ECS::Components::LinearVelocity2D>(entity);

    if (!rb || !vel) {
        LOG_ERROR("[ScriptAPI] Entity missing Rigidbody2D or LinearVelocity2D component");
        return;
    }

    Engine::Physics::ApplyForce(*rb, *vel, Vector2D(forceX, forceY));
}

/// <summary>
/// Apply an impulse to an entity (instant velocity change)
/// </summary>
SCRIPT_API void ScriptAPI_Physics_ApplyImpulse(uint64_t entityId, float impulseX, float impulseY) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (!g_scriptWorld->IsAlive(entity)) {
        LOG_ERROR("[ScriptAPI] Entity not alive");
        return;
    }

    auto* rb = g_scriptWorld->TryGet<ECS::Components::Rigidbody2D>(entity);
    auto* vel = g_scriptWorld->TryGet<ECS::Components::LinearVelocity2D>(entity);

    if (!rb || !vel) {
        LOG_ERROR("[ScriptAPI] Entity missing Rigidbody2D or LinearVelocity2D component");
        return;
    }

    Engine::Physics::ApplyImpulse(*rb, *vel, Vector2D(impulseX, impulseY));
}

/// <summary>
/// Get the velocity of an entity
/// </summary>
SCRIPT_API void ScriptAPI_Physics_GetVelocity(uint64_t entityId, float* outX, float* outY) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (!g_scriptWorld->IsAlive(entity)) {
        LOG_ERROR("[ScriptAPI] Entity not alive");
        return;
    }

    auto* vel = g_scriptWorld->TryGet<ECS::Components::LinearVelocity2D>(entity);
    if (!vel) {
        if (outX) *outX = 0.0f;
        if (outY) *outY = 0.0f;
        return;
    }

    if (outX) *outX = vel->Value.X;
    if (outY) *outY = vel->Value.Y;
}

/// <summary>
/// Set the velocity of an entity
/// </summary>
SCRIPT_API void ScriptAPI_Physics_SetVelocity(uint64_t entityId, float x, float y) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (!g_scriptWorld->IsAlive(entity)) {
        LOG_ERROR("[ScriptAPI] Entity not alive");
        return;
    }

    auto* vel = g_scriptWorld->TryGet<ECS::Components::LinearVelocity2D>(entity);
    if (!vel) {
        // Add component if it doesn't exist
        g_scriptWorld->Add<ECS::Components::LinearVelocity2D>(entity, ECS::Components::LinearVelocity2D{ Vector2D(x, y) });
    }
    else {
        vel->Value.X = x;
        vel->Value.Y = y;
    }
}
