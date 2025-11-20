/**
 * @Name: Samantha Leong, 2403088
 * @email: s.leong@digipen.edu
 * @file Logger.cpp
 * @brief Implements the logging system for the engine.
 *
 * This file defines the Logger class, which provides structured
 * logging to both the console and log files. The logger supports
 * multiple severity levels and timestamped output, making it
 * easier to trace and debug program execution.
 *
 * Features:
 *   - Logs messages at levels: TRACE, INFO, DEBUG, WARNING, ERROR, CRITICAL.
 *   - Timestamped entries with millisecond precision.
 *   - Console output with optional color-coding by severity level.
 *   - File logging separated into info/warning logs and error/critical logs.
 *   - Configurable log destinations (console and/or files).
 *
 * Platform-specific behavior:
 *   - Windows: Uses Win32 API (SetConsoleTextAttribute) for colored console output.
 *   - Other platforms: Uses ANSI escape codes for colored console output.
 *
 * Usage:
 *   Logger logger;
 *   logger.Log(LogLevel::INFO, "Application started");
 *   logger.Log(LogLevel::ERROR, "Failed to load configuration file");
 *
 *   logger.SetLogConsole(true);                // Enable console output
 *   logger.SetLogFile(LogLevel::INFO, "app.log"); // Redirect INFO/WARNING logs
 *   logger.SetLogFile(LogLevel::ERROR, "error.log"); // Redirect ERROR/CRITICAL logs
 */


#include "core/Logger.h"
#include <windows.h>
#include <chrono>      // for std::chrono::system_clock, duration_cast, etc.
#include <iomanip>     // for std::put_time, std::setfill, std::setw
#include <sstream>     // for std::stringstream
#include <ctime>       // for std::localtime (sometimes included automatically)

#ifdef ERROR
#undef ERROR
#endif

/**
 * @brief Constructor for the Logger class.
 *
 * Opens output file streams for info and error logs.
 * Files are created/appended at the filenames specified
 * by m_infoFile and m_errorFile.
 */
Logger::Logger() {
	m_infoStream.open(m_infoFile, std::ios::out | std::ios::app);
	m_errorStream.open(m_errorFile, std::ios::out | std::ios::app);
}

/**
 * @brief Destructor for the Logger class.
 *
 * Ensures that all open file streams are properly closed
 * when the Logger object is destroyed.
 */
Logger::~Logger() {
	if (m_infoStream.is_open()) m_infoStream.close();
	if (m_errorStream.is_open()) m_errorStream.close();
}

/**
 * @brief Logs a message with the specified log level.
 *
 * Dispatches to the appropriate private _logX() method
 * (info, debug, warning, error, critical) and attaches
 * a timestamp to each entry.
 *
 * @param level   The severity level of the message.
 * @param message The text message to log.
 */
void Logger::Log(const LogLevel level, const std::string& message) {
	if (!m_LogConsoleEnabled && !m_infoStream.is_open() && !m_errorStream.is_open()) {
		return; // No logging destination available
	}

	// Get timestamp once for this log call
	std::string timestamp = _getCurrentTimestamp("%H:%M:%S");

	// Forward to console callback if registered
	if (m_consoleCallback) {
		m_consoleCallback(level, timestamp, message);
	}

	switch (level) {
	case LogLevel::TRACE:	_logTrace(message); break;
	case LogLevel::INFO:	_logInfo(message); break;
	case LogLevel::DEBUG:	_logDebug(message); break;
	case LogLevel::WARNING: _logWarning(message); break;
	case LogLevel::ERROR:	_logError(message); break;
	case LogLevel::CRITICAL:_logCritical(message); break;
	}
}


void Logger::Log(const LogLevel level, const std::stringstream& oss) {
	Log(level, oss.str()); // forward to the string version
}

/**
 * @brief Sets or changes the log file for a given log level.
 *
 * For INFO/WARNING logs, assigns m_infoFile.
 * For ERROR/CRITICAL logs, assigns m_errorFile.
 * Reopens streams accordingly.
 *
 * @param level    The log level to redirect to a file.
 * @param filename The filename to use for that log file.
 */
void Logger::SetLogFile(const LogLevel level, const std::string& filename) {
	if (level == LogLevel::INFO || level == LogLevel::WARNING) {
		m_infoFile = filename;
		if (m_infoStream.is_open()) {
			m_infoStream.close();
		}
		m_infoStream.open(m_infoFile, std::ios::out | std::ios::app);
	}
	else if (level == LogLevel::ERROR || level == LogLevel::CRITICAL) {
		m_errorFile = filename;
		if (m_errorStream.is_open()) {
			m_errorStream.close();
		}
		m_errorStream.open(m_errorFile, std::ios::out | std::ios::app);
	}
}

/**
 * @brief Logs a TRACE message to console and/or log file
 * 
 * @param message	The message string to log.
 */
void Logger::_logTrace(const std::string& message) {
	if (m_LogConsoleEnabled)
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [TRC] " << message << '\n';
	if (m_infoStream.is_open())
		m_infoStream << "[" << _getCurrentTimestamp() << "] [TRC] " << message << '\n';
}

/**
 * @brief Logs an INFO message to console and/or info log file.
 *
 * @param message   The message string to log.
 */
void Logger::_logInfo(const std::string& message) {
	if (m_LogConsoleEnabled)
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [INF] " << message << '\n';
	if (m_infoStream.is_open())
		m_infoStream << "[" << _getCurrentTimestamp() << "] [INF] " << message << '\n';
}

/**
 * @brief Logs a DEBUG message to console and/or info log file.
 *
 * Only logs if debug mode is enabled.
 *
 * @param message   The message string to log.
 */
void Logger::_logDebug(const std::string& message) {
	if (!m_debugEnabled) return;
	if (m_LogConsoleEnabled)
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [DBG] " << message << '\n';
	if (m_infoStream.is_open())
		m_infoStream << "[" << _getCurrentTimestamp() << "] [DBG] " << message << '\n';
}

/**
 * @brief Logs a WARNING message to console (colored yellow) and/or info log file.
 *
 * @param message   The message string to log.
 */
void Logger::_logWarning(const std::string& message) {
	if (m_LogConsoleEnabled) {
		_setConsoleColor(LogLevel::WARNING);
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [WRN] " << message << '\n';
		_resetConsoleColor();
	}
	if (m_infoStream.is_open())
		m_infoStream << "[" << _getCurrentTimestamp() << "] [WRN] " << message << '\n';
}

/**
 * @brief Logs an ERROR message to console (colored red) and/or error log file.
 *
 * @param message   The message string to log.
 */
void Logger::_logError(const std::string& message) {
	if (m_LogConsoleEnabled) {
		_setConsoleColor(LogLevel::ERROR);
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [ERR] " << message << '\n';
		_resetConsoleColor();
	}
	if (m_errorStream.is_open())
		m_errorStream << "[" << _getCurrentTimestamp() << "] [ERR] " << message << '\n';
}

/**
 * @brief Logs a CRITICAL message to console (colored purple) and/or error log file.
 *
 * @param message   The message string to log.
 */
void Logger::_logCritical(const std::string& message) {
	if (m_LogConsoleEnabled) {
		_setConsoleColor(LogLevel::CRITICAL);
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [CRT] " << message << '\n';
		_resetConsoleColor();
	}
	if (m_errorStream.is_open())
		m_errorStream << "[" << _getCurrentTimestamp() << "] [CRT] " << message << '\n';
}

void Logger::_logTrace(const std::stringstream& oss) {
	_logTrace(oss.str());
}

void Logger::_logInfo(const std::stringstream& oss) {
	_logInfo(oss.str());
}

void Logger::_logDebug(const std::stringstream& oss) {
	_logDebug(oss.str());
}

void Logger::_logWarning(const std::stringstream& oss) {
	_logWarning(oss.str());
}

void Logger::_logError(const std::stringstream& oss) {
	_logError(oss.str());
}

void Logger::_logCritical(const std::stringstream& oss) {
	_logCritical(oss.str());
}

/**
 * @brief Enables or disables logging to the console.
 *
 * @param enable Boolean flag to allow or block console output.
 */
void Logger::SetLogConsole(const bool enable) {
	m_LogConsoleEnabled = enable;
}

/**
 * @brief Sets the console text color according to log severity.
 *
 * Windows: Uses Win32 API (SetConsoleTextAttribute).
 * Other platforms: Uses ANSI escape codes.
 *
 * @param level The log level determining the color.
 */
void Logger::_setConsoleColor(const LogLevel level) {
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	int colorCode = 7; // Default to light gray
	switch (level) {
	case LogLevel::TRACE: colorCode = 6; break; // Light yellow
	case LogLevel::WARNING: colorCode = 14; break; // Yellow
	case LogLevel::ERROR: colorCode = 12; break;   // Red
	case LogLevel::CRITICAL: colorCode = 13; break; // Light purple
	case LogLevel::DEBUG: colorCode = 11; break; // Light cyan
	case LogLevel::INFO: colorCode = 7; break; // Light gray
	}
	SetConsoleTextAttribute(hConsole, static_cast<WORD>(colorCode));
#else
	// ANSI escape codes for other platforms
	switch (level) {
	case LogLevel::TRACE std::cout << "\033[33m"; break; // Light yellow
	case LogLevel::WARNING: std::cout << "\033[93m"; break; // Yellow
	case LogLevel::ERROR: std::cout << "\033[91m"; break;   // Red
	case LogLevel::CRITICAL: std::cout << "\033[35m"; break; // Light purple
	case LogLevel::DEBUG: std::cout << "\033[36m"; break; // Light cyan
	case LogLevel::INFO: std::cout << "\033[37m"; break; // Light gray
	}
#endif
}

/**
 * @brief Resets the console text color back to default.
 *
 * Windows: Resets via SetConsoleTextAttribute.
 * Other platforms: Outputs ANSI reset code.
 */
void Logger::_resetConsoleColor() {
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, 7); // Reset to default white
#else
	std::cout << "\033[0m";
#endif
}

/**
 * @brief Gets the current timestamp string with millisecond precision.
 *
 * Uses std::chrono and std::put_time for formatting.
 * Example format: 2025-10-02 15:32:45.123
 *
 * @return Formatted timestamp string.
 */
std::string Logger::_getCurrentTimestamp(const std::string& format) {
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()) % 1000;

	std::tm timeInfo;
	(void)localtime_s(&timeInfo, &time_t);  // Safe version

	std::stringstream ss;
	ss << std::put_time(&timeInfo, format.c_str());
	// ss << '.' << std::setfill('0') << std::setw(3) << ms.count(); // milliseconds if needed
	return ss.str();
}