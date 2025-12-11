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

INTEROP_API void* SceneManagerInterop_GetInstance() {
    if (!Engine::CORE)
        return nullptr;
    
    return static_cast<void*>(&Engine::CORE->GetSceneManager());
}

// ============================================================================
// Scene Management - Add/Remove/Query
// ============================================================================

INTEROP_API uint64_t SceneManagerInterop_AddScene(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return static_cast<uint64_t>(-1);

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    Scenes::Scene* newScene = new Scenes::Scene();
    return manager->AddScene(newScene);
}

INTEROP_API bool SceneManagerInterop_RemoveScene(void* sceneManagerPtr, uint64_t sceneIndex) {
    if (!sceneManagerPtr)
        return false;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return manager->RemoveScene(static_cast<size_t>(sceneIndex));
}

INTEROP_API uint64_t SceneManagerInterop_GetSceneCount(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return 0;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return static_cast<uint64_t>(manager->GetSceneCount());
}

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

INTEROP_API void SceneManagerInterop_SetActive(void* sceneManagerPtr, uint64_t sceneIndex) {
    if (!sceneManagerPtr)
        return;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    manager->SetActive(static_cast<size_t>(sceneIndex));
}

INTEROP_API void SceneManagerInterop_SetActiveImmediate(void* sceneManagerPtr, uint64_t sceneIndex) {
    if (!sceneManagerPtr)
        return;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    manager->SetActiveImmediate(static_cast<size_t>(sceneIndex));
}

INTEROP_API void* SceneManagerInterop_GetActive(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return nullptr;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    Scenes::Scene* scene = manager->GetActive();
    return static_cast<void*>(scene);
}

INTEROP_API uint64_t SceneManagerInterop_GetActiveIndex(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return static_cast<uint64_t>(-1);

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return static_cast<uint64_t>(manager->GetActiveIndex());
}

INTEROP_API uint64_t SceneManagerInterop_GetPendingIndex(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return static_cast<uint64_t>(-1);

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return static_cast<uint64_t>(manager->GetPendingIndex());
}

// ============================================================================
// Scene Update
// ============================================================================

INTEROP_API void SceneManagerInterop_Update(void* sceneManagerPtr) {
    if (!sceneManagerPtr)
        return;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    manager->Update();
}

// ============================================================================
// Scene Serialization
// ============================================================================

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

INTEROP_API bool SceneManagerInterop_LoadScene(void* sceneManagerPtr, uint64_t sceneIndex, const char* filename) {
    if (!sceneManagerPtr || !filename)
        return false;

    Scenes::SceneManager* manager = static_cast<Scenes::SceneManager*>(sceneManagerPtr);
    return manager->LoadScene(static_cast<size_t>(sceneIndex), filename);
}

// ============================================================================
// Scene Properties
// ============================================================================

INTEROP_API void SceneInterop_GetName(void* scenePtr, char* buffer, int bufferSize) {
    if (!scenePtr || !buffer || bufferSize <= 0)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    const std::string& name = scene->GetName();
    
    int copySize = std::min(static_cast<int>(name.size()), bufferSize - 1);
    std::memcpy(buffer, name.c_str(), copySize);
    buffer[copySize] = '\0';
}

INTEROP_API void SceneInterop_SetName(void* scenePtr, const char* name) {
    if (!scenePtr || !name)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    scene->SetName(name);
}

INTEROP_API void SceneInterop_GetPath(void* scenePtr, char* buffer, int bufferSize) {
    if (!scenePtr || !buffer || bufferSize <= 0)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    const std::string& path = scene->GetPath();
    
    int copySize = std::min(static_cast<int>(path.size()), bufferSize - 1);
    std::memcpy(buffer, path.c_str(), copySize);
    buffer[copySize] = '\0';
}

INTEROP_API void SceneInterop_SetPath(void* scenePtr, const char* path) {
    if (!scenePtr || !path)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    scene->SetPath(path);
}

// ============================================================================
// Scene World Access
// ============================================================================

INTEROP_API void* SceneInterop_GetWorld(void* scenePtr) {
    if (!scenePtr)
        return nullptr;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    return static_cast<void*>(&scene->GetWorld());
}

// ============================================================================
// Scene Entity Operations
// ============================================================================

INTEROP_API uint64_t SceneInterop_CreateEntity(void* scenePtr) {
    if (!scenePtr)
        return 0;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = scene->CreateEntity();
    return ECS::EntityUtils::Pack(entity);
}

INTEROP_API uint64_t SceneInterop_CreateEntityWithParent(void* scenePtr, uint64_t parentId) {
    if (!scenePtr)
        return 0;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity parent = ECS::EntityUtils::Unpack(parentId);
    ECS::Entity entity = scene->CreateEntity(parent);
    return ECS::EntityUtils::Pack(entity);
}

INTEROP_API void SceneInterop_DestroyEntity(void* scenePtr, uint64_t entityId) {
    if (!scenePtr)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    scene->DestroyEntity(entity);
}

INTEROP_API uint64_t SceneInterop_CreateOnLayer(void* scenePtr, uint16_t layerId) {
    if (!scenePtr)
        return 0;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = scene->CreateOnLayer(layerId);
    return ECS::EntityUtils::Pack(entity);
}

INTEROP_API void SceneInterop_SetLayer(void* scenePtr, uint64_t entityId, uint16_t layerId) {
    if (!scenePtr)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    scene->SetLayer(entity, layerId);
}

INTEROP_API void SceneInterop_RemoveFromLayer(void* scenePtr, uint64_t entityId) {
    if (!scenePtr)
        return;

    Scenes::Scene* scene = static_cast<Scenes::Scene*>(scenePtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    scene->RemoveFromLayer(entity);
}
