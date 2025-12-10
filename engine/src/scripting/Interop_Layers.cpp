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

// Get mask for layer id from active scene's LayerManager
INTEROP_API uint32_t EngineInterop_Layers_GetMask(uint16_t layerId) {
    if (!Engine::CORE)
        return 0xFFFFFFFFu;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return 0xFFFFFFFFu;
    return scene->GetLayers().GetLayerMask(layerId);
}

// Set mask for layer id on active scene's LayerManager
INTEROP_API void EngineInterop_Layers_SetMask(uint16_t layerId, uint32_t mask) {
    if (!Engine::CORE)
        return;
        
    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return;
    scene->GetLayers().SetLayerMask(layerId, mask);
}

// Set collision between two layers (symmetrically)
INTEROP_API void EngineInterop_Layers_SetCollisionBetween(uint16_t a, uint16_t b, uint8_t enabled) {
    if (!Engine::CORE)
        return;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return;
    scene->GetLayers().SetCollisionBetween(a, b, enabled != 0);
}

// Return number of registered layers
INTEROP_API uint16_t EngineInterop_Layers_Count() {
    if (!Engine::CORE)
        return 0;

    Scenes::Scene* scene = Engine::CORE->GetSceneManager().GetActive();
    if (!scene)
        return 0;

    auto list = scene->GetLayers().ListLayers();
    return static_cast<uint16_t>(list.size());
}

// Get layer id at given index in ListLayers order. Returns -1 if invalid.
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

// Write layer name at index into provided buffer. Returns 0 on success, -1 on failure.
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

// Get layer id by name. Returns -1 if not found.
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
