#include "Time.h"

std::chrono::high_resolution_clock::time_point Time::StartTime;
std::chrono::high_resolution_clock::time_point Time::LastFrameTime;

float Time::Dt = 0.f;
float Time::FixedDt = 1.0f / 60.0f;
float Time::TimeElapsed = 0.f;
float Time::TimeScaling = 1.f;

void Time::Initialize() {
    StartTime = std::chrono::high_resolution_clock::now();
    LastFrameTime = std::chrono::high_resolution_clock::now();
}

void Time::Update() {
    // Frame time
    const auto& now = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<float>& frameTime = now - LastFrameTime;
    LastFrameTime = now;
    Dt = frameTime.count();
    
    // Total time since start
    TimeElapsed = std::chrono::duration<float>(now - StartTime).count();
}

void Time::TimeScale(const float& scale) {
    TimeScaling = std::max(scale, 0.f);
}

std::string Time::Name() const  { return "Time"; }
float Time::DeltaTime()         { return Dt; }
float Time::FixedDeltaTime()    { return FixedDt; }
float Time::ElapsedTime()       { return TimeElapsed; }
float Time::TimeScale()         { return TimeScaling; }
