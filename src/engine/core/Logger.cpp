/* Start Header *****************************************************************/
/*!
\file   Logger.cpp
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   7th November 2025
\brief
Provides structured logging to console and files with severity levels, timestamps,
and optional colorized output. Designed for low‑friction diagnostics and clean
separation of informational vs error streams.

Responsibilities:
- Central logging dispatch via `Log(level, message)` and stringstream overload.
- Timestamp formatting and severity tags for consistent output.
- Separate destinations: info/warning vs error/critical log files.
- Toggle console output and guard DEBUG logs behind a runtime flag.

Features:
- Levels: TRACE, INFO, DEBUG, WARNING, ERROR, CRITICAL.
- Console coloring by severity (Windows API or ANSI on other platforms).
- File logging in append mode to preserve history across runs.
- Lightweight helpers for each severity to keep call sites concise.

Usage:
- `Logger::Get().Log(LogLevel::INFO, "Application started");`
- `Logger::Get().SetLogConsole(true);` // enable console output
- `Logger::Get().SetLogFile(LogLevel::INFO, "engine.log");`
- `Logger::Get().SetLogFile(LogLevel::ERROR, "error.log");`

Notes:
- Thread safety: streams are not synchronized; wrap externally if used from
  multiple threads concurrently.
- Performance: formatting and I/O are minimal; avoid logging in hot inner loops
  unless required.

Dependencies:
- C++ standard library (`chrono`, `iomanip`, `sstream`).
- Windows console API for coloring (`SetConsoleTextAttribute`); ANSI codes elsewhere.
*/
/* End Header *******************************************************************/


#include "core/Logger.h"
#include <windows.h>
#include <chrono>      // for std::chrono::system_clock, duration_cast, etc.
#include <iomanip>     // for std::put_time, std::setfill, std::setw
#include <sstream>     // for std::stringstream
#include <ctime>       // for std::localtime (sometimes included automatically)

#ifdef ERROR
#undef ERROR
#endif

// Constructor: open output file streams for info and error logs.
Logger::Logger() {
    // Open file streams in append mode so logs accumulate across runs.
	m_infoStream.open(m_infoFile, std::ios::out | std::ios::app);
	m_errorStream.open(m_errorFile, std::ios::out | std::ios::app);
}

// Destructor: ensure streams are closed on destruction.
Logger::~Logger() {
    // Close files if open.
	if (m_infoStream.is_open()) m_infoStream.close();
	if (m_errorStream.is_open()) m_errorStream.close();
}

// Log a message with the specified level; dispatch to _logX() helpers.
void Logger::Log(const LogLevel level, const std::string& message) {
    // No-op if console is disabled and no file destinations are open.
	if (!m_LogConsoleEnabled && !m_infoStream.is_open() && !m_errorStream.is_open()) {
		return; // No logging destination available
	}

    // Route by level; helpers format timestamp and destinations.
	switch (level) {
	case LogLevel::TRACE:	_logTrace(message); break;
	case LogLevel::INFO:	_logInfo(message); break;
	case LogLevel::DEBUG:	_logDebug(message); break;
	case LogLevel::WARNING: _logWarning(message); break;
	case LogLevel::ERROR:	_logError(message); break;
	case LogLevel::CRITICAL:_logCritical(message); break;
	}
}


// Log a message from a stringstream by forwarding to the string overload.
void Logger::Log(const LogLevel level, const std::stringstream& oss) {
	Log(level, oss.str()); // forward to the string version
}

// Set or change the log file for a given level; reopens streams accordingly.
void Logger::SetLogFile(const LogLevel level, const std::string& filename) {
    // INFO/WARNING share one file; ERROR/CRITICAL share another.
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

// TRACE: write to console and/or info file.
void Logger::_logTrace(const std::string& message) {
    // Console output with short timestamp (HH:MM).
	if (m_LogConsoleEnabled)
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [TRC] " << message << '\n';
    // File output with full timestamp.
	if (m_infoStream.is_open())
		m_infoStream << "[" << _getCurrentTimestamp() << "] [TRC] " << message << '\n';
}

// INFO: write to console and/or info file.
void Logger::_logInfo(const std::string& message) {
	if (m_LogConsoleEnabled)
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [INF] " << message << '\n';
	if (m_infoStream.is_open())
		m_infoStream << "[" << _getCurrentTimestamp() << "] [INF] " << message << '\n';
}

// DEBUG: write to console and/or info file when debug enabled.
void Logger::_logDebug(const std::string& message) {
	if (!m_debugEnabled) return;
	if (m_LogConsoleEnabled)
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [DBG] " << message << '\n';
	if (m_infoStream.is_open())
		m_infoStream << "[" << _getCurrentTimestamp() << "] [DBG] " << message << '\n';
}

// WARNING: console (colored), and info file.
void Logger::_logWarning(const std::string& message) {
	if (m_LogConsoleEnabled) {
		_setConsoleColor(LogLevel::WARNING);
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [WRN] " << message << '\n';
		_resetConsoleColor();
	}
	if (m_infoStream.is_open())
		m_infoStream << "[" << _getCurrentTimestamp() << "] [WRN] " << message << '\n';
}

// ERROR: console (colored), and error file.
void Logger::_logError(const std::string& message) {
	if (m_LogConsoleEnabled) {
		_setConsoleColor(LogLevel::ERROR);
		std::cout << "[" << _getCurrentTimestamp("%H:%M") << "] [ERR] " << message << '\n';
		_resetConsoleColor();
	}
	if (m_errorStream.is_open())
		m_errorStream << "[" << _getCurrentTimestamp() << "] [ERR] " << message << '\n';
}

// CRITICAL: console (colored), and error file.
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

// Enable or disable logging to the console.
void Logger::SetLogConsole(const bool enable) {
	m_LogConsoleEnabled = enable;
}

// Set console text color according to log severity (platform-specific).
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
	case LogLevel::TRACE: std::cout << "\033[33m"; break; // Light yellow
	case LogLevel::WARNING: std::cout << "\033[93m"; break; // Yellow
	case LogLevel::ERROR: std::cout << "\033[91m"; break;   // Red
	case LogLevel::CRITICAL: std::cout << "\033[35m"; break; // Light purple
	case LogLevel::DEBUG: std::cout << "\033[36m"; break; // Light cyan
	case LogLevel::INFO: std::cout << "\033[37m"; break; // Light gray
	}
#endif
}

// Reset the console text color back to default.
void Logger::_resetConsoleColor() {
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, 7); // Reset to default white
#else
	std::cout << "\033[0m";
#endif
}

// Get current timestamp string using std::chrono and std::put_time.
std::string Logger::_getCurrentTimestamp(const std::string& format) {
    // Extract system time and format it with std::put_time.
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