#ifndef TIME_H
#define TIME_H

#include <chrono>
#include "ISystem.h"

class Time final : public Engine::ISystem
{
public:
    /// Call this function before any other methods of the Time class
    void Initialize() override;

    /// This will update all time variables that need modifications
    /// To be called ONLY by the Engine's Update() loop
    void Update() override;

    /// Debug name
    std::string Name() const override;
    
    static float DeltaTime();
    static float UnscaledDeltaTime();
    static float FixedDeltaTime();
    static float UnscaledFixedDeltaTime();
    static float ElapsedTime();
    static void  TimeScale(const float& scale);
    static float TimeScale();
    static int   FrameCount();
    static void  MaximumDeltaTime(const float& maxDelta);
    static float MaximumDeltaTime();
    
private:
    static std::chrono::high_resolution_clock::time_point StartTime;
    static std::chrono::high_resolution_clock::time_point LastFrameTime;
    
    static float Dt;
    static float FixedDt;
    static float TimeElapsed;
    static float TimeScaling;
    static int   FrameCounter;
    static float MaximumDt;
};
#endif // TIME_H
