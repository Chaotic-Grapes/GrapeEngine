/* Start Header *****************************************************************/
/*!
\file    Interop_Scene.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of Scene/SceneManager interop functions for C# scripting.

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
#include "helpers/EntityUtils.h"
#include <string>
#include <cstring>

// ============================================================================
// SceneManager Lifecycle
// ============================================================================

/**
 * @brief Get the SceneManager instance from the Engine core
 * @return void* Pointer to the SceneManager, or nullptr on error
 */
INTEROP_API void* SceneManagerInterop_GetInstance() {
    if (!Engine::CORE)
        return nullptr;
    
    return static_cast<void*>(&Engine::CORE->GetSceneManager());
}

// ============================================================================
// Scene Management - Add/Remove/Query
// ============================================================================

/**
 * @brief Add a new empty scene to the SceneManager
 * @param sceneManagerPtr Pointer to the SceneManager
 * @return uint64_t Index of the added scene, or -1 on error
 */
INTEROP_API uint64_t SceneManagerInterop_AddScene(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return static_cast<uint64_t>(-1);

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    Scenes::Scene* newScene = new Scenes::Scene();
    return manager->AddScene(newScene);
}

/**
 * @brief Remove a scene by index from the SceneManager
 * @param sceneManagerPtr Pointer to the SceneManager
 * @param sceneIndex Index of the scene to remove
 * @return true if the scene was removed, false otherwise
 */
INTEROP_API bool SceneManagerInterop_RemoveScene(void* sceneManagerPtr, uint64_t sceneIndex) {
    if (!sceneManagerPtr)
        return false;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return manager->RemoveScene(static_cast<size_t>(sceneIndex));
}

/**
 * @brief Get the total number of scenes managed by the SceneManager
 * @param sceneManagerPtr Pointer to the SceneManager
 * @return uint64_t Number of scenes, or 0 on error
 */
INTEROP_API uint64_t SceneManagerInterop_GetSceneCount(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return 0;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return static_cast<uint64_t>(manager->GetSceneCount());
}

/**
 * @brief Get a scene by index from the SceneManager
 * @param sceneManagerPtr Pointer to the SceneManager
 * @param sceneIndex Index of the scene to retrieve
 * @return void* Pointer to the Scene, or nullptr on error
 */
INTEROP_API void* SceneManagerInterop_GetScene(void* sceneManagerPtr, uint64_t sceneIndex) {
    if (!sceneManagerPtr)
        return nullptr;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    const Scenes::Scene* scene = manager->GetScene(static_cast<size_t>(sceneIndex));
    return const_cast<void*>(static_cast<const void*>(scene));
}

// ============================================================================
// Scene Activation
// ============================================================================

/**
 * @brief Request that the SceneManager set the active scene (deferred)
 *
 * @param sceneManagerPtr Pointer to the SceneManager
 * @param sceneIndex Index of the scene to make active
 */
INTEROP_API void SceneManagerInterop_SetActive(void* sceneManagerPtr, uint64_t sceneIndex) {
    if (!sceneManagerPtr)
        return;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    manager->SetActive(static_cast<size_t>(sceneIndex));
}

/**
 * @brief Immediately set the active scene (synchronous)
 *
 * @param sceneManagerPtr Pointer to the SceneManager
 * @param sceneIndex Index of the scene to make active immediately
 */
INTEROP_API void SceneManagerInterop_SetActiveImmediate(void* sceneManagerPtr, uint64_t sceneIndex) {
    if (!sceneManagerPtr)
        return;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    manager->SetActiveImmediate(static_cast<size_t>(sceneIndex));
}

/**
 * @brief Get the currently active scene
 *
 * @param sceneManagerPtr Pointer to the SceneManager
 * @return void* Pointer to the active Scene, or nullptr on error
 */
INTEROP_API void* SceneManagerInterop_GetActive(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return nullptr;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    Scenes::Scene* scene = manager->GetActive();
    return static_cast<void*>(scene);
}

/**
 * @brief Get the index of the currently active scene
 *
 * @param sceneManagerPtr Pointer to the SceneManager
 * @return uint64_t Index of active scene, or (uint64_t)-1 on error
 */
INTEROP_API uint64_t SceneManagerInterop_GetActiveIndex(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return static_cast<uint64_t>(-1);

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return static_cast<uint64_t>(manager->GetActiveIndex());
}

/**
 * @brief Get the index of the pending scene to be activated
 *
 * @param sceneManagerPtr Pointer to the SceneManager
 * @return uint64_t Index of pending scene, or (uint64_t)-1 if none or on error
 */
INTEROP_API uint64_t SceneManagerInterop_GetPendingIndex(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return static_cast<uint64_t>(-1);

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return static_cast<uint64_t>(manager->GetPendingIndex());
}

// ============================================================================
// Scene Update
// ============================================================================

/**
 * @brief Update the SceneManager (typically advances scene logic)
 *
 * @param sceneManagerPtr Pointer to the SceneManager
 */
INTEROP_API void SceneManagerInterop_Update(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    manager->Update();
}

// ============================================================================
// Scene Serialization
// ============================================================================

/**
 * @brief Save a scene to disk
 *
 * @param sceneManagerPtr Pointer to the SceneManager
 * @param sceneIndex Index of the scene to save
 * @param filename Output filename/path
 * @param sceneName Optional scene name (NULL uses default "Scene")
 * @param version Optional version string (NULL uses default "1.0")
 * @return true on success, false on failure
 */
INTEROP_API bool SceneManagerInterop_SaveScene(void* sceneManagerPtr, uint64_t sceneIndex, const char* filename, const char* sceneName, const char* version) {
    if (!sceneManagerPtr || !filename)
        return false;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    
    std::string name = sceneName ? sceneName : "Scene";
    std::string ver = version ? version : "1.0";
    
    return manager->SaveScene(
        static_cast<size_t>(sceneIndex),
        filename,
        name,
        ver
    );
}

/**
 * @brief Load a scene from disk into the specified index
 *
 * @param sceneManagerPtr Pointer to the SceneManager
 * @param sceneIndex Index to load the scene into
 * @param filename Path to the scene file
 * @return true on success, false on failure
 */
INTEROP_API bool SceneManagerInterop_LoadScene(void* sceneManagerPtr, uint64_t sceneIndex, const char* filename) {
    if (!sceneManagerPtr || !filename)
        return false;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return manager->LoadScene(static_cast<size_t>(sceneIndex), filename);
}

// ============================================================================
// Scene Properties
// ============================================================================

/**
 * @brief Copy the scene's name into a caller buffer
 *
 * @param scenePtr Pointer to the Scene
 * @param buffer Destination buffer
 * @param bufferSize Size of the destination buffer in bytes
 */
INTEROP_API void SceneInterop_GetName(void* scenePtr, char* buffer, int bufferSize) {
    if (!scenePtr || !buffer || bufferSize <= 0)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    const std::string& name = scene->GetName();
    
    int copySize = std::min(static_cast<int>(name.size()), bufferSize - 1);
    std::memcpy(buffer, name.c_str(), copySize);
    buffer[copySize] = '\0';
}

/**
 * @brief Set the scene's name
 *
 * @param scenePtr Pointer to the Scene
 * @param name Null-terminated name string
 */
INTEROP_API void SceneInterop_SetName(void* scenePtr, const char* name) {
    if (!scenePtr || !name)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    scene->SetName(name);
}

/**
 * @brief Copy the scene's path into a caller buffer
 *
 * @param scenePtr Pointer to the Scene
 * @param buffer Destination buffer
 * @param bufferSize Size of the destination buffer in bytes
 */
INTEROP_API void SceneInterop_GetPath(void* scenePtr, char* buffer, int bufferSize) {
    if (!scenePtr || !buffer || bufferSize <= 0)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    const std::string& path = scene->GetPath();
    
    int copySize = std::min(static_cast<int>(path.size()), bufferSize - 1);
    std::memcpy(buffer, path.c_str(), copySize);
    buffer[copySize] = '\0';
}

/**
 * @brief Set the scene's file path
 *
 * @param scenePtr Pointer to the Scene
 * @param path Null-terminated path string
 */
INTEROP_API void SceneInterop_SetPath(void* scenePtr, const char* path) {
    if (!scenePtr || !path)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    scene->SetPath(path);
}

// ============================================================================
// Scene World Access
// ============================================================================

/**
 * @brief Get the ECS World associated with a Scene
 *
 * @param scenePtr Pointer to the Scene
 * @return void* Pointer to the World, or nullptr on error
 */
INTEROP_API void* SceneInterop_GetWorld(void* scenePtr) {
    if (!scenePtr)
        return nullptr;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    return static_cast<void*>(&scene->GetWorld());
}

// ============================================================================
// Scene Entity Operations
// ============================================================================

/**
 * @brief Create a new entity in the scene and return its packed id
 *
 * @param scenePtr Pointer to the Scene
 * @return uint64_t Packed entity id, or 0 on failure
 */
INTEROP_API uint64_t SceneInterop_CreateEntity(void* scenePtr) {
    if (!scenePtr)
        return 0;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = scene->CreateEntity();
    return ECS::EntityUtils::Pack(entity);
}

/**
 * @brief Create a new entity with the specified parent
 *
 * @param scenePtr Pointer to the Scene
 * @param parentId Packed entity id of the parent
 * @return uint64_t Packed entity id of the created entity, or 0 on failure
 */
INTEROP_API uint64_t SceneInterop_CreateEntityWithParent(void* scenePtr, uint64_t parentId) {
    if (!scenePtr)
        return 0;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity parent = ECS::EntityUtils::Unpack(parentId);
    ECS::Entity entity = scene->CreateEntity(parent);
    return ECS::EntityUtils::Pack(entity);
}

/**
 * @brief Destroy an entity in the scene
 *
 * @param scenePtr Pointer to the Scene
 * @param entityId Packed entity id to destroy
 */
INTEROP_API void SceneInterop_DestroyEntity(void* scenePtr, uint64_t entityId) {
    if (!scenePtr)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    scene->DestroyEntity(entity);
}

/**
 * @brief Create a new entity on the specified layer
 *
 * @param scenePtr Pointer to the Scene
 * @param layerId Layer identifier
 * @return uint64_t Packed entity id of the created entity, or 0 on failure
 */
INTEROP_API uint64_t SceneInterop_CreateOnLayer(void* scenePtr, uint16_t layerId) {
    if (!scenePtr)
        return 0;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = scene->CreateOnLayer(layerId);
    return ECS::EntityUtils::Pack(entity);
}

/**
 * @brief Set the layer of an entity
 *
 * @param scenePtr Pointer to the Scene
 * @param entityId Packed entity id
 * @param layerId Layer identifier to assign
 */
INTEROP_API void SceneInterop_SetLayer(void* scenePtr, uint64_t entityId, uint16_t layerId) {
    if (!scenePtr)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    scene->SetLayer(entity, layerId);
}

/**
 * @brief Remove an entity from its current layer
 *
 * @param scenePtr Pointer to the Scene
 * @param entityId Packed entity id
 */
INTEROP_API void SceneInterop_RemoveFromLayer(void* scenePtr, uint64_t entityId) {
    if (!scenePtr)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    scene->RemoveFromLayer(entity);
}

// ============================================================================
// Prefab Management
// ============================================================================

/**
 * @brief Get the PrefabManager associated with the scene manager
 *
 * @param scenePtr Pointer to the Scene
 * @return void* Pointer to the PrefabManager, or nullptr on error
 */
INTEROP_API void* SceneInterop_GetPrefabManager(void* scenePtr) {
    if (!scenePtr)
        return nullptr;

    if (!Engine::CORE)
        return nullptr;

    Scenes::SceneManager& sceneMgr = Engine::CORE->GetSceneManager();
    return static_cast<void*>(sceneMgr.GetPrefabManager());
}
