/* Start Header *****************************************************************/
/*!
\file   Logger.h
\author Samantha Leong Sher Yen (100%)
\par    s.leong@digipen.edu
\brief  
Logger class for logging messages with different severity levels to console and 
files. Supports logging from both engine and script sources, with customizable log 
file names and console output. Includes a console callback for editor integration 
and timestamped log entries.

Usage:
- Use the LOG_* macros to log messages at different severity levels 
(e.g., LOG_INFO("This is an info message")).
- Configure log file names and console output using the Logger class methods.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef LOGGER_H
#define LOGGER_H

#include "Export.h"

#ifdef ERROR
#undef ERROR
#endif

// Windows headers (and some third-party libs) sometimes define common
// short macros like ERROR, WARNING, DEBUG, INFO which conflict with our
// `LogLevel` enum identifiers. Undefine them here to avoid collisions when
// this header is included after platform headers.
#ifdef WARNING
#undef WARNING
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef INFO
#undef INFO
#endif

#define LOG_STREAM(level, msg)						\
    do {											\
        std::ostringstream ossMacro;				\
        ossMacro << msg;                            \
        Logger::Get().Log(level, ossMacro.str(), LogSource::ENGINE);   \
    } while(0)

#define LOG_TRACE(msg)    LOG_STREAM(LogLevel::TRACE, msg)
#define LOG_INFO(msg)     LOG_STREAM(LogLevel::INFO, msg)
#define LOG_DEBUG(msg)    LOG_STREAM(LogLevel::DEBUG, msg)
#define LOG_WARNING(msg)  LOG_STREAM(LogLevel::WARNING, msg)
#define LOG_ERROR(msg)    LOG_STREAM(LogLevel::ERROR, msg)
#define LOG_CRITICAL(msg) LOG_STREAM(LogLevel::CRITICAL, msg)

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <functional>

enum class LogSource {
	ENGINE,
	SCRIPT
};

enum class LogLevel {
	TRACE,
	INFO,
	DEBUG,
	WARNING,
	ERROR,
	CRITICAL
};

// enums for colour codingscoped

//AI states tracks what AI is doing at the current game loop

class GRAPEENGINE_API Logger {
public:
	/**
	 * @brief Construct logger and initialize output streams.
	 */
	Logger();

	/**
	 * @brief Flush and close logger resources.
	 */
	~Logger();

	/**
	 * @brief Get the singleton instance of the Logger
	 * @return Reference to the Logger instance
	 */
	static Logger& Get() {
		static Logger instance;
		return instance;
	}

	/**
	 * @brief Log a message with a specific log level
	 * @param level The severity level of the log
	 * @param message The message to log
	 * @param source The source of the log (ENGINE or SCRIPT)
	 */
	void Log(LogLevel level, const std::string& message, LogSource source = LogSource::ENGINE);

	/**
	 * @brief Log a message from a stringstream with a specific log level
	 * @param level The severity level of the log
	 * @param oss The stringstream containing the message to log
	 * @param source The source of the log (ENGINE or SCRIPT)
	 */
	void Log(LogLevel level, const std::stringstream& oss, LogSource source = LogSource::ENGINE);

	/**
	 * @brief Set the log file name
	 * @param level The log level for which to set the file
	 * @param filename The name of the log file
	 */
	void SetLogFile(LogLevel level, const std::string& filename);

	/**
	 * @brief Enable or disable logging to console
	 * @param enable True to enable console logging, false to disable
	 */
	void SetLogConsole(bool enable);

	/**
	 * @brief Enable or disable DEBUG-level emission.
	 * @param enable True to include debug logs.
	 */
	void EnableDebug(bool enable) { m_debugEnabled = enable; }

	// Console callback for editor integration
	using ConsoleCallback = std::function<void(LogLevel, LogSource, const std::string&, const std::string&)>;

	/**
	 * @brief Register callback to mirror logs to editor console UI.
	 * @param callback Callback receiving level, source, message, and timestamp.
	 */
	void SetConsoleCallback(ConsoleCallback callback) { m_consoleCallback = callback; }

private:

	/**
	 * @brief Write TRACE message to active sinks.
	 * @param message Text payload to write.
	 */
	void _logTrace(const std::string& message);

	/**
	 * @brief Write INFO message to active sinks.
	 * @param message Text payload to write.
	 */
	void _logInfo(const std::string& message);

	/**
	 * @brief Write DEBUG message to active sinks.
	 * @param message Text payload to write.
	 */
	void _logDebug(const std::string& message);

	/**
	 * @brief Write WARNING message to active sinks.
	 * @param message Text payload to write.
	 */
	void _logWarning(const std::string& message);

	/**
	 * @brief Write ERROR message to active sinks.
	 * @param message Text payload to write.
	 */
	void _logError(const std::string& message);

	/**
	 * @brief Write CRITICAL message to active sinks.
	 * @param message Text payload to write.
	 */
	void _logCritical(const std::string& message);

	/**
	 * @brief Write TRACE stream payload to active sinks.
	 * @param oss Stream containing formatted message text.
	 */
	void _logTrace(const std::stringstream& oss);

	/**
	 * @brief Write INFO stream payload to active sinks.
	 * @param oss Stream containing formatted message text.
	 */
	void _logInfo(const std::stringstream& oss);

	/**
	 * @brief Write DEBUG stream payload to active sinks.
	 * @param oss Stream containing formatted message text.
	 */
	void _logDebug(const std::stringstream& oss);

	/**
	 * @brief Write WARNING stream payload to active sinks.
	 * @param oss Stream containing formatted message text.
	 */
	void _logWarning(const std::stringstream& oss);

	/**
	 * @brief Write ERROR stream payload to active sinks.
	 * @param oss Stream containing formatted message text.
	 */
	void _logError(const std::stringstream& oss);

	/**
	 * @brief Write CRITICAL stream payload to active sinks.
	 * @param oss Stream containing formatted message text.
	 */
	void _logCritical(const std::stringstream& oss);

	// Private helper functions for console color
	/**
	 * @brief Set console color matching a log level.
	 * @param level Log level controlling color mapping.
	 */
	void _setConsoleColor(LogLevel level);
	
	/**
	 * @brief Restore default console color after logging.
	 */
	void _resetConsoleColor();

	/**
	 * @brief Build timestamp string for log formatting.
	 * @param format strftime-compatible format pattern.
	 * @return Formatted timestamp string.
	 */
	std::string _getCurrentTimestamp(const std::string& format = "%Y-%m-%d %H:%M:%S"); // Add timestamp to log entries

	std::string m_infoFile;  // Set via ProjectPaths::Initialize
	std::string m_errorFile; // Set via ProjectPaths::Initialize

	std::ofstream m_infoStream;
	std::ofstream m_errorStream;

	bool m_LogConsoleEnabled{ true }; // Default to log to console
	bool m_debugEnabled{ true }; // Default to log debug messages

	// Console callback for editor
	ConsoleCallback m_consoleCallback;
};

#endif
