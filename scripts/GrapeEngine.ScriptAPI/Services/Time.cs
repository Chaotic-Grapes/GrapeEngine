/* Start Header *****************************************************************/
/*!
\file   Time.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   27th October 2025
\brief
Provides access to time-related information in the game engine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.ScriptAPI.Unsafe;

namespace GrapeEngine.Scripting;

/// <summary>
/// Provides access to time-related information in the game engine.
/// </summary>
public static class Time
{
    // ============================================================================
    // Read-Only Properties
    // ============================================================================

    /// <summary>
    /// The time in seconds it took to complete the last frame (affected by TimeScale).
    /// </summary>
    public static float DeltaTime => TimeAPI.GetDeltaTime();

    /// <summary>
    /// The unscaled time in seconds it took to complete the last frame.
    /// </summary>
    public static float UnscaledDeltaTime => TimeAPI.GetUnscaledDeltaTime();

    /// <summary>
    /// The interval in seconds at which physics is calculated (affected by TimeScale).
    /// </summary>
    public static float FixedDeltaTime => TimeAPI.GetFixedDeltaTime();

    /// <summary>
    /// The unscaled interval in seconds at which physics is calculated.
    /// </summary>
    public static float UnscaledFixedDeltaTime => TimeAPI.GetUnscaledFixedDeltaTime();

    /// <summary>
    /// The total time in seconds since the engine started.
    /// </summary>
    public static double ElapsedTime => TimeAPI.GetElapsedTime();

    /// <summary>
    /// The total number of frames that have passed since the engine started.
    /// </summary>
    public static int FrameCount => TimeAPI.GetFrameCount();

    // ============================================================================
    // Read/Write Properties
    // ============================================================================

    /// <summary>
    /// The scale at which time passes.
    /// </summary>
    public static float TimeScale
    {
        get => TimeAPI.GetTimeScale();
        set => TimeAPI.SetTimeScale(value);
    }

    /// <summary>
    /// The maximum time (in seconds) a frame can take.
    /// Default is 0.4 seconds.
    /// </summary>
    /// <remarks> 
    /// This prevents huge time spikes when the game hitches or debugger pauses execution.
    /// </remarks>
    public static float MaximumDeltaTime
    {
        get => TimeAPI.GetMaximumDeltaTime();
        set => TimeAPI.SetMaximumDeltaTime(value);
    }
}
