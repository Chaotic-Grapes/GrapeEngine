/* Start Header *****************************************************************/
/*!
\file    ScriptAPI_Application.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    21st November 2025
\brief
C# scripting API exports for application-level operations and queries.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "core/Application.h"
#include "core/Logger.h"

// Export macro
#ifndef SCRIPT_API
#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#endif
#endif

// ============================================================================
// Application API - Application Control
// ============================================================================

/// <summary>
/// Request the application to quit
/// </summary>
SCRIPT_API void ScriptAPI_Application_Quit() {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] Application core not initialized");
        return;
    }
    Engine::CORE->Close();
}

// ============================================================================
// Application API - Configuration Queries
// ============================================================================

/// <summary>
/// Get the application name from configuration
/// </summary>
SCRIPT_API const char* ScriptAPI_Application_GetName() {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] Application core not initialized");
        return "";
    }
    // Use project name if project settings are loaded, otherwise use editor config
    if (Engine::CORE->HasProjectSettings()) {
        return Engine::CORE->GetProjectSettings().Title.c_str();
    }
    return Engine::CORE->GetConfig().Title.c_str();
}

/// <summary>
/// Get the fixed time step from configuration (from project settings)
/// </summary>
SCRIPT_API float ScriptAPI_Application_GetFixedTimeStep() {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] Application core not initialized");
        return 0.02f;
    }
    // Use project physics timestep if available
    if (Engine::CORE->HasProjectSettings()) {
        return Engine::CORE->GetProjectSettings().Physics.TimeStep;
    }
    return 0.02f; // Default fallback
}

/// <summary>
/// Check if VSync is enabled in configuration
/// </summary>
SCRIPT_API bool ScriptAPI_Application_IsVSyncEnabled() {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] Application core not initialized");
        return false;
    }
    // Use project settings if available, otherwise editor config
    if (Engine::CORE->HasProjectSettings()) {
        return Engine::CORE->GetProjectSettings().WindowSettings.VSync;
    }
    return Engine::CORE->GetConfig().WindowSettings.VSync;
}
