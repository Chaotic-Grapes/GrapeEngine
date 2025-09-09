#include "Time.h"

std::chrono::high_resolution_clock::time_point Time::StartTime;
std::chrono::high_resolution_clock::time_point Time::LastFrameTime;

float Time::Dt = 0.f;
float Time::FixedDt = 1.0f / 60.0f;
float Time::TimeElapsed = 0.f;

void Time::Initialize() {
    StartTime = std::chrono::high_resolution_clock::now();
    LastFrameTime = std::chrono::high_resolution_clock::now();
}

void Time::Update() {
    // Frame time
    const auto& now = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<float, std::milli>& frameTime = now - LastFrameTime;
    LastFrameTime = now;
    Dt = frameTime.count() / 1000.f;
    
    // Total time since start
    TimeElapsed = std::chrono::duration<float, std::milli>(now - StartTime).count();
}

float Time::DeltaTime()         { return Dt; }
float Time::FixedDeltaTime()    { return FixedDt; }
float Time::ElapsedTime()       { return TimeElapsed; }
