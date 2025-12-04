/* Start Header *****************************************************************/
/*!
\file   Time.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   14th September 2025
\brief
Time management utilities for the engine. Provides functions to get delta time,
elapsed time, time scaling, and frame counting.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef TIME_H
#define TIME_H

namespace Engine { class Application; }
class Time final {
public:
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

    // TODO: Move to Application
    static int    m_fpsCap; // 0 = uncapped
};

#endif
