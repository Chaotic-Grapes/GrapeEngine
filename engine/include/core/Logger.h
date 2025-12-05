#ifndef LOGGER_H
#define LOGGER_H

#include "Export.h"

#ifdef ERROR
#undef ERROR
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
	Logger();
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
	void EnableDebug(bool enable) { m_debugEnabled = enable; }

	// Console callback for editor integration
	using ConsoleCallback = std::function<void(LogLevel, LogSource, const std::string&, const std::string&)>;
	void SetConsoleCallback(ConsoleCallback callback) { m_consoleCallback = callback; }

private:
	// void _writeCrashLog(LogLevel level, const std::string& message);
	void _logTrace(const std::string& message);
	void _logInfo(const std::string& message);
	void _logDebug(const std::string& message);
	void _logWarning(const std::string& message);
	void _logError(const std::string& message);
	void _logCritical(const std::string& message);

	void _logTrace(const std::stringstream& oss);
	void _logInfo(const std::stringstream& oss);
	void _logDebug(const std::stringstream& oss);
	void _logWarning(const std::stringstream& oss);
	void _logError(const std::stringstream& oss);
	void _logCritical(const std::stringstream& oss);

	// Private helper functions for console color
	void _setConsoleColor(LogLevel level);
	void _resetConsoleColor();

	std::string _getCurrentTimestamp(const std::string& format = "%Y-%m-%d %H:%M:%S"); // Add timestamp to log entries

	std::string m_infoFile{ "engine.log" }; // Default log file name
	std::string m_errorFile{ "error.log" }; // Default error log file name

	std::ofstream m_infoStream;
	std::ofstream m_errorStream;

	bool m_LogConsoleEnabled{ true }; // Default to log to console
	bool m_debugEnabled{ true }; // Default to log debug messages

	// Console callback for editor
	ConsoleCallback m_consoleCallback;
};

#endif