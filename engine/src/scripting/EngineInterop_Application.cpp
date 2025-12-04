/* Start Header *****************************************************************/
/*!
\file    EngineInterop_Application.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    21st November 2025
\brief
C API exports for managed C# scripting systems for application-level operations and queries.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "core/Application.h"
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

// ============================================================================
// Application API - Application Control
// ============================================================================

/// <summary>
/// Request the application to quit
/// </summary>
ENGINE_INTEROP_API void EngineInterop_Application_Quit() {
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
ENGINE_INTEROP_API const char* EngineInterop_Application_GetName() {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] Application core not initialized");
        return "";
    }
    // Use project name if project settings are loaded, otherwise use editor config
    if (Engine::CORE->HasProjectSettings()) {
        return Engine::CORE->GetProjectSettings().Title.c_str();
    }
    return "";
}

/// <summary>
/// Get the fixed time step from configuration (from project settings)
/// </summary>
ENGINE_INTEROP_API float EngineInterop_Application_GetFixedTimeStep() {
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
ENGINE_INTEROP_API bool EngineInterop_Application_IsVSyncEnabled() {
    if (!Engine::CORE) {
        LOG_ERROR("[ScriptAPI] Application core not initialized");
        return false;
    }
    // Use project settings for VSync
    if (Engine::CORE->HasProjectSettings()) {
        return Engine::CORE->GetProjectSettings().WindowSettings.VSync;
    }
    // Fallback to checking window manager directly
    if (auto* window = WindowManager::GetMainWindow()) {
        return window->IsVSync();
    }
    return true; // Default to VSync enabled
}
