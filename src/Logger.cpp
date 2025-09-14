#include "Logger.h"

/*naming conventions:

public functions = files = FooBar()
private functions = _fooBar()
public data member = FooRealQuick
private data member = m_fooRealQuick
public macro = ALL_CAPS (includes global const)*/


void Logger::Initialize() {
    m_info_stream.open(m_infoFile, std::ios::out | std::ios::app);
    m_error_stream.open(m_errorFile, std::ios::out | std::ios::app);

    Log(INFO, "Logger system initialized.");

    m_start_time = std::chrono::steady_clock::now();
    m_last_update_time = m_start_time;
}

void Logger::Update() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_update_time);

    if (elapsed.count() >= 10) {
        auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time);
        Log(INFO, "Engine has been running for " + std::to_string(total_elapsed.count()) + " seconds.");
        m_last_update_time = now;
    }
}

//users will call this function to log their stuff
void Logger::Log(LogSeverity type, const std::string& message) {
    if (!m_logToConsole && !m_info_stream.is_open() && !m_error_stream.is_open()) {
        return;
    }

    switch (type) {
    case INFO:    m_info(message);    break;
   // case DEBUG:   m_debug(message);   break;
    case WARNING: m_warn(message);    break;
    case ERROR:   m_error(message);   break;
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

void Logger::m_info(const std::string& message) {
    if (m_logToConsole) {
        std::cout << "[" << Name() << "][INFO] " << message << std::endl;
    }
    if (m_info_stream.is_open()) {
        m_info_stream << "[" << Name() << "][INFO] " << message << std::endl;
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

void Logger::m_warn(const std::string& message) {
    if (m_logToConsole) {
        std::cout << "[" << Name() << "][WARNING] " << message << std::endl;
    }
    if (m_info_stream.is_open()) {
        m_info_stream << "[" << Name() << "][WARNING] " << message << std::endl;
    }
}

void Logger::m_error(const std::string& message) {
    if (m_logToConsole) {
        std::cerr << "[" << Name() << "][ERROR] " << message << std::endl;
    }
    if (m_error_stream.is_open()) {
        m_error_stream << "[" << Name() << "][ERROR] " << message << std::endl;
    }
}
