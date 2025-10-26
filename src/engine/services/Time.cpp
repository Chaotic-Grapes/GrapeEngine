#include "services/Time.h"
#include <algorithm>
#include <string>
#include <GLFW/glfw3.h>

// ************** Variables ************** //
double Time::m_startTime          = 0.0;
float  Time::m_deltaTime          = 0.f;
float  Time::m_fixedDeltaTime     = 1.f / 60.f;
double Time::m_timeElapsed        = 0.f;
float  Time::m_timeScale          = 1.f;
int    Time::m_frameCounter       = 0;
float  Time::m_maximumDeltaTime   = 0.4f;
int    Time::m_fpsCap             = 0; // 0 = uncapped

void Time::_initialize() {
    m_startTime = glfwGetTime();
}

void Time::_update(const double dt, const double now) {
    // Frame time
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

float  Time::DeltaTime()                 { return m_deltaTime * m_timeScale; }
float  Time::UnscaledDeltaTime()         { return m_deltaTime; }
float  Time::FixedDeltaTime()            { return m_fixedDeltaTime * m_timeScale; }
float  Time::UnscaledFixedDeltaTime()    { return m_fixedDeltaTime; }
double Time::ElapsedTime()               { return m_timeElapsed; }
float  Time::TimeScale()                 { return m_timeScale; }
int    Time::FrameCount()                { return m_frameCounter; }
float  Time::MaximumDeltaTime()          { return m_maximumDeltaTime; }

void   Time::FpsCap(const int fps)       { m_fpsCap = std::max(fps, 0); }
int    Time::FpsCap()                    { return m_fpsCap; }
