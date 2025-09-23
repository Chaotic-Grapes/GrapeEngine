#ifndef LOGGER_H
#define LOGGER_H

#ifdef ERROR
#undef ERROR
#endif

#include <iostream>
#include <string>
#include <fstream>
#include <GLFW/glfw3.h>

enum LogLevel {
	INFO,
	DEBUG,
	WARNING,
	ERROR,
	CRITICAL
};

class Logger {
public:
	Logger();

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
	 */
	void Log(LogLevel level, const std::string& message);

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

private:
	// void _writeCrashLog(LogLevel level, const std::string& message);
	void _logInfo(const std::string& message);
	void _logDebug(const std::string& message);
	void _logWarning(const std::string& message);
	void _logError(const std::string& message);
	void _logCritical(const std::string& message);

	// Private helper functions for console color
	void _setConsoleColor(LogLevel level);
	void _resetConsoleColor();

	std::string m_infoFile{ "engine.log" }; // Default log file name
	std::string m_errorFile{ "error.log" }; // Default error log file name

	std::ofstream m_infoStream;
	std::ofstream m_errorStream;

	bool m_LogConsoleEnabled{ true }; // Default to log to console
	bool m_debugEnabled{ true }; // Default to log debug messages
};

#endif