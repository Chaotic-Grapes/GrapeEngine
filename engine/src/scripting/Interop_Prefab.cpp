/* Start Header *****************************************************************/
/*!
\file    Interop_Prefab.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of PrefabManager interop functions for C# scripting.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "core/Application.h"
#include "scene/SceneManager.h"
#include "scene/Scene.h"
#include "ecs/PrefabManager.h"
#include <string>
#include <cstring>

// ============================================================================
// PrefabManager Lifecycle
// ============================================================================

/**
 * @brief Get the PrefabManager instance from the Engine core
 * @return void* Pointer to the PrefabManager, or nullptr on error
 */
INTEROP_API void* PrefabManagerInterop_GetInstance() {
    if (!Engine::CORE)
        return nullptr;
    
    Scenes::SceneManager& sceneMgr = Engine::CORE->GetSceneManager();
    return static_cast<void*>(sceneMgr.GetPrefabManager());
}

/**
 * @brief Get the PrefabManager from a specific Scene
 * @param scenePtr Pointer to the Scene
 * @return void* Pointer to the PrefabManager, or nullptr on error
 */
INTEROP_API void* PrefabManagerInterop_GetFromScene(void* scenePtr) {
    if (!scenePtr)
        return nullptr;
    
    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    return static_cast<void*>(scene->GetWorld().GetPrefabManager());
}

// ============================================================================
// Prefab Registration & Hash Lookup
// ============================================================================

/**
 * @brief Register a prefab path and get its hash
 * @param prefabManagerPtr Pointer to the PrefabManager
 * @param path The prefab file path
 * @return uint32_t The FNV-1a hash of the prefab path
 */
INTEROP_API uint32_t PrefabManagerInterop_RegisterPrefab(void* prefabManagerPtr, const char* path) {
    if (!prefabManagerPtr || !path)
        return 0;

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    return manager->RegisterPrefab(path);
}

/**
 * @brief Resolve a prefab hash back to its registered path.
 * @param prefabManagerPtr Pointer to the PrefabManager.
 * @param hash FNV-1a hash of the prefab path.
 * @return const char* Internal path string pointer, or empty string on error.
 */
INTEROP_API const char* PrefabManagerInterop_GetPrefabPath(void* prefabManagerPtr, uint32_t hash) {
    if (!prefabManagerPtr)
        return "";

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    const std::string& path = manager->GetPrefabPath(hash);
    
    // NOTE: This returns a pointer to internal memory. Caller must copy immediately.
    return path.c_str();
}

/**
 * @brief Check whether a prefab hash is registered
 *
 * @param prefabManagerPtr Pointer to the PrefabManager
 * @param hash FNV-1a hash of the prefab path
 * @return true if registered, false otherwise
 */
INTEROP_API bool PrefabManagerInterop_IsRegistered(void* prefabManagerPtr, uint32_t hash) {
    if (!prefabManagerPtr)
        return false;

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    return manager->IsRegistered(hash);
}

// ============================================================================
// Prefab Instantiation
// ============================================================================

/**
 * @brief Instantiate a prefab and return the created entity id.
 * @param prefabManagerPtr Pointer to the PrefabManager.
 * @param path Path to the prefab asset.
 * @return uint32_t Entity id of the instantiated prefab, or 0 on failure.
 */
INTEROP_API uint32_t PrefabManagerInterop_Instantiate(void* prefabManagerPtr, const char* path) {
    if (!prefabManagerPtr || !path)
        return 0;

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    ECS::Entity entity = manager->Instantiate(path);
    
    return entity.IsNull() ? 0 : entity.Index;
}

/**
 * @brief Instantiate a prefab and return the created entity id
 *
 * @param prefabManagerPtr Pointer to the PrefabManager
 * @param path Path to the prefab asset
 * @param parentEntityId Entity id of the parent
 * @return uint32_t Entity id of the instantiated prefab, or 0 on failure
 */
INTEROP_API uint32_t PrefabManagerInterop_InstantiateAsChild(void* prefabManagerPtr, const char* path, uint32_t parentEntityId) {
    if (!prefabManagerPtr || !path)
        return 0;

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    
    // Construct parent entity from ID
    ECS::Entity parent;
    parent.Index = parentEntityId;
    
    ECS::Entity entity = manager->InstantiateAsChild(path, parent);
    
    return entity.IsNull() ? 0 : entity.Index;
}

// ============================================================================
// Instance Tracking
// ============================================================================

/**
 * @brief Track an instantiated entity as a prefab instance.
 * @param prefabManagerPtr Pointer to the PrefabManager.
 * @param entityId Entity id to track.
 * @param prefabHash FNV-1a hash of the source prefab path.
 */
INTEROP_API void PrefabManagerInterop_TrackInstance(void* prefabManagerPtr, uint32_t entityId, uint32_t prefabHash) {
    if (!prefabManagerPtr)
        return;

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    
    ECS::Entity entity;
    entity.Index = entityId;
    
    manager->TrackInstance(entity, prefabHash);
}

/**
 * @brief Untrack an entity instance previously tracked to a prefab
 *
 * @param prefabManagerPtr Pointer to the PrefabManager
 * @param entityId Entity id to untrack
 */
INTEROP_API void PrefabManagerInterop_UntrackInstance(void* prefabManagerPtr, uint32_t entityId) {
    if (!prefabManagerPtr)
        return;

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    
    ECS::Entity entity;
    entity.Index = entityId;
    
    manager->UntrackInstance(entity);
}

/**
 * @brief Get the number of tracked instances for a prefab hash.
 * @param prefabManagerPtr Pointer to the PrefabManager.
 * @param prefabHash FNV-1a hash of the prefab path.
 * @return uint32_t Number of tracked instances, or 0 on error.
 */
INTEROP_API uint32_t PrefabManagerInterop_GetInstanceCount(void* prefabManagerPtr, uint32_t prefabHash) {
    if (!prefabManagerPtr)
        return 0;

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    return manager->GetInstanceCount(prefabHash);
}

// ============================================================================
// Prefab Synchronization
// ============================================================================

/**
 * @brief Synchronize an entity's state with the prefab at the given path
 *
 * @param prefabManagerPtr Pointer to the PrefabManager
 * @param entityId Entity id to synchronize
 * @param prefabPath Path to the prefab asset to synchronize from
 * @return true on success, false on failure
 */
INTEROP_API bool PrefabManagerInterop_SynchronizeInstance(void* prefabManagerPtr, uint32_t entityId, const char* prefabPath) {
    if (!prefabManagerPtr || !prefabPath)
        return false;

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    
    ECS::Entity entity;
    entity.Index = entityId;
    
    return manager->SynchronizeInstance(entity, prefabPath);
}

/**
 * @brief Detach an entity from its prefab (make it independent)
 *
 * @param prefabManagerPtr Pointer to the PrefabManager
 * @param entityId Entity id to detach
 * @return true on success, false on failure
 */
INTEROP_API bool PrefabManagerInterop_DetachInstance(void* prefabManagerPtr, uint32_t entityId) {
    if (!prefabManagerPtr)
        return false;

    ECS::PrefabManager* manager = static_cast<ECS::PrefabManager*>(prefabManagerPtr);
    
    ECS::Entity entity;
    entity.Index = entityId;
    
    return manager->DetachInstance(entity);
}
