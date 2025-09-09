#ifndef TIME_H
#define TIME_H

#include <chrono>

// TODO: Timescale?
class Time
{
public:
    static void Initialize();
    static void Update();

    static float DeltaTime();
    static float FixedDeltaTime();
    static float ElapsedTime();
    
private:
    static std::chrono::high_resolution_clock::time_point StartTime;
    static std::chrono::high_resolution_clock::time_point LastFrameTime;
    
    static float Dt;
    static float FixedDt;
    static float TimeElapsed;
};
#endif // TIME_H
