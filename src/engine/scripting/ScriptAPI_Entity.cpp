/* Start Header *****************************************************************/
/*!
\file   ScriptAPI_Entity.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
C# scripting API exports for entity lifecycle operations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/World.h"
#include "ecs/Components.h"
#include "helpers/EntityUtils.h"
#include <iostream>

// Export macro
#ifndef SCRIPT_API
#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#endif
#endif

// External world access (defined in ScriptAPI_Component.cpp)
extern ECS::World* g_scriptWorld;

// ============================================================================
// Entity API - Lifecycle Operations
// ============================================================================

/// <summary>
/// Create a new entity in the world
/// </summary>
SCRIPT_API uint64_t ScriptAPI_CreateEntity() {
    if (!g_scriptWorld) {
        std::cerr << "[ScriptAPI] World not set" << '\n';
        return 0;
    }

    ECS::Entity entity = g_scriptWorld->Create();
    // Ensure new entities always have a default transform
    g_scriptWorld->Add<ECS::Components::LocalTransform>(entity);
    return ECS::EntityUtils::Pack(entity);
}

/// <summary>
/// Destroy an entity
/// </summary>
SCRIPT_API void ScriptAPI_DestroyEntity(uint64_t entityId) {
    if (!g_scriptWorld) {
        std::cerr << "[ScriptAPI] World not set" << '\n';
        return;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (g_scriptWorld->IsAlive(entity)) {
        g_scriptWorld->Destroy(entity);
    }
}

/// <summary>
/// Check if an entity is alive in the world
/// </summary>
SCRIPT_API bool ScriptAPI_IsAlive(uint64_t entityId) {
    if (!g_scriptWorld) {
        std::cerr << "[ScriptAPI] World not set" << '\n';
        return false;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    return g_scriptWorld->IsAlive(entity);
}
