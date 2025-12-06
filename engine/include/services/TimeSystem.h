/* Start Header *****************************************************************/
/*!
\file   TimeSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   14th September 2025
\brief
Declares the time service using std::chrono; provides delta, smoothing,
fixed-step accumulator, time-scaling, and a simple profiler sampling API.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef TIMESYSTEM_H
#define TIMESYSTEM_H

#include "Export.h"
#include <chrono>
#include <vector>
#include <deque>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <string>
#include <memory>
#include <unordered_map>

class GRAPEENGINE_API TimeSystem {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<double>;

    struct ProfileSample {
        char Name[64];
        double DurationSeconds = 0.0;
    };

    using ProfilerCollector = std::function<void(const std::vector<ProfileSample>&)>;

    /**
     * @page TimeSystem_Usage TimeSystem usage examples
     *
     * Typical main-loop integration:
     * @code{.cpp}
     * using namespace std::chrono;
     * double lastNow = duration<double>(steady_clock::now().time_since_epoch()).count();
     * for (;;) {
     *     double now = duration<double>(steady_clock::now().time_since_epoch()).count();
     *     double rawDelta = now - lastNow;
     *     lastNow = now;
     *
     *     // Advance the TimeSystem once per frame
     *     TimeSystem::Instance().Advance(rawDelta, now);
     *
     *     // Use scaled delta for gameplay updates
     *     double dt = TimeSystem::Instance().GetDeltaTime();
     *     UpdateGame(dt);
     *
     *     // Use fixed-step loop for deterministic physics
     *     double fixed = TimeSystem::Instance().GetFixedTimeStep();
     *     double accumulator = 0.0;
     *     accumulator += TimeSystem::Instance().GetUnscaledDeltaTime();
     *     while (accumulator >= fixed) {
     *         PhysicsStep(fixed);
     *         accumulator -= fixed;
     *     }
     * }
     * @endcode
     *
     * Smoothing vs raw delta:
     * @code{.cpp}
     * // If a system needs a less noisy delta, use GetSmoothedDeltaTime().
     * double smoothDt = TimeSystem::Instance().GetSmoothedDeltaTime();
     * @endcode
     *
     * Profiler sampling example (collector + RAII scope helper):
     * @code{.cpp}
     * // Install a simple collector to print samples once per Advance().
     * TimeSystem::Instance().SetProfilerCollector([](const std::vector<TimeSystem::ProfileSample>& samples){
     *     for (const auto &s : samples) {
     *         printf("%s: %f\n", s.Name, s.DurationSeconds);
     *     }
     * });
     *
     * // Small RAII helper for scoped profiling
     * struct ProfileScope {
     *     const char* name;
     *     ProfileScope(const char* n) : name(n) { TimeSystem::Instance().ProfileBegin(name); }
     *     ~ProfileScope() { TimeSystem::Instance().ProfileEnd(); }
     * };
     *
     * // Usage
     * {
     *     ProfileScope p("Render");
     *     RenderFrame();
     * }
     * @endcode
     */

    /**
     * @brief Get the global TimeSystem singleton instance.
     * @return Reference to the TimeSystem singleton.
     */
    static TimeSystem& Instance();

    /**
     * @brief Advance the time system by a raw delta and a provided absolute now.
     * @param rawDeltaSeconds Unscaled delta time in seconds since last advance.
     * @param nowSeconds Absolute time in seconds (platform clock) for this tick.
     *
     * This should be called once per frame from the engine main loop.
     */
    void Advance(double rawDeltaSeconds, double nowSeconds);

    /**
     * @name Time scaling and queries
     */
    /** @{ */
    /** @brief Set global time scale (1.0 = realtime).
     *  @param s Time scale multiplier.
     */
    void SetTimeScale(double s);
    /** @brief Get current time scale. */
    double GetTimeScale() const;

    /** @brief Get scaled delta time (seconds) used for update steps. */
    double GetDeltaTime() const;
    /** @brief Get raw (unscaled) delta time (seconds). */
    double GetUnscaledDeltaTime() const;
    /** @brief Get smoothed (moving average) unscaled delta time. */
    double GetSmoothedDeltaTime() const;
    /** @brief Get total scaled time since start (seconds). */
    double GetTotalTime() const;
    /** @brief Get total unscaled time since start (seconds). */
    double GetRealTimeSinceStart() const;
    /** @brief Get number of frames advanced since start. */
    int    GetFrameCount() const;
    /** @} */

    /**
     * @name Delta clamping
     */
    /** @{ */
    /** @brief Set the maximum allowed unscaled delta time (seconds).
     *  @param seconds Maximum allowed delta; values larger than this will be clamped.
     */
    void SetMaximumDeltaTime(double seconds);
    /** @brief Get the currently configured maximum unscaled delta time (seconds). */
    double GetMaximumDeltaTime() const;
    /** @} */

    /**
     * @name Fixed timestep controls
     */
    /** @{ */
    /** @brief Set fixed timestep duration in seconds. */
    void SetFixedTimeStep(double seconds);
    /** @brief Get fixed timestep duration in seconds. */
    double GetFixedTimeStep() const;
    /** @brief Set maximum fixed steps allowed per frame to avoid spiral of death. */
    void SetMaxFixedStepsPerFrame(int maxSteps);
    /** @} */

    /**
     * @name Smoothing (moving average)
     */
    /** @{ */
    /** @brief Set smoothing window size (number of frames). */
    void SetSmoothingWindowSize(size_t frames);
    /** @brief Get current smoothing window size. */
    size_t GetSmoothingWindowSize() const;
    /** @} */

    /**
     * @name Profiler sampling
     */
    /** @{ */
    /** @brief Set profiler collector callback invoked after Advance(). */
    void SetProfilerCollector(ProfilerCollector cb);
    /** @brief Begin a profiler sample on the current thread (very cheap). */
    void ProfileBegin(const char* name);
    /** @brief End the most recent profiler sample on the current thread. */
    void ProfileEnd();
    /** @} */

    /**
     * @name Lifecycle
     */
    /** @{ */
    /** @brief Start the time system (resets counters). */
    void Start();
    /** @brief Stop the time system. */
    void Stop();
    /** @brief Returns true if the system has been started. */
    bool IsRunning() const;
    /** @} */

private:
    TimeSystem();
    ~TimeSystem();

    // internal state
    std::atomic<double> m_timeScale{1.0};
    double m_unscaledDeltaTime = 0.0;
    double m_deltaTime = 0.0;
    double m_smoothDeltaTime = 0.0;
    double m_totalTimeUnscaled = 0.0;
    double m_totalTimeScaled = 0.0;
    std::deque<double> m_smoothWindow;
    size_t m_smoothWindowSize = 10;

    double m_fixedTimeStep = 1.0 / 60.0;
    double m_fixedAccumulator = 0.0;
    int m_maxFixedSteps = 5;

    // Maximum allowed unscaled delta (seconds). 0 = disabled.
    double m_maximumDeltaTime = 0.4;

    std::atomic<int> m_frameCount{0};

    // profiler collector + thread-local buffers
    ProfilerCollector m_profilerCollector;
    struct ThreadSamples {
      std::vector<ProfileSample> Samples; 
      std::vector<std::pair<const char*, double>> Stack; 
    };
    // map thread id -> samples (protected by mutex when collecting)
    std::mutex m_profilerMutex;
    std::unordered_map<std::thread::id, std::unique_ptr<ThreadSamples>> m_threadSamples;

    // timebase
    double m_startTimeSeconds = 0.0;
    bool m_running = false;

    // helpers
    double _platformNowSeconds() const;
    ThreadSamples* _getOrCreateThreadSamples();
    void _collectThreadSamples();
};

#endif
