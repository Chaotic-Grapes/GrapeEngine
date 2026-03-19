/* Start Header *****************************************************************/
/*!
\file    Interop_System.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
C API exports for system-level scripting controls.

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
#include "core/Logger.h"

/**
 * @brief Enable or disable a system by name.
 * @param systemName The target system name.
 * @param enabled True to enable, false to disable.
 * @return True if the system state was updated, false otherwise.
 */
INTEROP_API bool EngineInterop_System_SetEnabled(const char* systemName, bool enabled) {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] Application core not initialized");
        return false;
    }
    if (!systemName || systemName[0] == '\0') {
        LOG_WARNING("[ScriptAPI] SetEnabled called with empty system name");
        return false;
    }

    return Engine::CORE->GetSystemManager().SetSystemEnabled(systemName, enabled);
}

/**
 * @brief Check whether a system is enabled by name.
 * @param systemName The target system name.
 * @return True if the system is enabled, false otherwise.
 */
INTEROP_API bool EngineInterop_System_IsEnabled(const char* systemName) {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] Application core not initialized");
        return false;
    }
    if (!systemName || systemName[0] == '\0') {
        LOG_WARNING("[ScriptAPI] IsEnabled called with empty system name");
        return false;
    }

    return Engine::CORE->GetSystemManager().IsSystemEnabled(systemName);
}

