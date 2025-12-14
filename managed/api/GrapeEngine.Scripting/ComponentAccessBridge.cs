/* Start Header *****************************************************************/
/*!
\file   ComponentAccessBridge.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Bridge for converting C# component access attributes to C++ format.

Enables C# systems to participate in the C++ dependency resolution system
by converting their attribute-based declarations to the C++ metadata format.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Hosting;

namespace GrapeEngine.Scripting;

/// <summary>
/// Bridge for converting C# component access declarations to C++ format.
/// 
/// Enables C# systems to declare component dependencies using attributes,
/// which are then converted to the C++ ComponentAccessMode representation
/// for use in the dependency resolution system.
/// 
/// Example:
/// <code>
/// [ReadOnly<Velocity>]
/// [WriteAccess<Transform>]
/// public class MySystem : ISystem
/// {
///     // Component access is automatically extracted and reported to C++
/// }
/// 
/// // Internally, this is converted to:
/// // ReadComponents: { ComponentTypeId(Velocity) }
/// // WriteComponents: { ComponentTypeId(Transform) }
/// </code>
/// </summary>
public static class ComponentAccessBridge
{
    /// <summary>
    /// Extract and validate C# component access attributes from a system type.
    /// 
    /// Reads all [ReadOnly<T>] and [WriteAccess<T>] attributes and converts
    /// them to the format expected by the C++ dependency system.
    /// </summary>
    /// <param name="systemType">The system type to analyze</param>
    /// <returns>List of component accesses with their modes</returns>
    public static List<(uint ComponentHash, ComponentAccessMode Mode)> ExtractComponentAccesses(Type systemType)
    {
        var accesses = new List<(uint, ComponentAccessMode)>();

        foreach (var (componentType, mode) in SystemMetadataExtractor.GetComponentAccesses(systemType))
        {
            var hash = GetComponentTypeHash(componentType);
            accesses.Add((hash, mode));
        }

        return accesses;
    }

    /// <summary>
    /// Get the read-only component hashes for a system type.
    /// </summary>
    public static List<uint> ExtractReadComponentHashes(Type systemType)
    {
        var hashes = new List<uint>();

        foreach (var compType in SystemMetadataExtractor.GetReadOnlyComponents(systemType))
        {
            hashes.Add(GetComponentTypeHash(compType));
        }

        return hashes;
    }

    /// <summary>
    /// Get the write-access component hashes for a system type.
    /// </summary>
    public static List<uint> ExtractWriteComponentHashes(Type systemType)
    {
        var hashes = new List<uint>();

        foreach (var compType in SystemMetadataExtractor.GetWriteAccessComponents(systemType))
        {
            hashes.Add(GetComponentTypeHash(compType));
        }

        return hashes;
    }

    /// <summary>
    /// Get the FNV-1a hash for a component type.
    /// Must match the C++ hashing algorithm for proper dependency detection.
    /// </summary>
    /// <param name="componentType">Component type to hash</param>
    /// <returns>FNV-1a 32-bit hash of the component's full name</returns>
    public static uint GetComponentTypeHash(Type componentType)
    {
        var name = componentType.FullName ?? componentType.Name;
        return Fnv1aHash(name);
    }

    /// <summary>
    /// FNV-1a hash algorithm (32-bit) for component type names.
    /// Matches the C++ implementation in ComponentTypeId.
    /// </summary>
    private static uint Fnv1aHash(string input)
    {
        const uint fnvOffset = 0x811c9dc5;  // 32-bit offset basis
        const uint fnvPrime = 0x01000193;   // 32-bit FNV prime

        uint hash = fnvOffset;

        foreach (var c in input)
        {
            hash ^= c;
            hash *= fnvPrime;
        }

        return hash;
    }

    /// <summary>
    /// Validate that component access declarations are consistent.
    /// Ensures no component is declared with conflicting modes.
    /// </summary>
    /// <param name="systemType">System type to validate</param>
    /// <returns>True if declarations are consistent, false otherwise</returns>
    public static bool ValidateComponentAccesses(Type systemType)
    {
        var accesses = ExtractComponentAccesses(systemType);
        var seenComponents = new HashSet<uint>();

        foreach (var (hash, mode) in accesses)
        {
            if (seenComponents.Contains(hash))
            {
                // Component declared multiple times with different modes - invalid
                return false;
            }

            seenComponents.Add(hash);
        }

        return true;
    }

    /// <summary>
    /// Get human-readable description of component accesses for debugging.
    /// </summary>
    public static string DescribeComponentAccesses(Type systemType)
    {
        var reads = SystemMetadataExtractor.GetReadOnlyComponents(systemType).ToList();
        var writes = SystemMetadataExtractor.GetWriteAccessComponents(systemType).ToList();

        var parts = new List<string>();

        if (reads.Any())
        {
            parts.Add($"Reads: {string.Join(", ", reads.Select(t => t.Name))}");
        }

        if (writes.Any())
        {
            parts.Add($"Writes: {string.Join(", ", writes.Select(t => t.Name))}");
        }

        return string.Join("; ", parts) ?? "(no component accesses declared)";
    }

    /// <summary>
    /// Check if two systems have compatible component accesses.
    /// Returns false if they would conflict on any component.
    /// </summary>
    public static bool CanRunInParallel(Type systemTypeA, Type systemTypeB)
    {
        var accessesA = ExtractComponentAccesses(systemTypeA);
        var accessesB = ExtractComponentAccesses(systemTypeB);

        var readHashesA = new HashSet<uint>(accessesA
            .Where(x => x.Mode == ComponentAccessMode.Read)
            .Select(x => x.ComponentHash));

        var readHashesB = new HashSet<uint>(accessesB
            .Where(x => x.Mode == ComponentAccessMode.Read)
            .Select(x => x.ComponentHash));

        var writeHashesA = new HashSet<uint>(accessesA
            .Where(x => x.Mode != ComponentAccessMode.Read)
            .Select(x => x.ComponentHash));

        var writeHashesB = new HashSet<uint>(accessesB
            .Where(x => x.Mode != ComponentAccessMode.Read)
            .Select(x => x.ComponentHash));

        // Write-Write conflict
        if (writeHashesA.Overlaps(writeHashesB))
            return false;

        // Write-Read conflicts
        if (writeHashesA.Overlaps(readHashesB))
            return false;

        if (readHashesA.Overlaps(writeHashesB))
            return false;

        // No conflicts - can run in parallel
        return true;
    }
}
