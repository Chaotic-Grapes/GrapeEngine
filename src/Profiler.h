#ifndef PROFILER_H
#define PROFILER_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <fstream> 
#include <chrono> // for wall-clock time and timelapse
#include "systems/Time.h"

/*naming conventions:

public functions = files = FooBar()
private functions = _fooBar()
public data member = FooRealQuick
private data member = m_fooRealQuick
public macro = ALL_CAPS (includes global const)*/

// A structure to hold profiling data for a single scope
struct ScopeData {
    // Stores a history of the last N frame times (in milliseconds)
    std::vector<float> frameTimes;
    float lastTimeMs = 0.0f;
    float avgTimeMs = 0.0f;
    float maxTimeMs = 0.0f;
};

class Profiler {
public:

    /**
     * @brief Get the singleton instance of the Profiler
     * @return Reference to the Profiler instance
     */
    static Profiler& Get() {
        static Profiler instance;
        return instance;
    }


    void RenderUI(); // Display profiler performance data in ImGui window

    /// Gets a const reference to the map of all profiling scopes.
    /// This allows other systems (like the DebugUI) to read the data.
    const std::map<std::string, ScopeData>& GetScopes() const { return m_scopes; }

    /// Starts a timing scope. This is a public method but is
    /// intended to be used by the ProfileScope class.
    void BeginScope(const std::string& scopeName);

    /// Ends a timing scope and logs the elapsed time. This is a public method but is
    /// intended to be used by the ProfileScope class.
    void EndScope(const std::string& scopeName);

private:
    Profiler() = default;
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    std::map<std::string, std::chrono::steady_clock::time_point> m_startTimes;
    std::map<std::string, ScopeData> m_scopes;

    static const int MAX_HISTORY_FRAMES = 120;
};


// A helper class that automatically profiles a code block.
// Uses the RAII (Resource Acquisition Is Initialization) pattern.
class ProfileScope {
public:
    explicit ProfileScope(const std::string& scopeName)
        : m_scopeName(scopeName) {
        Profiler::Get().BeginScope(m_scopeName);
    }

    ~ProfileScope() {
        Profiler::Get().EndScope(m_scopeName);
    }

private:
    std::string m_scopeName;

};

#endif