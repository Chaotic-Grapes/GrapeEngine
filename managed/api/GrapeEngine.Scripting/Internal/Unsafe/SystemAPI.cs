/* Start Header *****************************************************************/
/*!
\file   SystemAPI.cs
\brief  P/Invoke declarations for system runtime control APIs.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

internal static partial class SystemAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_System_SetEnabled", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool SetEnabled(string systemName, [MarshalAs(UnmanagedType.I1)] bool enabled);

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_System_IsEnabled", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsEnabled(string systemName);
}

