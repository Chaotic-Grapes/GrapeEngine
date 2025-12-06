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

/// <summary>
/// Get the scaled delta time (time since last frame, affected by time scale)
/// </summary>
INTEROP_API float EngineInterop_Time_GetDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetDeltaTime());
}

/// <summary>
/// Get the unscaled delta time (time since last frame, NOT affected by time scale)
/// </summary>
INTEROP_API float EngineInterop_Time_GetUnscaledDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetUnscaledDeltaTime());
}

/// <summary>
/// Get the scaled fixed delta time (physics timestep, affected by time scale)
/// </summary>
INTEROP_API float EngineInterop_Time_GetFixedDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetFixedTimeStep() * TimeSystem::Instance().GetTimeScale());
}

/// <summary>
/// Get the unscaled fixed delta time (physics timestep, NOT affected by time scale)
/// </summary>
INTEROP_API float EngineInterop_Time_GetUnscaledFixedDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetFixedTimeStep());
}

/// <summary>
/// Get the total elapsed time since engine start
/// </summary>
INTEROP_API double EngineInterop_Time_GetElapsedTime() {
    return TimeSystem::Instance().GetTotalTime();
}

/// <summary>
/// Get the current frame count
/// </summary>
INTEROP_API int EngineInterop_Time_GetFrameCount() {
    return TimeSystem::Instance().GetFrameCount();
}

// ============================================================================
// Time API - Read/Write Properties
// ============================================================================

/// <summary>
/// Get the current time scale multiplier
/// </summary>
INTEROP_API float EngineInterop_Time_GetTimeScale() {
    return static_cast<float>(TimeSystem::Instance().GetTimeScale());
}

/// <summary>
/// Set the time scale multiplier (0.0 = paused, 1.0 = normal, 2.0 = double speed)
/// </summary>
INTEROP_API void EngineInterop_Time_SetTimeScale(float scale) {
    TimeSystem::Instance().SetTimeScale(scale);
}

/// <summary>
/// Get the maximum allowed delta time (prevents large spikes)
/// </summary>
INTEROP_API float EngineInterop_Time_GetMaximumDeltaTime() {
    return static_cast<float>(TimeSystem::Instance().GetMaximumDeltaTime());
}

/// <summary>
/// Set the maximum allowed delta time
/// </summary>
INTEROP_API void EngineInterop_Time_SetMaximumDeltaTime(float maxDelta) {
    TimeSystem::Instance().SetMaximumDeltaTime(static_cast<double>(maxDelta));
}

// TODO: Set FPS cap somewhere else like in Application rather than Time.
// This includes the C# binding as well.
// So for now, do not expose the FPS cap API to C#. (don't bind them)
