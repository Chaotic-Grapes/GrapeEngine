#ifndef LOGGER_H
#define LOGGER_H
#include "ISystem.h"
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>

/*naming conventions:

public functions = files = FooBar()
private functions = _fooBar()
public data member = FooRealQuick
private data member = m_fooRealQuick
public macro = ALL_CAPS (includes global const)*/

enum LogSeverity {
    INFO,
    DEBUG,
    WARNING,
    ERROR
};

class Logger : public Engine::ISystem {
public:
    /// Call this function before any other methods of the Logger class
    void Initialize() override;

    /// This will update all time variables that need modifications
    /// To be called ONLY by the Engine's Update() loop
    void Update() override;

    /// Debug name
    std::string Name() const override;

    // This implements the Singleton pattern, ensuring only one instance of Logger exists.
    static Logger& Get() {
        static Logger instance;
        return instance;
    }

    void Log(LogSeverity type, const std::string& message); //users will call this function to log their stuff
    void SetLogFile(LogSeverity type, const std::string& filename); //changing to either infoFile or errorfile
    void SetLogToConsole(bool enable); //you want to log to files or console?
    
    //Logger.Log(std::string message, LogSeverity severity) 

    /* user input - mouse click and key press, event, action ? under DEBUG ?
     Those kinds of actions would typically be managed by a separate Input System that also inherits from ISystem.
     The InputSystem would then call Logger::Get().Log(...) to report user events*/
        
private:
    // Logging functions
    //void writeCrashLog(LogSeverity level, const std::string& message);
    //void DebugMsg(const std::string& message);
    void m_info(const std::string& message);
    void m_warn(const std::string& message);
    void m_error(const std::string& message);

    // Configuration
    //void SetLogToFile(bool enable, const std::string& filename = "engine.log"); // don't need
    
    
private:
    std::string m_infoFile{ "engine.log" }; //data member tracks filname
    std::string m_errorFile{ "errors.log" };

    std::ofstream m_info_stream;
    std::ofstream m_error_stream;

    bool m_logToConsole{ true };
    bool m_debugEnabled{ true };

    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_update_time;
};

#endif // LOGGER_H