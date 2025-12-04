/**
 * @Name: Samantha Leong, 2403088
 * @email: s.leong@digipen.edu
 * @file Profiler.cpp
 * @brief Implements the profiling system for performance measurement.
 *
 * This file defines the Profiler class, which tracks system performance
 * and execution times for debugging and optimization purposes. It can
 * measure global metrics (FPS, frame time) and scoped timings of specific
 * systems or functions.
 *
 * Features:
 *   - Calculates FPS (frames per second) and frame time (ms).
 *   - Tracks scoped timing via BeginScope() / EndScope().
 *   - Stores history of frame times for each scope (up to MAX_HISTORY_FRAMES).
 *   - Provides average and maximum times for each scope.
 *   - Supports clearing performance history during runtime.
 *   - Integrates with DebugUI (e.g., ImGui) for live display.
 *
 * Dependencies:
 *   - Logger system for reporting warnings and status.
 *   - C++ standard library (chrono, numeric, algorithm).
 *
 * Usage:
 *   // Update each frame
 *   Profiler::UpdateTime(1.0);   // Update every 1 second
 *
 *   // Scoped profiling
 *   Profiler::BeginScope("Physics");
 *   RunPhysicsStep();
 *   Profiler::EndScope("Physics");
 *
 *   // Query data
 *   float fps = Profiler::GetFPS();
 *   float frameTime = Profiler::GetFrameTimeMs();
 *   auto& scopes = Profiler::GetAllScopeData();
 */


#include "core/Profiler.h"
#include "core/Logger.h"
#include <algorithm> // for std::max_element
#include <numeric>

 // Logger::Get().Log(INFO, "Profiler system initialized."); // print upon initialization 

double Profiler::Fps = 0.0;
double Profiler::FrameTimeMs = 0.0;
// Profiler::ScopeDataMap Profiler::m_scopes;
// std::unordered_map<std::string, std::chrono::steady_clock::time_point> Profiler::m_startTimes;


/**
 * @brief Updates FPS and frame time calculations.
 *
 * Uses Time::UnscaledDeltaTime() to measure frame duration in ms.
 * FPS is recalculated at intervals (default every 1s, max 10s).
 *
 * @param fpsCalcInt Interval in seconds between FPS updates.
 */
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
/**
 * @brief Returns the current FPS as a float.
 * @return Frames per second.
 */
float Profiler::GetFPS() {
    // Cast to float for ImGui (which uses float for display)
    return static_cast<float>(Profiler::Fps);
}

/**
 * @brief Returns the current frame time in milliseconds.
 * @return Frame time in ms.
 */
float Profiler::GetFrameTimeMs() {
    // Cast to float for ImGui
    return static_cast<float>(Profiler::FrameTimeMs);
}

/**
 * @brief Retrieves all scope timing data.
 *
 * @return const reference to ScopeDataMap containing
 *         last, average, and max times per scope.
 */
const Profiler::ScopeDataMap& Profiler::GetAllScopeData() {
    // Returns the map containing all system times
    return Profiler::Get().m_scopes;
}

/**
 * @brief Returns the total execution time of all scopes combined.
 *
 * @return Total scope time in milliseconds.
 */
double Profiler::GetTotalScopeTimes() {
	double total = 0.0;
    for (const auto& [_, data] : Profiler::Get().m_scopes) {
        total += static_cast<double>(data.LastTimeMs);
	}
	return total;
}

/**
 * @brief Clears performance history for all scopes.
 *
 * Resets frame time history, average, and maximum values.
 * Useful when user presses "Clear Performance History".
 */
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
/**
 * @brief Marks the beginning of a profiling scope.
 *
 * Records the current time for later duration calculation.
 *
 * @param scopeName Unique identifier of the scope (e.g., "Physics").
 */
void Profiler::BeginScope(const std::string& scopeName) {
    m_startTimes[scopeName] = std::chrono::steady_clock::now();
}

/**
 * @brief Marks the end of a profiling scope and stores its duration.
 *
 * - Calculates elapsed time since BeginScope().
 * - Updates scope's frame history, last time, average, and max.
 * - Logs a warning if EndScope() is called without BeginScope().
 *
 * @param scopeName Unique identifier of the scope.
 */
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
