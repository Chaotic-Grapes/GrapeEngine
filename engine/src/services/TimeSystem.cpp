/* Start Header *****************************************************************/
/*!
\file     TimeSystem.cpp
\authors  Muhammad Nur Fadzly Bin Zulkifli (70%)
          Samantha Leong (30%)
\par      muhammadnurfadzly.b@digipen.edu
          s.leong@digipen.edu
\date     14th September 2025
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
#include <numeric>

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

void TimeSystem::Advance(double rawDeltaSeconds, double /*nowSeconds*/) {
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

    // Apply time scale to the clamped frame delta so fixed-step consumers stay stable on hitches.
    double unscaledSeconds = clampedDelta;
    double scaledSeconds = unscaledSeconds * (double)m_timeScale.load();

    m_unscaledDeltaTime = (float)unscaledSeconds;
    m_deltaTime = (float)scaledSeconds;

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
    // Update FPS / frame time stats
    m_frameTimeMs = (float)(unscaledSeconds * 1000.0);
    m_currentFrameFPS = (float)(1.0 / std::max(1e-6, unscaledSeconds));
    m_fpsWindow.push_back(1.0 / std::max(1e-6, unscaledSeconds));

    if (m_fpsWindow.size() > (size_t)m_fpsWindowSize)
        m_fpsWindow.pop_front();

    double fpsSum = std::accumulate(m_fpsWindow.begin(), m_fpsWindow.end(), 0.0);
    m_fps = (float)(fpsSum / m_fpsWindow.size());

    // Collect profiler samples from threads and aggregate
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

void TimeSystem::SetMaximumFPS(double fps) {
    // Negative or zero => disabled (no cap)
    if (fps <= 0.0)
        fps = 0.0;

    m_maximumFPS = fps;
}

double TimeSystem::GetMaximumFPS() const { return m_maximumFPS; }

/**
 * @brief Install or replace the profiler collector callback.
 * @param cb Callback invoked with raw per-scope samples collected in a frame.
 * @return void
 * @note Access is synchronized with sampler collection to avoid data races.
 */
void TimeSystem::SetProfilerCollector(ProfilerCollector cb) {
    // The collector may be installed or replaced at runtime. Guard the
    // writer with a mutex to avoid races with _collectThreadSamples().
    std::lock_guard<std::mutex> lk(m_profilerMutex);
    m_profilerCollector = std::move(cb);
}

void TimeSystem::ProfileBegin(const char* name) {
    // Intern the name (may lock briefly on first-seen names) and push
    // a (scopeId, startTime) into the per-thread stack. This keeps the
    // hot path allocation- and lock-light for repeated scopes.
    uint32_t id = _internScopeName(name);
    auto* ts = _getOrCreateThreadSamples();
    ts->Stack.emplace_back(id, _platformNowSeconds());
}

void TimeSystem::ProfileEnd() {
    double now = _platformNowSeconds();
    auto* ts = _getOrCreateThreadSamples();

    if (ts->Stack.empty())
        return;
    
    auto ent = ts->Stack.back();
    ts->Stack.pop_back();

    ProfileSample s;
    s.ScopeId = ent.first;
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

/**
 * @brief Collect thread-local samples and aggregate them into frame-level scope data.
 * @return void
 * @note Aggregation is per frame: each scope stores total milliseconds and call count
 *       for the latest frame, plus a bounded history of per-frame totals.
 * @complexity O(S + H), where S is number of samples this frame and H is
 *             total retained history touched while recomputing aggregate stats.
 */
void TimeSystem::_collectThreadSamples() {
    // Collect all samples produced on threads this frame.
    std::vector<ProfileSample> all;
    ProfilerCollector collectorCopy;
    {
        std::lock_guard<std::mutex> lk(m_profilerMutex);

        for (auto& kv : m_threadSamples) {
            auto* ts = kv.second.get();

            for (auto& s : ts->Samples)
                all.push_back(s);

            ts->Samples.clear();
        }

        // Snapshot callback under the same lock used by writers.
        collectorCopy = m_profilerCollector;
    }

    // Aggregate samples into per-id ScopeData for UI consumption. We always reset
    // this-frame fields so stale values do not leak into frames with no samples.
    {
        std::lock_guard<std::mutex> lk(m_scopeMutex);

        // Reset per-frame values for all known scopes.
        for (auto& sd : m_scopeDataById) {
            sd.LastTimeMs = 0.0f;
            sd.CallCount = 0;
        }

        // Build frame totals per scope id so repeated calls in one frame are summed.
        struct FrameAggregate {
            float TotalMs = 0.0f;
            uint32_t Calls = 0;
        };
        std::unordered_map<uint32_t, FrameAggregate> frameByScope;
        frameByScope.reserve(all.size());

        for (const auto &s : all) {
            uint32_t id = s.ScopeId;
            float ms = static_cast<float>(s.DurationSeconds * 1000.0);

            auto& agg = frameByScope[id];
            agg.TotalMs += ms;
            ++agg.Calls;
        }

        for (const auto& [id, agg] : frameByScope) {

            if (id >= m_scopeDataById.size()) {
                m_scopeDataById.resize(id + 1);
                // m_scopeNames should already contain a name for this id via interning
            }

            auto &sd = m_scopeDataById[id];
            sd.LastTimeMs = agg.TotalMs;
            sd.CallCount = agg.Calls;

            // Store one value per frame (total scope time this frame).
            sd.FrameTimes.push_back(agg.TotalMs);
            if (sd.FrameTimes.size() > m_scopeHistorySize)
                sd.FrameTimes.erase(sd.FrameTimes.begin());

            // compute average and max
            float sum = 0.0f;
            float maxv = 0.0f;
            for (float v : sd.FrameTimes) {
                sum += v;
                if (v > maxv) maxv = v;
            }
            sd.AverageTimeMs = sd.FrameTimes.empty() ? 0.0f : (sum / sd.FrameTimes.size());
            sd.MaxTimeMs = maxv;
        }
    }

    // If a collector is installed, forward the raw sample vector as well
    if (collectorCopy && !all.empty()) {
        collectorCopy(all);
    }
}

// ==================== Getters ====================

double TimeSystem::GetDeltaTime() const { return m_deltaTime; }
double TimeSystem::GetUnscaledDeltaTime() const { return m_unscaledDeltaTime; }
double TimeSystem::GetSmoothedDeltaTime() const { return m_smoothDeltaTime; }
double TimeSystem::GetTotalTime() const { return m_totalTimeScaled; }
double TimeSystem::GetRealTimeSinceStart() const { return m_totalTimeUnscaled; }
int    TimeSystem::GetFrameCount() const { return static_cast<int>(m_frameCount.load()); }

float TimeSystem::GetFPS() const {
    return m_fps;
}

float TimeSystem::GetFrameTimeMs() const {
    return m_frameTimeMs;
}

/**
 * @brief Return a snapshot of all scope aggregates keyed by scope name.
 * @return Copy of scope data map safe for UI consumption.
 * @complexity O(N), where N is number of interned scopes.
 */
TimeSystem::ScopeDataMap TimeSystem::GetAllScopeData() const {
    ScopeDataMap out;
    std::lock_guard<std::mutex> lk(m_scopeMutex);
    const size_t n = std::min(m_scopeNames.size(), m_scopeDataById.size());
    for (size_t i = 0; i < n; ++i) {
        out[m_scopeNames[i]] = m_scopeDataById[i];
    }
    return out;
}

/**
 * @brief Resolve an interned scope id to its name.
 * @param id Scope id returned by internal name interning.
 * @return Scope name or empty string if id is out of range.
 * @complexity O(1).
 */
std::string TimeSystem::GetScopeName(uint32_t id) const {
    std::lock_guard<std::mutex> lk(m_scopeMutex);
    if (id < m_scopeNames.size()) return m_scopeNames[id];
    return std::string();
}

/**
 * @brief Sum latest per-frame totals across all scopes.
 * @return Total milliseconds attributed to instrumented scopes in latest frame.
 * @complexity O(N), where N is number of interned scopes.
 */
double TimeSystem::GetTotalScopeTimes() const {
    std::lock_guard<std::mutex> lk(m_scopeMutex);
    double total = 0.0;
    const size_t n = std::min(m_scopeNames.size(), m_scopeDataById.size());
    for (size_t i = 0; i < n; ++i) {
        total += m_scopeDataById[i].LastTimeMs;
    }
    return total;
}

void TimeSystem::ClearProfilerHistory() {
    std::lock_guard<std::mutex> lk(m_scopeMutex);
    for (auto &sd : m_scopeDataById) sd.FrameTimes.clear();
}

/**
 * @brief Intern a scope name and return its stable numeric id.
 * @param name Null-terminated scope name.
 * @return Interned id; returns 0 when name is null.
 * @note Uses the scope-data mutex so name/id maps and scope vectors stay coherent.
 * @complexity Average O(1).
 */
uint32_t TimeSystem::_internScopeName(const char* name) {
    if (!name) return 0;
    std::lock_guard<std::mutex> lk(m_scopeMutex);
    auto it = m_scopeNameToId.find(name);
    if (it != m_scopeNameToId.end()) return it->second;
    uint32_t id = static_cast<uint32_t>(m_scopeNames.size());
    m_scopeNames.emplace_back(name);
    m_scopeNameToId.emplace(m_scopeNames.back(), id);
    if (m_scopeDataById.size() <= id) m_scopeDataById.resize(id + 1);
    return id;
}
