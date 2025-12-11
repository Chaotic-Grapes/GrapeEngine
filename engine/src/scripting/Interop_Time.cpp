/* Start Header *****************************************************************/
/*!
\file    Interop_Time.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
C API exports for managed C# scripting systems for Time service.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "services/TimeSystem.h"

// ============================================================================
// Time API - Read-Only Properties
// ============================================================================

/**
 * @brief Get the scaled delta time (time since last frame, affected by time scale)
 * @return The scaled delta time in seconds
 */
INTEROP_API float EngineInterop_Time_GetDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetDeltaTime());
}

/**
 * @brief Get the unscaled delta time (time since last frame, NOT affected by time scale)
 * @return The unscaled delta time in seconds
 */
INTEROP_API float EngineInterop_Time_GetUnscaledDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetUnscaledDeltaTime());
}

/**
 * @brief Get the unscaled fixed delta time (physics timestep, NOT affected by time scale)
 * @return The unscaled fixed delta time in seconds
 */
INTEROP_API float EngineInterop_Time_GetFixedDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetFixedTimeStep());
}

/**
 * @brief Get the unscaled fixed delta time (physics timestep, NOT affected by time scale)
 * @return The unscaled fixed delta time in seconds
 */
INTEROP_API float EngineInterop_Time_GetUnscaledFixedDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetFixedTimeStep());
}

/**
 * @brief Get the total elapsed time since engine start
 * @return The total elapsed time in seconds
 */
INTEROP_API double EngineInterop_Time_GetElapsedTime() {
    return TimeSystem::Instance().GetTotalTime();
}

/**
 * @brief Get the current frame count
 * @return The number of frames since engine start
 */
INTEROP_API int EngineInterop_Time_GetFrameCount() {
    return TimeSystem::Instance().GetFrameCount();
}

/**
 * @brief Get the smoothed (moving average) delta time
 * @return The smoothed delta time in seconds
 */
INTEROP_API float EngineInterop_Time_GetSmoothedDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetSmoothedDeltaTime());
}

/**
 * @brief Get the total unscaled time since engine start
 * @return The total unscaled time in seconds
 */
INTEROP_API double EngineInterop_Time_GetRealTimeSinceStart() {
    return TimeSystem::Instance().GetRealTimeSinceStart();
}

// ============================================================================
// Time API - Read/Write Properties
// ============================================================================

/**
 * @brief Get current frames per second
 * @return The current FPS
 */
INTEROP_API float EngineInterop_Time_GetFPS() {
    return TimeSystem::Instance().GetFPS();
}

/**
 * @brief Get frame time in milliseconds
 * @return The frame time in milliseconds
 */
INTEROP_API float EngineInterop_Time_GetFrameTimeMs() {
    return TimeSystem::Instance().GetFrameTimeMs();
}

/**
 * @brief Get the current time scale multiplier
 * @return The time scale value
 */
INTEROP_API float EngineInterop_Time_GetTimeScale() {
    return static_cast<float>(TimeSystem::Instance().GetTimeScale());
}

/**
 * @brief Set the time scale multiplier
 * @param scale The time scale (0.0 = paused, 1.0 = normal, 2.0 = double speed)
 */
INTEROP_API void EngineInterop_Time_SetTimeScale(float scale) {
    TimeSystem::Instance().SetTimeScale(scale);
}

/**
 * @brief Get the maximum allowed delta time
 * @return The maximum delta time in seconds
 */
INTEROP_API float EngineInterop_Time_GetMaximumDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetMaximumDeltaTime());
}

/**
 * @brief Set the maximum allowed delta time
 * @param maxDelta The maximum delta time in seconds
 */
INTEROP_API void EngineInterop_Time_SetMaximumDeltaTime(float maxDelta) {
    TimeSystem::Instance().SetMaximumDeltaTime(static_cast<double>(maxDelta));
}

/**
 * @brief Set the maximum FPS cap
 * @param fps The maximum FPS (0 = disabled, no cap)
 */
INTEROP_API void EngineInterop_Time_SetMaximumFPS(float fps) {
    TimeSystem::Instance().SetMaximumFPS(static_cast<double>(fps));
}

/**
 * @brief Get the maximum FPS cap
 * @return The maximum FPS cap (0 = disabled)
 */
INTEROP_API float EngineInterop_Time_GetMaximumFPS() {
    return static_cast<float>(TimeSystem::Instance().GetMaximumFPS());
}
