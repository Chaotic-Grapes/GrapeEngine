#ifndef TIME_H
#define TIME_H

namespace Engine { class Application; }
class Time final {
public:
    Time();

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
    static void   FpsCap(int fps);
    static int    FpsCap();
    
private:
	friend class Engine::Application;

    static void _update(double dt, double now);
    static void _initialize();

    static double m_startTime;
    static float  m_deltaTime;
    static float  m_fixedDeltaTime;
    static double m_timeElapsed;
    static float  m_timeScale;
    static int    m_frameCounter;
    static float  m_maximumDeltaTime;

    static int    m_fpsCap; // 0 = uncapped
};
#endif // TIME_H
