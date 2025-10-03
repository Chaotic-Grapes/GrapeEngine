#include "Profiler.h"
#include "systems/Logger.h"
#include <algorithm> // for std::max_element
#include <numeric>
#include "DebugUI.h"

 // Logger::Get().Log(INFO, "Profiler system initialized."); // print upon initialization 

double Profiler::Fps = 0.0;
double Profiler::FrameTimeMs = 0.0;
// Profiler::ScopeDataMap Profiler::m_scopes;
// std::unordered_map<std::string, std::chrono::steady_clock::time_point> Profiler::m_startTimes;

void Profiler::UpdateTime(double fpsCalcInt) {
    // get elapsed time (in seconds) between previous and current frames
    /*static double prev_time = glfwGetTime();
    double curr_time = glfwGetTime();
    Profiler::delta_time = curr_time - prev_time;
    prev_time = curr_time;*/
    //double delta_time = Time::UnscaledDeltaTime();
    
    // 1. Calculate and store total frame time (in milliseconds)
    // Time::UnscaledDeltaTime() returns seconds, so multiply by 1000
    Profiler::FrameTimeMs = Time::UnscaledDeltaTime() * 1000.0;

    // fps calculations
    static double count = 0.0; // number of game loop iterations
    //static double start_time = glfwGetTime();
    // get elapsed time since very beginning (in seconds) ...
    //double elapsed_time = curr_time - start_time;
    static double startTime = Time::ElapsedTime(); // Use Time's elapsed time

    ++count;

    // Get elapsed time since the last FPS update
    const double elapsedTime = Time::ElapsedTime() - startTime;

    // update fps at least every 10 seconds ...
    fpsCalcInt = (fpsCalcInt < 0.0) ? 0.0 : fpsCalcInt;
    fpsCalcInt = (fpsCalcInt > 10.0) ? 10.0 : fpsCalcInt;
    if (elapsedTime > fpsCalcInt) {
        Profiler::Fps = count / elapsedTime;
        startTime = Time::ElapsedTime();
        count = 0.0;
    }
}

// =========================================================================
// PUBLIC ACCESSORS FOR DEBUGUI
// =========================================================================
float Profiler::GetFPS() {
    // Cast to float for ImGui (which uses float for display)
    return static_cast<float>(Profiler::Fps);
}

float Profiler::GetFrameTimeMs() {
    // Cast to float for ImGui
    return static_cast<float>(Profiler::FrameTimeMs);
}

const Profiler::ScopeDataMap& Profiler::GetAllScopeData() {
    // Returns the map containing all system times
    return Profiler::Get().m_scopes;
}

double Profiler::GetTotalScopeTimes() {
	double total = 0.0;
    for (const auto& [_, data] : Profiler::Get().m_scopes) {
        total += static_cast<double>(data.LastTimeMs);
	}
	return total;
}

void Profiler::ClearHistory() {
    // Clears the history for the 'Clear Performance History' button
    for (auto& [name, data] : Profiler::Get().m_scopes) {
        data.FrameTimes.clear();
        data.AverageTimeMs = 0.0f;
        data.MaxTimeMs = 0.0f;
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
    const auto it = m_startTimes.find(scopeName);
    if (it == m_startTimes.end()) {
        Logger::Get().Log(LogLevel::WARNING, "Profiler scope '" + scopeName + "' not found. Ignoring.");
        return;
    }
    const auto endTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - it->second);
    m_startTimes.erase(it);

    // Get the current scope data, or create it if it doesn't exist
    auto& [FrameTimes, LastTimeMs, AverageTimeMs, MaxTimeMs] = m_scopes[scopeName];
    LastTimeMs = static_cast<float>(duration.count()) / 1000.0f;

    // Maintain a history of frame times
    FrameTimes.push_back(LastTimeMs);
    if (FrameTimes.size() > MAX_HISTORY_FRAMES) {
        FrameTimes.erase(FrameTimes.begin());
    }

    // Update avg and max
    const float sum = std::accumulate(FrameTimes.begin(), FrameTimes.end(), 0.0f);
    AverageTimeMs = sum / static_cast<float>(FrameTimes.size());
    MaxTimeMs = *std::max_element(FrameTimes.begin(), FrameTimes.end());
    // Log to the console for post-mortem analysis
    // std::string output = "Scope '" + scopeName + "' took " + std::to_string(scopeData.LastTimeMs) + " ms.";
    // Logger::Get().Log(LogLevel::INFO, output);
}
