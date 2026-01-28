/* Start Header *****************************************************************/
/*!
\file   ApplicationAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   21st November 2025
\brief
P/Invoke declarations for the Application API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

/// <summary>
/// Internal use only. P/Invoke declarations for the Application API.
/// </summary>
internal partial class ApplicationAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Application_Quit")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial void Quit();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Application_GetName", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial string GetName();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Application_GetFixedTimeStep")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    internal static partial float GetFixedTimeStep();

    [LibraryImport("GrapeEngineNative", EntryPoint = "EngineInterop_Application_IsVSyncEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool IsVSyncEnabled();
}


