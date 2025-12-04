/* Start Header *****************************************************************/
/*!
\file   EngineInterop_Entity.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
C API exports for managed C# scripting systems for entity lifecycle operations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/World.h"
#include "ecs/Components.h"
#include "helpers/EntityUtils.h"
#include "core/Logger.h"

// Export macro for C API
#ifdef _WIN32
    #ifdef BUILDING_ENGINE_INTEROP
        #define ENGINE_INTEROP_API extern "C" __declspec(dllexport)
    #else
        #define ENGINE_INTEROP_API extern "C" __declspec(dllimport)
    #endif
#else
    #define ENGINE_INTEROP_API extern "C"
#endif

// External world access (defined in EngineInterop_Component.cpp)
extern ECS::World* g_scriptWorld;

// ============================================================================
// Entity API - Lifecycle Operations
// ============================================================================

/**
 * @brief Create a new entity in the world
 * @return The packed entity ID of the newly created entity
 */
ENGINE_INTEROP_API uint64_t EngineInterop_CreateEntity() {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return 0;
    }

    ECS::Entity entity = g_scriptWorld->Create();
    // Ensure new entities always have a default transform
    g_scriptWorld->Add<ECS::Components::LocalTransform>(entity);
    return ECS::EntityUtils::Pack(entity);
}

/**
 * @brief Destroy an entity in the world
 * @param entityId The packed entity ID of the entity to destroy
 */
ENGINE_INTEROP_API void EngineInterop_DestroyEntity(uint64_t entityId) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (g_scriptWorld->IsAlive(entity)) {
        g_scriptWorld->Destroy(entity);
    }
}

/**
 * @brief Check if an entity is alive in the world
 * @param entityId The packed entity ID to check
 * @return True if the entity is alive; false otherwise
 */
ENGINE_INTEROP_API bool EngineInterop_IsAlive(uint64_t entityId) {
    if (!g_scriptWorld) {
        LOG_ERROR("[ScriptAPI] World not set");
        return false;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    return g_scriptWorld->IsAlive(entity);
}
