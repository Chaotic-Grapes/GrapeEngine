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
    static std::chrono::high_resolution_clock::time_point m_startTime;
    static std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    
    static float m_deltaTime;
    static float m_fixedDeltaTime;
    static float m_timeElapsed;
    static float m_timeScale;
    static int   m_frameCounter;
    static float m_maximumDeltaTime;
};
#endif // TIME_H
