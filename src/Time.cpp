#include "Time.h"

std::chrono::high_resolution_clock::time_point Time::m_startTime;
std::chrono::high_resolution_clock::time_point Time::m_lastFrameTime;

float Time::m_deltaTime           = 0.f;
float Time::m_fixedDeltaTime      = 1.f / 60.f;
float Time::m_timeElapsed  = 0.f;
float Time::m_timeScale  = 1.f;
int   Time::m_frameCounter = 0;
float Time::m_maximumDeltaTime    = 0.4f;

void Time::Initialize() {
    m_startTime = std::chrono::high_resolution_clock::now();
    m_lastFrameTime = std::chrono::high_resolution_clock::now();
}

void Time::Update() {
    // Frame time
    const auto& now = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<float>& frameTime = now - m_lastFrameTime;
    m_lastFrameTime = now;
    m_deltaTime = std::min(frameTime.count(), m_maximumDeltaTime);
    
    // Total time since start
    m_timeElapsed = std::chrono::duration<float>(now - m_startTime).count();

    // Frame count
    ++m_frameCounter;
}

void Time::TimeScale(const float& scale) {
    m_timeScale = std::max(scale, 0.f);
}

void Time::MaximumDeltaTime(const float& maxDelta) {
    m_maximumDeltaTime = std::max(maxDelta, 0.f);
}

std::string Time::Name() const          { return "Time"; }
float Time::DeltaTime()                 { return m_deltaTime * m_timeScale; }
float Time::UnscaledDeltaTime()         { return m_deltaTime; }
float Time::FixedDeltaTime()            { return m_fixedDeltaTime * m_timeScale; }
float Time::UnscaledFixedDeltaTime()    { return m_fixedDeltaTime; }
float Time::ElapsedTime()               { return m_timeElapsed; }
float Time::TimeScale()                 { return m_timeScale; }
int   Time::FrameCount()                { return m_frameCounter; }
float Time::MaximumDeltaTime()          { return m_maximumDeltaTime; }
