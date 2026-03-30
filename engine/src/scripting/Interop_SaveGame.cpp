/* Start Header *****************************************************************/
/*!
\file    Interop_SaveGame.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
C API exports for managed scripting save-game operations (save/load slots,
profile selection, and metadata enumeration).
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "core/Application.h"
#include "core/Logger.h"
#include <algorithm>
#include <cstring>
#include <string>

namespace {
    /**
     * @brief Function purpose: copy UTF-8 string into a caller-provided output buffer.
     * Input parameters: value Source UTF-8 text, buffer Destination pointer, bufferSize Destination capacity.
     * Output/return values: required byte count excluding null terminator.
     */
    int WriteUtf8ToBuffer(const std::string& value, char* buffer, int bufferSize) {
        const int required = static_cast<int>(value.size());
        if (!buffer || bufferSize <= 0) {
            return required;
        }

        const int copyLen = std::min(required, bufferSize - 1);
        if (copyLen > 0) {
            std::memcpy(buffer, value.data(), static_cast<size_t>(copyLen));
        }
        buffer[copyLen] = '\0';
        return required;
    }
}

INTEROP_API bool EngineInterop_SaveGame_SaveSlot(uint32_t slot, const char* displayName, const char* userDataJson) {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] SaveGame_SaveSlot failed: Application core not initialized");
        return false;
    }

    const std::string name = displayName ? std::string(displayName) : std::string();
    const std::string data = userDataJson ? std::string(userDataJson) : std::string();
    return Engine::CORE->GetSaveGameManager().SaveSlot(Engine::CORE->GetSceneManager(), slot, name, data);
}

INTEROP_API bool EngineInterop_SaveGame_LoadSlot(uint32_t slot) {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] SaveGame_LoadSlot failed: Application core not initialized");
        return false;
    }

    return Engine::CORE->GetSaveGameManager().LoadSlot(Engine::CORE->GetSceneManager(), slot);
}

INTEROP_API bool EngineInterop_SaveGame_DeleteSlot(uint32_t slot) {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] SaveGame_DeleteSlot failed: Application core not initialized");
        return false;
    }

    return Engine::CORE->GetSaveGameManager().DeleteSlot(slot);
}

INTEROP_API bool EngineInterop_SaveGame_HasSlot(uint32_t slot) {
    if (!Engine::CORE) {
        return false;
    }

    return Engine::CORE->GetSaveGameManager().HasSlot(slot);
}

INTEROP_API bool EngineInterop_SaveGame_SetActiveProfile(const char* profileId) {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] SaveGame_SetActiveProfile failed: Application core not initialized");
        return false;
    }

    const std::string requested = profileId ? std::string(profileId) : std::string("default");
    return Engine::CORE->GetSaveGameManager().SetActiveProfile(requested);
}

INTEROP_API int EngineInterop_SaveGame_GetActiveProfile(char* buffer, int bufferSize) {
    if (!Engine::CORE) {
        return WriteUtf8ToBuffer("", buffer, bufferSize);
    }

    return WriteUtf8ToBuffer(Engine::CORE->GetSaveGameManager().GetActiveProfile(), buffer, bufferSize);
}

INTEROP_API int EngineInterop_SaveGame_GetSlotsJson(char* buffer, int bufferSize) {
    if (!Engine::CORE) {
        return WriteUtf8ToBuffer("[]", buffer, bufferSize);
    }

    return WriteUtf8ToBuffer(Engine::CORE->GetSaveGameManager().ExportSlotsJson(), buffer, bufferSize);
}

INTEROP_API int EngineInterop_SaveGame_GetSlotJson(uint32_t slot, char* buffer, int bufferSize) {
    if (!Engine::CORE) {
        return WriteUtf8ToBuffer("{}", buffer, bufferSize);
    }

    return WriteUtf8ToBuffer(Engine::CORE->GetSaveGameManager().ExportSlotJson(slot), buffer, bufferSize);
}
