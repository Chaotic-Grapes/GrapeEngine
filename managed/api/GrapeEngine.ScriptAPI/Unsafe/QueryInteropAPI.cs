/* Start Header *****************************************************************/
/*!
\file   QueryInteropAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for Query operations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.ScriptAPI.Unsafe;

/// <summary>
/// Query iterator structure matching C++ QueryIterator
/// </summary>
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct QueryIterator
{
    public void* WorldPtr;
    public void* Archetypes;
    public uint ArchetypeIndex;
    public uint ChunkIndex;
    public uint EntityIndex;
    public uint ComponentCount;
    public fixed uint ComponentTypeIds[8];
}

/// <summary>
/// Internal P/Invoke declarations for Query operations.
/// </summary>
internal static partial class QueryInteropAPI
{
    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_CreateQuery")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool CreateQuery(void* worldPtr, uint* componentHashes, int componentCount, QueryIterator* outIterator);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_QueryNext")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool QueryNext(QueryIterator* iterator, ulong* outEntityId);

    [LibraryImport(UnsafeApiHelper.NativeLib, EntryPoint = "WorldInterop_QueryGetComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* QueryGetComponent(QueryIterator* iterator, int componentIndex);
}
