/* Start Header *****************************************************************/
/*!
\file    Interop_Debug.cpp
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

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "core/Logger.h"

// ============================================================================
// Script Diagnostics API - Compiler & Runtime Logging
// ============================================================================

/**
 * @brief Report a single compiler diagnostic with structured data.
 * Called once per diagnostic for O(1) processing.
 * 
 * @param id Diagnostic ID (e.g., "CS0103")
 * @param severity 0=Hidden, 1=Warning, 2=Error
 * @param file Source file path
 * @param line Line number (1-based)
 * @param column Column number (1-based)
 * @param message Diagnostic message
 */
INTEROP_API void EngineInterop_ScriptDiagnostic(
    const char* id,
    uint8_t severity,
    const char* file,
    int line,
    int column,
    const char* message) 
{
    if (!message || !file) return;
    
    // Format as: file(line,column): severity id: message
    // Example: Player.cs(42,17): error CS0103: The name 'speedz' does not exist
    std::string formatted = std::string(file) + "(" + std::to_string(line) + "," + std::to_string(column) + "): " + 
                           (id ? std::string(id) + ": " : "") + message;
    
    LogLevel level = (severity == 2) ? LogLevel::ERROR : LogLevel::WARNING;
    Logger::Get().Log(level, formatted, LogSource::SCRIPT);
}

/**
 * @brief Log a message from script code at runtime.
 * 
 * @param message The message to log
 * @param level Log level (0=Trace, 1=Info, 2=Warning, 3=Error, 4=Fatal)
 */
INTEROP_API void EngineInterop_ScriptLog(const char* message, uint8_t level) {
    if (!message) return;
    
    LogLevel logLevel;
    switch (level) {
        case 0: logLevel = LogLevel::DEBUG; break;      // Debug
        case 1: logLevel = LogLevel::INFO; break;       // Info
        case 2: logLevel = LogLevel::WARNING; break;    // Warning
        case 3: logLevel = LogLevel::ERROR; break;      // Error
        case 4: logLevel = LogLevel::CRITICAL; break;   // Fatal
        default: logLevel = LogLevel::INFO; break;
    }
    
    Logger::Get().Log(logLevel, message, LogSource::SCRIPT);
}

/**
 * @brief Log a message from script code with source location information.
 * 
 * @param message The message to log
 * @param level Log level (0=Debug, 1=Info, 2=Warning, 3=Error, 4=Fatal)
 * @param file Source file path
 * @param line Line number (1-based)
 */
INTEROP_API void EngineInterop_ScriptLogWithLocation(
    const char* message,
    uint8_t level,
    const char* file,
    int line)
{
    if (!message || !file) return;
    
    LogLevel logLevel;
    switch (level) {
        case 0: logLevel = LogLevel::DEBUG; break;      // Debug
        case 1: logLevel = LogLevel::INFO; break;       // Info
        case 2: logLevel = LogLevel::WARNING; break;    // Warning
        case 3: logLevel = LogLevel::ERROR; break;      // Error
        case 4: logLevel = LogLevel::CRITICAL; break;   // Fatal
        default: logLevel = LogLevel::INFO; break;
    }
    
    std::string formatted = std::string(file) + "(" + std::to_string(line) + "): " + message;
    Logger::Get().Log(logLevel, formatted, LogSource::SCRIPT);
}
