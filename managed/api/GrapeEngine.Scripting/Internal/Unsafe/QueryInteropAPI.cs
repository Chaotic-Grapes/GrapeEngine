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

namespace GrapeEngine.Scripting.Internal.Unsafe;

/// <summary>
/// Query iterator structure matching C++ QueryIterator
/// 
/// SAFETY NOTES:
/// - The 'Archetypes' pointer is only valid during the lifetime of the query iteration
/// - Do NOT store QueryIterator instances across World structural changes (entity/archetype creation)
/// - Do NOT iterate over the same world in nested loops (would invalidate the outer iterator)
/// - Iterator should always be used in a contained scope (foreach, while loop, etc)
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
    public uint OptionalCount;
    public fixed uint OptionalTypeIds[8];
    public uint ExcludeCount;
    public fixed uint ExcludeTypeIds[8];
}

/// <summary>
/// Internal P/Invoke declarations for Query operations.
/// 
/// THREAD SAFETY:
/// All Query operations must be called from the main thread.
/// The C++ side does not provide thread synchronization for query iteration.
/// </summary>
internal static partial class QueryInteropAPI
{
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_CreateQuery")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool CreateQuery(void* worldPtr, uint* componentHashes, int componentCount, uint* optionalHashes, int optionalCount, uint* excludeHashes, int excludeCount, QueryIterator* outIterator);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_QueryNext")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool QueryNext(QueryIterator* iterator, ulong* outEntityId);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_QueryGetComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* QueryGetComponent(QueryIterator* iterator, int componentIndex);

    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_QueryGetOptionalComponent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void* QueryGetOptionalComponent(QueryIterator* iterator, uint componentTypeHash);
}


