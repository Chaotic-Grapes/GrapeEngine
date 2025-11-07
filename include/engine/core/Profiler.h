/* Start Header *****************************************************************/
/*!
\file   Profiler.h
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   7th November 2025
\brief
Declares the Profiler for FPS/frame time tracking and scoped performance measurement,
plus RAII ProfileScope helper.
*/
/* End Header *******************************************************************/

#ifndef PROFILER_H
#define PROFILER_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
//#include <fstream> 
#include <chrono> // for wall-clock time and timelapse
#include <unordered_map>
#include "services/Time.h"

// A structure to hold profiling data for a single scope
struct ScopeData {
    // Stores a history of the last N frame times (in milliseconds)
    std::vector<float> FrameTimes;
    float LastTimeMs = 0.0f;
    float AverageTimeMs = 0.0f;
    float MaxTimeMs = 0.0f;
};

class Profiler {
public:

    // Define the map type for clarity
    using ScopeDataMap = std::map<std::string, ScopeData>;

    // Get the singleton instance of the Profiler.
    static Profiler& Get() {
        static Profiler instance;
        return instance;
    }

    // Update FPS and frame time; call once per frame.
    static void UpdateTime(double fpsCalcInt = 1.0);

    // Static member declarations
    // Latest FPS computed by UpdateTime.
    static double Fps;
    // Latest frame time (ms) computed by UpdateTime.
    static double FrameTimeMs;

    // Read-only access to all profiling scopes (e.g., for Debug UI).
    const ScopeDataMap& GetScopes() const { return m_scopes; }

    // Returns current frames-per-second.
    static float GetFPS();
    // Returns current frame time in milliseconds.
    static float GetFrameTimeMs();
    // Read-only access to all scope timing data.
    static const ScopeDataMap& GetAllScopeData();
    // Accumulated total of all scope times measured in the last frame.
    static double GetTotalScopeTimes();
    // Clears timing history for all scopes.
    static void ClearHistory();

    // Begin a timing scope (used by ProfileScope).
    void BeginScope(const std::string& scopeName);

    // End a timing scope and record elapsed time.
    void EndScope(const std::string& scopeName);

private:
    Profiler() = default;
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    std::map<std::string, std::chrono::steady_clock::time_point> m_startTimes;
    std::map<std::string, ScopeData> m_scopes;

    // Cached sum of all scope timings from the last frame.
    static double m_lastTotalScopeTime;

    // Number of recent frames retained per scope.
    static const int MAX_HISTORY_FRAMES = 120;
};


// A helper class that automatically profiles a code block.
// Uses the RAII (Resource Acquisition Is Initialization) pattern.
class ProfileScope {
public:
    // Begins a profiling scope upon construction.
    explicit ProfileScope(const std::string& scopeName)
        : m_scopeName(scopeName) {
        Profiler::Get().BeginScope(m_scopeName);
    }

    // Ends the profiling scope upon destruction.
    ~ProfileScope() {
        Profiler::Get().EndScope(m_scopeName);
    }

private:
    std::string m_scopeName;

};

#endif