#include "Time.h"

std::chrono::high_resolution_clock::time_point Time::StartTime;
std::chrono::high_resolution_clock::time_point Time::LastFrameTime;

float Time::Dt           = 0.f;
float Time::FixedDt      = 1.f / 60.f;
float Time::TimeElapsed  = 0.f;
float Time::TimeScaling  = 1.f;
int   Time::FrameCounter = 0;
float Time::MaximumDt    = 0.4f;

void Time::Initialize() {
    StartTime = std::chrono::high_resolution_clock::now();
    LastFrameTime = std::chrono::high_resolution_clock::now();
}

void Time::Update() {
    // Frame time
    const auto& now = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<float>& frameTime = now - LastFrameTime;
    LastFrameTime = now;
    Dt = std::max(frameTime.count(), MaximumDt);
    
    // Total time since start
    TimeElapsed = std::chrono::duration<float>(now - StartTime).count();

    // Frame count
    ++FrameCounter;
}

void Time::TimeScale(const float& scale) {
    TimeScaling = std::max(scale, 0.f);
}

void Time::MaximumDeltaTime(const float& maxDelta) {
    MaximumDt = std::max(maxDelta, 0.f);
}

std::string Time::Name() const          { return "Time"; }
float Time::DeltaTime()                 { return Dt * TimeScaling; }
float Time::UnscaledDeltaTime()         { return Dt; }
float Time::FixedDeltaTime()            { return FixedDt * TimeScaling; }
float Time::UnscaledFixedDeltaTime()    { return FixedDt; }
float Time::ElapsedTime()               { return TimeElapsed; }
float Time::TimeScale()                 { return TimeScaling; }
int   Time::FrameCount()                { return FrameCounter; }
float Time::MaximumDeltaTime()          { return MaximumDt; }
