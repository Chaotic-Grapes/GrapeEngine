#ifndef TIME_H
#define TIME_H

#include "ISystem.h"

class Time final : public Engine::ISystem {
public:
    /// Call this function before any other methods of the Time class
    void OnCreate() override;

    /// This will update all time variables that need modifications
    /// To be called ONLY by the Engine's OnUpdate() loop
    void OnUpdate() override;

    /// Debug name
    std::string Name() const override;

    static float  DeltaTime();
    static float  UnscaledDeltaTime();
    static float  FixedDeltaTime();
    static float  UnscaledFixedDeltaTime();
    static double ElapsedTime();
    static void   TimeScale(const float& scale);
    static float  TimeScale();
    static int    FrameCount();
    static void   MaximumDeltaTime(const float& maxDelta);
    static float  MaximumDeltaTime();
    
private:
    static double m_startTime;
    static double m_lastFrameTime;
    
    static float  m_deltaTime;
    static float  m_fixedDeltaTime;
    static double m_timeElapsed;
    static float  m_timeScale;
    static int    m_frameCounter;
    static float  m_maximumDeltaTime;
};
#endif // TIME_H
