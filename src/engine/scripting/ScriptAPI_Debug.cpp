/* Start Header *****************************************************************/
/*!
\file    ScriptAPI_Debug.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\date    20th November 2025
\brief
C# scripting API exports for debug logging.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "core/Logger.h"

// Export macro
#ifndef SCRIPT_API
#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#endif
#endif

// ============================================================================
// Debug API - Logging
// ============================================================================

/**
 * @brief Log an info message to the console
 * @param message The message to log
 */
SCRIPT_API void ScriptAPI_Debug_LogInfo(const char* message) {
    if (!message) return;
    Logger::Get().Log(LogLevel::INFO, message, LogSource::SCRIPT);
}

/** 
 * @brief Log a debug message to the console
 * @param message The message to log
 */
SCRIPT_API void ScriptAPI_Debug_LogDebug(const char* message) {
    if (!message) return;
    Logger::Get().Log(LogLevel::DEBUG, message, LogSource::SCRIPT);
}

/**
 * @brief Log a warning message to the console
 * @param message The message to log
 */
SCRIPT_API void ScriptAPI_Debug_LogWarning(const char* message) {
    if (!message) return;
    Logger::Get().Log(LogLevel::WARNING, message, LogSource::SCRIPT);
}

/**
 * @brief Log an error message to the console
 * @param message The message to log
 */
SCRIPT_API void ScriptAPI_Debug_LogError(const char* message) {
    if (!message) return;
    Logger::Get().Log(LogLevel::ERROR, message, LogSource::SCRIPT);
}
