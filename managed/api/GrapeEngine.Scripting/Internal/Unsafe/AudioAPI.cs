/* Start Header *****************************************************************/
/*!
\file   AudioAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
P/Invoke declarations for the Audio API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Audio API.
/// </summary>
internal partial class AudioAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_LoadCue", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool LoadCue(string cueId, string filePath, [MarshalAs(UnmanagedType.I1)] bool is3D, [MarshalAs(UnmanagedType.I1)] bool isLooping, [MarshalAs(UnmanagedType.I1)] bool isStreaming);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_UnloadCue", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void UnloadCue(string cueId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_HasCue", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool HasCue(string cueId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_Play", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ulong Play(string cueId, float volume, float pitch, [MarshalAs(UnmanagedType.I1)] bool paused);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_PlaySingle", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial ulong PlaySingle(string cueId, float volume, float pitch, int policy);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_Stop")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Stop(ulong handleId, int stopMode);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_StopCue", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void StopCue(string cueId, int stopMode);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_IsCuePlaying", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsCuePlaying(string cueId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_SetInstanceVolume")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetInstanceVolume(ulong handleId, float volume);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_SetInstancePitch")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetInstancePitch(ulong handleId, float pitch);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_SetInstancePan")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetInstancePan(ulong handleId, float pan);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_SetInstanceLowPassGain")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetInstanceLowPassGain(ulong handleId, float gain);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_SetInstancePosition")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetInstancePosition(ulong handleId, float posX, float posY, float posZ, float velX, float velY, float velZ);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_SetMasterVolume")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetMasterVolume(float volume);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_GetMasterVolume")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetMasterVolume();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_SetListener")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetListener(float posX, float posY, float posZ, float velX, float velY, float velZ, float fwdX, float fwdY, float fwdZ, float upX, float upY, float upZ);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_SetBusVolume")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void SetBusVolume(int bus, float volume);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_GetBusVolume")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetBusVolume(int bus);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Audio_FadeBusVolume")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void FadeBusVolume(int bus, float targetVolume, float duration);
}


