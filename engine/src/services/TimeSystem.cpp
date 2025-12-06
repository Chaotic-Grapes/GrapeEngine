/* Start Header *****************************************************************/
/*!
\file   TimeSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   14th September 2025
\brief
Implements the Time service which provides time-related functionality
within the engine. Adds time scaling, fixed timestep, smoothing, and a simple
profiler sampling API.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "services/TimeSystem.h"
#include <algorithm>
#include <cstring>

// Simple singleton instance
TimeSystem& TimeSystem::Instance() {
    static TimeSystem instance;
    return instance;
}

// This a service consumed by many subsystems (rendering, physics, scripting)
// Exposing a simple global singleton keeps call sites concise
// (TimeSystem::Instance()) and avoids passing a time object through
// many interfaces. Internally atomics and a mutex are used only where
// necessary (profiler aggregation)

// ==================== Ctor/Dtor ====================

// Initialize the steady-clock timebase immediately so the
// first Advance() has a consistent reference. We use steady_clock to avoid
// problems with system clock adjustments.
TimeSystem::TimeSystem() { m_startTimeSeconds = _platformNowSeconds(); }
TimeSystem::~TimeSystem() { }

// ==================== Public API ====================

void TimeSystem::Start() {
    // Start/Reset the internal counters. This is used at engine startup
    // or when the time system needs to be explicitly restarted for tests
    // or editor tooling.
    m_startTimeSeconds = _platformNowSeconds();
    m_totalTimeScaled = 0.0;
    m_totalTimeUnscaled = 0.0;
    m_frameCount.store(0);
    m_running = true;
}

void TimeSystem::Stop() { m_running = false; }

bool TimeSystem::IsRunning() const { return m_running; }

void TimeSystem::SetTimeScale(double s) { m_timeScale.store(s); }
double TimeSystem::GetTimeScale() const { return m_timeScale.load(); }

void TimeSystem::SetFixedTimeStep(double seconds) { m_fixedTimeStep = seconds; }
double TimeSystem::GetFixedTimeStep() const { return m_fixedTimeStep; }
void TimeSystem::SetMaxFixedStepsPerFrame(int maxSteps) { m_maxFixedSteps = std::max(1, maxSteps); }

void TimeSystem::SetSmoothingWindowSize(size_t frames) { m_smoothWindowSize = std::max<size_t>(1, frames); }
size_t TimeSystem::GetSmoothingWindowSize() const { return m_smoothWindowSize; }

void TimeSystem::Advance(double rawDeltaSeconds, double nowSeconds) {
    if (!m_running)
        Start();

    // Clamp raw delta to the configured maximum to avoid huge timesteps
    // (e.g., editor pause/stepping, debugger break, or slow resume).
    double clampedDelta = rawDeltaSeconds;
    if (m_maximumDeltaTime > 0.0 && clampedDelta > m_maximumDeltaTime) {
        clampedDelta = m_maximumDeltaTime;
    }

    // The unscaled frame time provided by the platform loop (clamped).
    // We keep both the raw value and a smoothed moving-average to allow
    // subsystems to opt-in to either behavior.
    m_unscaledDeltaTime = clampedDelta;

    // Maintain a simple moving average of the last N frames.
    // This helps dampen short spikes in frame time which could otherwise
    // adversely affect gameplay logic that consumes delta directly.
    m_smoothWindow.push_back(m_unscaledDeltaTime);
    while (m_smoothWindow.size() > m_smoothWindowSize)
        m_smoothWindow.pop_front();

    double sum = 0.0;
    for (double v : m_smoothWindow)
        sum += v;
    m_smoothDeltaTime = sum / static_cast<double>(m_smoothWindow.size());

    // Apply global time scaling (slow motion / pause control).
    double scale = m_timeScale.load();
    m_deltaTime = m_unscaledDeltaTime * scale;

    // Totals for diagnostics / scripting
    m_totalTimeUnscaled += m_unscaledDeltaTime;
    m_totalTimeScaled += m_deltaTime;

    // Accumulate raw time and consume fixed steps as needed by
    // deterministic simulation systems. We cap the number of steps
    // per Advance() to avoid spending excessive CPU when a large
    // delta arrives
    m_fixedAccumulator += m_unscaledDeltaTime;
    int steps = 0;
    while (m_fixedAccumulator >= m_fixedTimeStep && steps < m_maxFixedSteps) {
        m_fixedAccumulator -= m_fixedTimeStep;
        ++steps;
    }

    // Frame counter (atomic for cheap cross-thread reads).
    m_frameCount.fetch_add(1);

    // Aggregate and dispatch profiler samples collected on threads during
    // this frame. Keeping collection centralized minimizes overhead in the
    // sampling hot path.
    _collectThreadSamples();
}

void TimeSystem::SetMaximumDeltaTime(double seconds) {
    // Negative or zero => disabled (no clamping)
    if (seconds <= 0.0) 
        seconds = 0.0;
        
    m_maximumDeltaTime = seconds;
}

double TimeSystem::GetMaximumDeltaTime() const {
    return m_maximumDeltaTime;
}

void TimeSystem::SetProfilerCollector(ProfilerCollector cb) {
    // The collector may be installed or replaced at runtime. Guard the
    // writer with a mutex to avoid races with _collectThreadSamples().
    std::lock_guard<std::mutex> lk(m_profilerMutex);
    m_profilerCollector = std::move(cb);
}

void TimeSystem::ProfileBegin(const char* name) {
    // Push a cheap sampling marker into the current thread's stack. We
    // record a pointer to the name (callers should ensure its lifetime).
    auto* ts = _getOrCreateThreadSamples();
    ts->Stack.emplace_back(name, _platformNowSeconds());
}

void TimeSystem::ProfileEnd() {
    double now = _platformNowSeconds();
    auto* ts = _getOrCreateThreadSamples();

    if (ts->Stack.empty())
        return;
    
    auto ent = ts->Stack.back();
    ts->Stack.pop_back();

    ProfileSample s;
    std::strncpy(s.Name, ent.first, sizeof(s.Name)-1);
    s.Name[sizeof(s.Name)-1] = '\0';
    s.DurationSeconds = now - ent.second;

    ts->Samples.push_back(s);
}

// ==================== Helpers ====================

double TimeSystem::_platformNowSeconds() const {
    // Use steady_clock for monotonic timestamps and convert to seconds.
    auto tp = Clock::now().time_since_epoch();
    return std::chrono::duration_cast<Duration>(tp).count();
}

TimeSystem::ThreadSamples* TimeSystem::_getOrCreateThreadSamples() {
    auto id = std::this_thread::get_id();
    // Look up or create a per-thread sample buffer. The mutex protects
    // the map only; per-thread vectors are used without extra locking
    // to keep sampling cheap.
    std::lock_guard<std::mutex> lk(m_profilerMutex);
    auto it = m_threadSamples.find(id);

    if (it != m_threadSamples.end())
        return it->second.get();

    auto ptr = std::make_unique<ThreadSamples>();
    ThreadSamples* raw = ptr.get();
    m_threadSamples[id] = std::move(ptr);

    return raw;
}

void TimeSystem::_collectThreadSamples() {
    if (!m_profilerCollector)
        return;

    std::vector<ProfileSample> all;
    std::lock_guard<std::mutex> lk(m_profilerMutex);

    for (auto& kv : m_threadSamples) {
        auto* ts = kv.second.get();

        for (auto& s : ts->Samples)
            all.push_back(s);

        ts->Samples.clear();
    }

    if (!all.empty())
        m_profilerCollector(all);
}

// ==================== Getters ====================

double TimeSystem::GetDeltaTime() const { return m_deltaTime; }
double TimeSystem::GetUnscaledDeltaTime() const { return m_unscaledDeltaTime; }
double TimeSystem::GetSmoothedDeltaTime() const { return m_smoothDeltaTime; }
double TimeSystem::GetTotalTime() const { return m_totalTimeScaled; }
double TimeSystem::GetRealTimeSinceStart() const { return m_totalTimeUnscaled; }
int    TimeSystem::GetFrameCount() const { return static_cast<int>(m_frameCount.load()); }
