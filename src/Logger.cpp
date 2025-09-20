#include "Logger.h"
#include <GLFW/glfw3.h>
#include <windows.h>


/*naming conventions:

public functions = files = FooBar()
private functions = _fooBar()
public data member = FooRealQuick
private data member = m_fooRealQuick
public macro = ALL_CAPS (includes global const)
*/


void Logger::Initialize() {
    m_info_stream.open(m_infoFile, std::ios::out | std::ios::app);
    m_error_stream.open(m_errorFile, std::ios::out | std::ios::app);

    Log(INFO, "Logger system initialized.");

    m_gameTime = 0.0;      // accumulated simulation time in seconds
    m_lastUpdate = 0.0;    // last time we printed an uptime log
}

void Logger::Update() {
    /*auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_update_time);

    if (elapsed.count() >= 10) {
        auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time);
        Log(INFO, "Engine has been running for " + std::to_string(total_elapsed.count()) + " seconds.");
        m_last_update_time = now;
    }*/
    // Get the current time using glfwGetTime()
    m_gameTime = glfwGetTime(); // get delta time
    
    if (m_gameTime - m_lastUpdate >= 10.0) {
        Log(INFO, "Engine has been running for " + std::to_string((int)m_gameTime) + " seconds.");
        m_lastUpdate = m_gameTime;
    }
}

//users will call this function to log their stuff
void Logger::Log(LogSeverity type, const std::string& message) {
    if (!m_logToConsole && !m_info_stream.is_open() && !m_error_stream.is_open()) {
        return;
    }

    switch (type) {
    case INFO:    _info(message);    break;
   // case DEBUG:   _debug(message);   break;
    case WARNING: _warn(message);    break;
    case ERR:   _error(message);   break;
    }
} 

//changing to either infoFile or errorfile
void Logger::SetLogFile(LogSeverity type, const std::string& filename) {
    if (type == INFO || type == WARNING) {
        m_infoFile = filename;
        if (m_info_stream.is_open()) {
            m_info_stream.close();
        }
        m_info_stream.open(m_infoFile, std::ios::out | std::ios::app);
    }
    else if (type == ERROR) {
        m_errorFile = filename;
        if (m_error_stream.is_open()) {
            m_error_stream.close();
        }
        m_error_stream.open(m_errorFile, std::ios::out | std::ios::app);
    }
} 

//you want to log to files or console?
void Logger::SetLogToConsole(bool enable) {
    m_logToConsole = enable;
} 

std::string Logger::Name() const { return "Logger System"; }

// --- Private Implementation Functions ---

void Logger::_info(const std::string& message) {
    if (m_logToConsole) {
        std::cout << "[" << Name() << "][INF] " << message << std::endl;
    }
    if (m_info_stream.is_open()) {
        m_info_stream << "[" << Name() << "][INF] " << message << std::endl;
    }
}

//void Logger::m_debug(const std::string& message) {
//    if (!m_debugEnabled) {
//        return;
//    }
//    if (m_logToConsole) {
//        std::cout << "[" << Name() << "][DEBUG] " << message << std::endl;
//    }
//    if (m_info_stream.is_open()) {
//        m_info_stream << "[" << Name() << "][DEBUG] " << message << std::endl;
//    }
//}

void Logger::_warn(const std::string& message) {
    if (m_logToConsole) {
        std::cout << "[" << Name() << "][WRN] " << message << std::endl;
    }
    if (m_info_stream.is_open()) {
        m_info_stream << "[" << Name() << "][WRN] " << message << std::endl;
    }
}

void Logger::_error(const std::string& message) {
    if (m_logToConsole) {
        std::cerr << "[" << Name() << "][ERR] " << message << std::endl;
    }
    if (m_error_stream.is_open()) {
        m_error_stream << "[" << Name() << "][ERR] " << message << std::endl;
    }
}

void Logger::_setConsoleColor(LogSeverity type) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    int colorCode = 7; // Default: White
    switch (type) {
    case INFO:    colorCode = 3; break;   // Cyan
    case DEBUG:   colorCode = 2; break;   // Green
    case WARNING: colorCode = 6; break;   // Yellow
    case ERR:   colorCode = 4; break;   // Red
    }
    SetConsoleTextAttribute(hConsole, colorCode);
#else
    std::string colorCode;
    switch (type) {
    case INFO:    colorCode = "\033[0;36m"; break; // Cyan
    case DEBUG:   colorCode = "\033[0;32m"; break; // Green
    case WARNING: colorCode = "\033[0;33m"; break; // Yellow
    case ERROR:   colorCode = "\033[0;31m"; break; // Red
    default:      colorCode = "\033[0m";   break; // Reset
    }
    std::cout << colorCode;
#endif
}


void Logger::_resetConsoleColor() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, 7); // Reset to default white
#else
    std::cout << "\033[0m";
#endif
}