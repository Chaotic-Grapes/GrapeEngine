/* Start Header *****************************************************************/
/*!
\file   Profiler.cpp
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   7th November 2025
\brief
Implements runtime performance profiling: per‑frame FPS and frame time tracking,
plus scoped timings with bounded history, averages, and maxima. Integrates with
DebugUI for live visualization.

Responsibilities:
- Update FPS and frame time each frame via `UpdateTime()`.
- Measure code sections using `BeginScope()` / `EndScope()` or RAII `ProfileScope`.
- Provide accessors to current metrics, scope histories, and aggregated totals.

Features:
- Bounded history per scope (up to `MAX_HISTORY_FRAMES`).
- Average and maximum time computation for quick trend analysis.
- Simple total scope time aggregation for frame budget overview.
- ClearHistory() at runtime for fresh profiling sessions.

Usage:
- `Profiler::UpdateTime(1.0);` // call once per frame, recompute FPS every 1s
- `ProfileScope physicsScope("Physics");` // RAII scope measurement
- Or manual pairing: `BeginScope("Render"); ...; EndScope("Render");`

Notes:
- Overhead is low; when disabled, functions short‑circuit quickly.
- Not thread‑safe by design; use on main thread or guard externally.

Dependencies:
- `services/Time` for timing, `<chrono>`, `<numeric>`, `<algorithm>` for math utilities.
- `Logger` for warnings/status.
*/
/* End Header *******************************************************************/


#include "core/Profiler.h"
#include "core/Logger.h"
#include <algorithm> // for std::max_element
#include <numeric>
#include "services/DebugUI.h"

 // Logger::Get().Log(INFO, "Profiler system initialized."); // print upon initialization 

double Profiler::Fps = 0.0;
double Profiler::FrameTimeMs = 0.0;
// Profiler::ScopeDataMap Profiler::m_scopes;
// std::unordered_map<std::string, std::chrono::steady_clock::time_point> Profiler::m_startTimes;


// Update FPS and frame time; uses Time service and interval-based recalculation.
void Profiler::UpdateTime(double fpsCalcInt) {
    // Compute total frame time (ms): Time::UnscaledDeltaTime() returns seconds.
    
    Profiler::FrameTimeMs = Time::UnscaledDeltaTime() * 1000.0;

    // Maintain frame counter and start time of current FPS window.
    static double count = 0.0;
    static double startTime = Time::ElapsedTime(); // Use Time's elapsed time

    ++count;

    // Seconds elapsed since last FPS update window.
    const double elapsedTime = Time::ElapsedTime() - startTime;

    // Clamp update interval to [0, 10] seconds to avoid extreme values.
    fpsCalcInt = (fpsCalcInt < 0.0) ? 0.0 : fpsCalcInt;
    fpsCalcInt = (fpsCalcInt > 10.0) ? 10.0 : fpsCalcInt;
    if (elapsedTime > fpsCalcInt) {
        // Recalculate FPS: frames counted divided by seconds elapsed.
        Profiler::Fps = count / elapsedTime;
        // Reset window start and frame counter.
        startTime = Time::ElapsedTime();
        count = 0.0;
    }
}

// =========================================================================
// PUBLIC ACCESSORS FOR DEBUGUI
// =========================================================================
// Return current frames-per-second as float for UI display.
float Profiler::GetFPS() {
    // Cast to float for ImGui (which uses float for display)
    return static_cast<float>(Profiler::Fps);
}

// Return current frame time in milliseconds as float for UI display.
float Profiler::GetFrameTimeMs() {
    // Cast to float for ImGui
    return static_cast<float>(Profiler::FrameTimeMs);
}

// Retrieve const reference to all scope timing data (last/avg/max per scope).
const Profiler::ScopeDataMap& Profiler::GetAllScopeData() {
    // Returns the map containing all system times
    return Profiler::Get().m_scopes;
}

// Return total of all scope times measured in the last frame (ms).
double Profiler::GetTotalScopeTimes() {
	double total = 0.0;
    for (const auto& [_, data] : Profiler::Get().m_scopes) {
        total += static_cast<double>(data.LastTimeMs);
	}
	return total;
}

// Clear performance history for all scopes; resets history, avg, and max.
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
// Begin a profiling scope: record start time for later duration calculation.
void Profiler::BeginScope(const std::string& scopeName) {
    m_startTimes[scopeName] = std::chrono::steady_clock::now();
}

// End a profiling scope: compute duration, update history, and summaries.
void Profiler::EndScope(const std::string& scopeName) {
    const auto it = m_startTimes.find(scopeName);
    if (it == m_startTimes.end()) {
        // Guard against missing BeginScope() calls.
        Logger::Get().Log(LogLevel::WARNING, "Profiler scope '" + scopeName + "' not found. Ignoring.");
        return;
    }
    const auto endTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - it->second);
    m_startTimes.erase(it);

    // Get or create scope data record.
    auto& [FrameTimes, LastTimeMs, AverageTimeMs, MaxTimeMs] = m_scopes[scopeName];
    // Convert microseconds to milliseconds for storage.
    LastTimeMs = static_cast<float>(duration.count()) / 1000.0f;

    // Maintain bounded history of recent frame times.
    FrameTimes.push_back(LastTimeMs);
    if (FrameTimes.size() > MAX_HISTORY_FRAMES) {
        FrameTimes.erase(FrameTimes.begin());
    }

    // Update average and maximum over history window.
    const float sum = std::accumulate(FrameTimes.begin(), FrameTimes.end(), 0.0f);
    AverageTimeMs = sum / static_cast<float>(FrameTimes.size());
    MaxTimeMs = *std::max_element(FrameTimes.begin(), FrameTimes.end());
    // Log to the console for post-mortem analysis
    // std::string output = "Scope '" + scopeName + "' took " + std::to_string(scopeData.LastTimeMs) + " ms.";
    // Logger::Get().Log(LogLevel::INFO, output);
}
