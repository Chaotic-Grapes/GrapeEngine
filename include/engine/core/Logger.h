/* Start Header *****************************************************************/
/*!
\file   Logger.h
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   7th November 2025
\brief
Declares the Logger interface and log level enums for structured console and file
logging with timestamped entries.
*/
/* End Header *******************************************************************/

#ifndef LOGGER_H
#define LOGGER_H

#ifdef ERROR
#undef ERROR
#endif

#define LOG_STREAM(level, msg)                 \
    do {                                       \
        std::ostringstream oss;                \
        oss << msg;                            \
        Logger::Get().Log(level, oss.str());   \
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

class Logger {
public:
    Logger();
	~Logger();

	// Get the singleton instance of the Logger.
	static Logger& Get() {
		static Logger instance;
		return instance;
	}

	// Log a message with a specific log level.
	void Log(LogLevel level, const std::string& message);

	// Log a message from a stringstream with a specific log level.
	void Log(LogLevel level, const std::stringstream& oss);

	// Set the log file name for a given log level.
	void SetLogFile(LogLevel level, const std::string& filename);

	// Enable or disable logging to the console.
	void SetLogConsole(bool enable);

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
};

#endif