/* Start Header *****************************************************************/
/*!
\file    Interop_Layers.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief   Interop functions to expose Layer masks to managed scripts

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
#include <string>
#include <vector>

/**
 * @brief Get mask for layer id from active scene's LayerManager
 *
 * @param layerId The layer identifier to query
 * @return uint32_t The layer mask for the given id or 0xFFFFFFFF on error
 */
INTEROP_API uint32_t EngineInterop_Layers_GetMask(uint16_t layerId) {
    if (!Engine::CORE)
        return 0xFFFFFFFFu;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return 0xFFFFFFFFu;
    return scene->GetLayers().GetLayerMask(layerId);
}

/**
 * @brief Set mask for layer id on active scene's LayerManager
 *
 * @param layerId The layer identifier to modify
 * @param mask The mask value to set for the layer
 */
INTEROP_API void EngineInterop_Layers_SetMask(uint16_t layerId, uint32_t mask) {
    if (!Engine::CORE)
        return;
        
    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return;
    scene->GetLayers().SetLayerMask(layerId, mask);
}

/**
 * @brief Set collision between two layers (symmetrically)
 *
 * @param a First layer id
 * @param b Second layer id
 * @param enabled Non-zero to enable collision, zero to disable
 */
INTEROP_API void EngineInterop_Layers_SetCollisionBetween(uint16_t a, uint16_t b, uint8_t enabled) {
    if (!Engine::CORE)
        return;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return;
    scene->GetLayers().SetCollisionBetween(a, b, enabled != 0);
}

/**
 * @brief Return number of registered layers
 *
 * @return uint16_t Number of layers registered in the active scene
 */
INTEROP_API uint16_t EngineInterop_Layers_Count() {
    if (!Engine::CORE)
        return 0;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return 0;

    auto list = scene->GetLayers().ListLayers();
    return static_cast<uint16_t>(list.size());
}

/**
 * @brief Get layer id at given index in ListLayers order
 *
 * @param index The index into the layer list
 * @return int32_t Layer id at the index, or -1 if invalid
 */
INTEROP_API int32_t EngineInterop_Layers_GetIdAtIndex(uint16_t index) {
    if (!Engine::CORE)
        return -1;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return -1;

    auto list = scene->GetLayers().ListLayers();
    if (index >= list.size())
        return -1;

    return static_cast<int32_t>(list[index].first);
}

/**
 * @brief Write layer name at index into provided buffer
 *
 * @param index The index into the layer list
 * @param outBuf Destination buffer to receive the name
 * @param bufSize Size of the destination buffer in bytes
 * @return int32_t 0 on success, -1 on failure
 */
INTEROP_API int32_t EngineInterop_Layers_GetNameAtIndex(uint16_t index, char* outBuf, int32_t bufSize) {
    if (!Engine::CORE)
        return -1;
    if (!outBuf || bufSize <= 0)
        return -1;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return -1;
    
    auto list = scene->GetLayers().ListLayers();
    if (index >= list.size())
        return -1;
    const std::string& name = list[index].second;

    strncpy_s(outBuf, bufSize, name.c_str(), _TRUNCATE);
    return 0;
}

/**
 * @brief Get layer id by name
 *
 * @param name Null-terminated layer name to look up
 * @return int32_t Layer id if found, or -1 if not found or on error
 */
INTEROP_API int32_t EngineInterop_Layers_IdOf(const char* name) {
    if (!Engine::CORE)
        return -1;
    if (!name)
        return -1;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return -1;

    auto opt = scene->GetLayers().IdOf(std::string(name));
    if (!opt.has_value())
        return -1;

    return static_cast<int32_t>(opt.value());
}

/**
 * @brief Check if a layer has rendering enabled
 *
 * @param layerId The layer identifier to query
 * @return uint8_t Non-zero if rendering is enabled, zero otherwise or on error
 */
INTEROP_API uint8_t EngineInterop_Layers_IsRenderEnabled(uint16_t layerId) {
    if (!Engine::CORE)
        return 0;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return 0;

    const auto& layerData = scene->GetLayers().Get(layerId);
    return layerData.renderEnabled ? 1 : 0;
}

/**
 * @brief Check if a layer has updates enabled
 *
 * @param layerId The layer identifier to query
 * @return uint8_t Non-zero if updates are enabled, zero otherwise or on error
 */
INTEROP_API uint8_t EngineInterop_Layers_IsUpdateEnabled(uint16_t layerId) {
    if (!Engine::CORE)
        return 0;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return 0;

    const auto& layerData = scene->GetLayers().Get(layerId);
    return layerData.updateEnabled ? 1 : 0;
}

/**
 * @brief Check if a layer has physics enabled
 *
 * @param layerId The layer identifier to query
 * @return uint8_t Non-zero if physics is enabled, zero otherwise or on error
 */
INTEROP_API uint8_t EngineInterop_Layers_IsPhysicsEnabled(uint16_t layerId) {
    if (!Engine::CORE)
        return 0;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return 0;

    const auto& layerData = scene->GetLayers().Get(layerId);
    return layerData.physicsEnabled ? 1 : 0;
}

/**
 * @brief Check if a layer is visible in the editor
 *
 * @param layerId The layer identifier to query
 * @return uint8_t Non-zero if visible, zero otherwise or on error
 */
INTEROP_API uint8_t EngineInterop_Layers_IsVisible(uint16_t layerId) {
    if (!Engine::CORE)
        return 0;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return 0;

    const auto& layerData = scene->GetLayers().Get(layerId);
    return layerData.editorVisible ? 1 : 0;
}

/**
 * @brief Check if a layer is locked in the editor
 *
 * @param layerId The layer identifier to query
 * @return uint8_t Non-zero if locked, zero otherwise or on error
 */
INTEROP_API uint8_t EngineInterop_Layers_IsLocked(uint16_t layerId) {
    if (!Engine::CORE)
        return 0;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return 0;

    const auto& layerData = scene->GetLayers().Get(layerId);
    return layerData.editorLocked ? 1 : 0;
}
