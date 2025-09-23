#include "systems/Logger.h"
#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif

Logger::Logger() {
	m_infoStream.open(m_infoFile, std::ios::out | std::ios::app);
	m_errorStream.open(m_errorFile, std::ios::out | std::ios::app);
}

void Logger::Log(const LogLevel level, const std::string& message) {
	if (!m_LogConsoleEnabled && !m_infoStream.is_open() && !m_errorStream.is_open()) {
		return; // No logging destination available
	}

	switch (level) {
	case INFO:	_logInfo(message); break;
	case DEBUG: _logDebug(message); break;
	case WARNING: _logWarning(message); break;
	case ERROR: _logError(message); break;
	case CRITICAL: _logCritical(message); break;
	}
}

void Logger::SetLogFile(const LogLevel level, const std::string& filename) {
	if (level == INFO || level == WARNING) {
		m_infoFile = filename;
		if (m_infoStream.is_open()) {
			m_infoStream.close();
		}
		m_infoStream.open(m_infoFile, std::ios::out | std::ios::app);
	}
	else if (level == ERROR || level == CRITICAL) {
		m_errorFile = filename;
		if (m_errorStream.is_open()) {
			m_errorStream.close();
		}
		m_errorStream.open(m_errorFile, std::ios::out | std::ios::app);
	}
}

void Logger::_logInfo(const std::string& message) {
	if (m_LogConsoleEnabled)
		std::cout << "[INF] " << message << '\n';
	if (m_infoStream.is_open())
		m_infoStream << "[INF] " << message << '\n';
}

void Logger::_logDebug(const std::string& message) {
	if (!m_debugEnabled) return;
	if (m_LogConsoleEnabled)
		std::cout << "[DBG] " << message << '\n';
	if (m_infoStream.is_open())
		m_infoStream << "[DBG] " << message << '\n';
}

void Logger::_logWarning(const std::string& message) {
	if (m_LogConsoleEnabled) {
		_setConsoleColor(WARNING);
		std::cout << "[WRN] " << message << '\n';
		_resetConsoleColor();
	}
	if (m_infoStream.is_open())
		m_infoStream << "[WRN] " << message << '\n';
}

void Logger::_logError(const std::string& message) {
	if (m_LogConsoleEnabled) {
		_setConsoleColor(LogLevel::ERROR);
		std::cout << "[ERR] " << message << '\n';
		_resetConsoleColor();
	}
	if (m_errorStream.is_open())
		m_errorStream << "[ERR] " << message << '\n';
}

void Logger::_logCritical(const std::string& message) {
	if (m_LogConsoleEnabled) {
		_setConsoleColor(CRITICAL);
		std::cout << "[CRT] " << message << '\n';
		_resetConsoleColor();
	}
	if (m_errorStream.is_open())
		m_errorStream << "[CRT] " << message << '\n';
}

void Logger::SetLogConsole(const bool enable) {
	m_LogConsoleEnabled = enable;
}

void Logger::_setConsoleColor(const LogLevel level) {
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	int colorCode = 7; // Default to light gray
	switch (level) {
	case WARNING: colorCode = 14; break; // Yellow
	case ERROR: colorCode = 12; break;   // Red
	case CRITICAL: colorCode = 13; break; // Light purple
	case DEBUG: colorCode = 11; break; // Light cyan
	case INFO: colorCode = 7; break; // Light gray
	}
	SetConsoleTextAttribute(hConsole, static_cast<WORD>(colorCode));
#else
	// ANSI escape codes for other platforms
	switch (level) {
	case WARNING: std::cout << "\033[33m"; break; // Yellow
	case ERROR: std::cout << "\033[31m"; break;   // Red
	case CRITICAL: std::cout << "\033[35m"; break; // Light purple
	case DEBUG: std::cout << "\033[36m"; break; // Light cyan
	case INFO: std::cout << "\033[37m"; break; // Light gray
	}
#endif
}
