/* Start Header *****************************************************************/
/*!
\file    EngineInterop_Debug.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    20th November 2025
\brief
C API exports for managed C# scripting systems for debug logging.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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
// Debug API - Logging
// ============================================================================

/**
 * @brief Log an info message to the console
 * @param message The message to log
 */
ENGINE_INTEROP_API void EngineInterop_Debug_LogInfo(const char* message) {
    if (!message) return;
    Logger::Get().Log(LogLevel::INFO, message, LogSource::SCRIPT);
}

/** 
 * @brief Log a debug message to the console
 * @param message The message to log
 */
ENGINE_INTEROP_API void EngineInterop_Debug_LogDebug(const char* message) {
    if (!message) return;
    Logger::Get().Log(LogLevel::DEBUG, message, LogSource::SCRIPT);
}

/**
 * @brief Log a warning message to the console
 * @param message The message to log
 */
ENGINE_INTEROP_API void EngineInterop_Debug_LogWarning(const char* message) {
    if (!message) return;
    Logger::Get().Log(LogLevel::WARNING, message, LogSource::SCRIPT);
}

/**
 * @brief Log an error message to the console
 * @param message The message to log
 */
ENGINE_INTEROP_API void EngineInterop_Debug_LogError(const char* message) {
    if (!message) return;
    Logger::Get().Log(LogLevel::ERROR, message, LogSource::SCRIPT);
}
