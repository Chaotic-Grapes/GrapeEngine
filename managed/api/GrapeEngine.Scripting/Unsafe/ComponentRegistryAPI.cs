/* Start Header *****************************************************************/
/*!
\file   ComponentRegistryAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for component registration operations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for component registration.
/// </summary>
internal static partial class ComponentRegistryAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_RegisterComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static partial bool RegisterComponent(uint typeNameHash, int size, int alignment);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_IsComponentRegistered")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static partial bool IsComponentRegistered(uint typeNameHash);
}
