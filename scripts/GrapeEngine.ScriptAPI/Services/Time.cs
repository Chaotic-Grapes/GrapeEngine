using System.Runtime.InteropServices;

namespace GrapeEngine
{
    /// <summary>
    /// Provides access to time-related information in the game engine.
    /// </summary>
    public static class Time
    {
        private const string NativeLib = "GrapeEngine.exe";

        // ============================================================================
        // Read-Only Properties
        // ============================================================================

        /// <summary>
        /// The time in seconds it took to complete the last frame (affected by TimeScale).
        /// </summary>
        public static float DeltaTime => GetDeltaTime();

        /// <summary>
        /// The unscaled time in seconds it took to complete the last frame.
        /// </summary>
        public static float UnscaledDeltaTime => GetUnscaledDeltaTime();

        /// <summary>
        /// The interval in seconds at which physics is calculated (affected by TimeScale).
        /// </summary>
        public static float FixedDeltaTime => GetFixedDeltaTime();

        /// <summary>
        /// The unscaled interval in seconds at which physics is calculated.
        /// </summary>
        public static float UnscaledFixedDeltaTime => GetUnscaledFixedDeltaTime();

        /// <summary>
        /// The total time in seconds since the engine started.
        /// </summary>
        public static double ElapsedTime => GetElapsedTime();

        /// <summary>
        /// The total number of frames that have passed since the engine started.
        /// </summary>
        public static int FrameCount => GetFrameCount();

        // ============================================================================
        // Read/Write Properties
        // ============================================================================

        /// <summary>
        /// The scale at which time passes.
        /// </summary>
        public static float TimeScale
        {
            get => GetTimeScale();
            set => SetTimeScale(value);
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
            get => GetMaximumDeltaTime();
            set => SetMaximumDeltaTime(value);
        }
        
        // ============================================================================
        // P/Invoke Declarations | Unsafe
        // ============================================================================

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_GetDeltaTime", CallingConvention = CallingConvention.Cdecl)]
        private static extern float GetDeltaTime();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_GetUnscaledDeltaTime", CallingConvention = CallingConvention.Cdecl)]
        private static extern float GetUnscaledDeltaTime();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_GetFixedDeltaTime", CallingConvention = CallingConvention.Cdecl)]
        private static extern float GetFixedDeltaTime();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_GetUnscaledFixedDeltaTime", CallingConvention = CallingConvention.Cdecl)]
        private static extern float GetUnscaledFixedDeltaTime();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_GetElapsedTime", CallingConvention = CallingConvention.Cdecl)]
        private static extern double GetElapsedTime();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_GetFrameCount", CallingConvention = CallingConvention.Cdecl)]
        private static extern int GetFrameCount();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_GetTimeScale", CallingConvention = CallingConvention.Cdecl)]
        private static extern float GetTimeScale();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_SetTimeScale", CallingConvention = CallingConvention.Cdecl)]
        private static extern void SetTimeScale(float scale);

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_GetMaximumDeltaTime", CallingConvention = CallingConvention.Cdecl)]
        private static extern float GetMaximumDeltaTime();

        [DllImport(NativeLib, EntryPoint = "ScriptAPI_Time_SetMaximumDeltaTime", CallingConvention = CallingConvention.Cdecl)]
        private static extern void SetMaximumDeltaTime(float maxDelta);
    }
}
