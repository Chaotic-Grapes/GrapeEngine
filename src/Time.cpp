#include "systems/Time.h"
#include <algorithm>
#include <string>
#include <GLFW/glfw3.h>

// ********** Tracker variables ********** //
double Time::m_startTime;
double Time::m_lastFrameTime;

// ************** Variables ************** //
float  Time::m_deltaTime          = 0.f;
float  Time::m_fixedDeltaTime     = 1.f / 60.f;
double Time::m_timeElapsed        = 0.f;
float  Time::m_timeScale          = 1.f;
int    Time::m_frameCounter       = 0;
float  Time::m_maximumDeltaTime   = 0.4f;

void Time::OnCreate() {
    m_lastFrameTime = m_startTime = glfwGetTime();
}

void Time::OnUpdate() {
    // Frame time
    const auto& now = glfwGetTime();
    const auto& dt = now - m_lastFrameTime;
    m_lastFrameTime = now;
    m_deltaTime = std::min(static_cast<float>(dt), m_maximumDeltaTime);
    
    // Total time since start
    m_timeElapsed = now - m_startTime;

    // Frame count
    ++m_frameCounter;
}

void Time::TimeScale(const float& scale) {
    m_timeScale = std::max(scale, 0.f);
}

void Time::MaximumDeltaTime(const float& maxDelta) {
    m_maximumDeltaTime = std::max(maxDelta, 0.f);
}

std::string Time::Name() const           { return "Time"; }
float  Time::DeltaTime()                 { return m_deltaTime * m_timeScale; }
float  Time::UnscaledDeltaTime()         { return m_deltaTime; }
float  Time::FixedDeltaTime()            { return m_fixedDeltaTime * m_timeScale; }
float  Time::UnscaledFixedDeltaTime()    { return m_fixedDeltaTime; }
double Time::ElapsedTime()               { return m_timeElapsed; }
float  Time::TimeScale()                 { return m_timeScale; }
int    Time::FrameCount()                { return m_frameCounter; }
float  Time::MaximumDeltaTime()          { return m_maximumDeltaTime; }
