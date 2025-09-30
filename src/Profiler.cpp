#include "Profiler.h"
#include "systems/Logger.h"
#include <algorithm> // for std::max_element
#include <iostream>
#include <stdexcept>
#include <numeric>
#include "DebugUI.h"

/*naming conventions:

public functions = files = FooBar()
private functions = _fooBar()
public data member = FooRealQuick
private data member = m_fooRealQuick
public macro = ALL_CAPS (includes global const)
*/

// Profiler::Profiler();

 //   Logger::Get().Log(INFO, "Profiler system initialized."); // print upon initialization 


double Profiler::fps = 0.0;
double Profiler::frameTimeMs = 0.0;
//Profiler::ScopeDataMap Profiler::m_scopes;
//std::unordered_map<std::string, std::chrono::steady_clock::time_point> Profiler::m_startTimes;

  //  DebugUI::NewFrame();

    /*"Logic System Time Consumption: " << PUT_HERE << "%" << std::endl
"Physics System Time Consumption: " << PUT_HERE << "%" << std::endl
"Collision System Time Consumption: " << PUT_HERE << "%" << std::endl
"Transform System Time Consumption: " << PUT_HERE << "%" << std::endl
"Audio System Time Consumption: " << PUT_HERE << "%" << std::endl
"Graphics System Time Consumption: " << PUT_HERE << "%" << std::endl*/

void Profiler::update_time(double fps_calc_interval) {
    // get elapsed time (in seconds) between previous and current frames
    /*static double prev_time = glfwGetTime();
    double curr_time = glfwGetTime();
    Profiler::delta_time = curr_time - prev_time;
    prev_time = curr_time;*/
    //double delta_time = Time::UnscaledDeltaTime();
    
    // 1. Calculate and store total frame time (in milliseconds)
    // Time::UnscaledDeltaTime() returns seconds, so multiply by 1000
    Profiler::frameTimeMs = Time::UnscaledDeltaTime() * 1000.0;

    // fps calculations
    static double count = 0.0; // number of game loop iterations
    //static double start_time = glfwGetTime();
    // get elapsed time since very beginning (in seconds) ...
    //double elapsed_time = curr_time - start_time;
    static double start_time = Time::ElapsedTime(); // Use Time's elapsed time


    ++count;

    // Get elapsed time since the last FPS update
    double elapsed_time = Time::ElapsedTime() - start_time;

    // update fps at least every 10 seconds ...
    fps_calc_interval = (fps_calc_interval < 0.0) ? 0.0 : fps_calc_interval;
    fps_calc_interval = (fps_calc_interval > 10.0) ? 10.0 : fps_calc_interval;
    if (elapsed_time > fps_calc_interval) {
        Profiler::fps = count / elapsed_time;
        start_time = Time::ElapsedTime();
        count = 0.0;
    }
}

// =========================================================================
// PUBLIC ACCESSORS FOR DEBUGUI
// =========================================================================
float Profiler::GetFPS() {
    // Cast to float for ImGui (which uses float for display)
    return static_cast<float>(Profiler::fps);
}

float Profiler::GetFrameTimeMs() {
    // Cast to float for ImGui
    return static_cast<float>(Profiler::frameTimeMs);
}

const Profiler::ScopeDataMap& Profiler::GetAllScopeData() {
    // Returns the map containing all system times
    return Profiler::Get().m_scopes;
}

void Profiler::ClearHistory() {
    // Clears the history for the 'Clear Performance History' button
    for (auto& [name, data] : Profiler::Get().m_scopes) {
        data.frameTimes.clear();
        data.avgTimeMs = 0.0f;
        data.maxTimeMs = 0.0f;
    }
    Logger::Get().Log(LogLevel::INFO, "Performance history cleared.");
}

// =========================================================================
// SCOPE-LEVEL TIME TRACKING
// =========================================================================

void Profiler::BeginScope(const std::string& scopeName) {
    m_startTimes[scopeName] = std::chrono::steady_clock::now();
}

void Profiler::EndScope(const std::string& scopeName) {
    auto it = m_startTimes.find(scopeName);
    if (it == m_startTimes.end()) {
        Logger::Get().Log(LogLevel::WARNING, "Profiler scope '" + scopeName + "' not found. Ignoring.");
        return;
    }
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - it->second);
    m_startTimes.erase(it);
    // Get the current scope data, or create it if it doesn't exist
    ScopeData& scopeData = m_scopes[scopeName];
    scopeData.lastTimeMs = static_cast<float>(duration.count()) / 1000.0f;
    // Maintain a history of frame times
    scopeData.frameTimes.push_back(scopeData.lastTimeMs);
    if (scopeData.frameTimes.size() > MAX_HISTORY_FRAMES) {
        scopeData.frameTimes.erase(scopeData.frameTimes.begin());
    }
    // Update avg and max
    float sum = std::accumulate(scopeData.frameTimes.begin(), scopeData.frameTimes.end(), 0.0f);
    scopeData.avgTimeMs = sum / scopeData.frameTimes.size();
    scopeData.maxTimeMs = *std::max_element(scopeData.frameTimes.begin(), scopeData.frameTimes.end());
    // Log to the console for post-mortem analysis
   // std::string output = "Scope '" + scopeName + "' took " + std::to_string(scopeData.lastTimeMs) + " ms.";
   // Logger::Get().Log(LogLevel::INFO, output);
}


