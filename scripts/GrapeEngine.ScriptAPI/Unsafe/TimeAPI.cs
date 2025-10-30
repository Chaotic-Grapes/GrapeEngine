using System.Runtime.InteropServices;

namespace GrapeEngine.ScriptAPI.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Time API.
/// </summary>
internal partial class TimeAPI
{
    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_GetDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetDeltaTime();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_GetUnscaledDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetUnscaledDeltaTime();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_GetFixedDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetFixedDeltaTime();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_GetUnscaledFixedDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetUnscaledFixedDeltaTime();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_GetElapsedTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial double GetElapsedTime();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_GetFrameCount")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial int GetFrameCount();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_GetTimeScale")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetTimeScale();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_SetTimeScale")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetTimeScale(float scale);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_GetMaximumDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetMaximumDeltaTime();

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "ScriptAPI_Time_SetMaximumDeltaTime")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetMaximumDeltaTime(float maxDelta);
}
