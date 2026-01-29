/* Start Header *****************************************************************/
/*!
\file   StringAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for native string interning and resolution.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for StringId interop.
/// </summary>
internal static partial class StringAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "StringInterop_Intern", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static partial uint Intern(string value);

    [LibraryImport("GrapeEngineNative", EntryPoint = "StringInterop_Resolve")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static partial nint Resolve(uint id);

    [LibraryImport("GrapeEngineNative", EntryPoint = "StringInterop_FreeString")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static partial void FreeString(nint ptr);
}
