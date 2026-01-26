/* Start Header *****************************************************************/
/*!
\file   ComponentTypeRegistry.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\date   26th October 2025
\brief
Component type registry and hashing helper.
*/
/* End Header *******************************************************************/

using System;

namespace GrapeEngine.Scripting.Components;

/// <summary>
/// Internal registry for component type hashing and identification.
/// 
/// Uses FNV-1a hash algorithm to generate deterministic 32-bit hashes from component type names.
/// These hashes are used by the C++ engine to identify and match component types across
/// the managed-native boundary.
/// 
/// IMPORTANT: The hash algorithm MUST match the C++ ComponentType::Hash() implementation exactly.
/// Any change to the hashing function will break component registration and queries.
/// </summary>
internal static class ComponentTypeRegistry
{
    /// <summary>
    /// FNV-1a (Fowler-Noll-Vo) hash function for string hashing.
    /// 
    /// This implementation:
    /// - Uses FNV offset basis: 0x811C9DC5
    /// - Uses FNV prime: 0x01000193
    /// - Processes each character: hash ^= char, hash *= prime
    /// 
    /// IMPORTANT: This algorithm MUST match the C++ implementation in ComponentType::Hash().
    /// Do NOT modify without updating C++ code and all existing serialized component data.
    /// 
    /// The algorithm produces identical hashes for the same input on both C# and C++ sides,
    /// allowing the engine to reliably identify component types during registration and queries.
    /// </summary>
    /// <param name="str">The component type name (e.g., "LocalTransform")</param>
    /// <returns>32-bit FNV-1a hash of the input string</returns>
    private static uint FNV1aHash(string str)
    {
        // FNV-1a constants matching C++ implementation
        const uint fnvPrime = 0x01000193;     // FNV prime
        const uint fnvOffset = 0x811C9DC5;    // FNV offset basis

        var hash = fnvOffset;
        foreach (var c in str)
        {
            hash ^= c;
            hash *= fnvPrime;
        }

        return hash;
    }

    /// <summary>
    /// Get the FNV-1a hash for a component type.
    /// 
    /// This hash is used throughout the ECS system to identify component types:
    /// - Component registration in the native engine
    /// - Query building and execution
    /// - Component serialization (hot reload)
    /// - Entity component lookups
    /// 
    /// The generic type parameter must be an unmanaged struct with sequential layout.
    /// Hash values are cached by the runtime and reused for all instances of the same type.
    /// </summary>
    /// <typeparam name="T">The component type to hash (must be unmanaged struct)</typeparam>
    /// <returns>32-bit hash value for the component type</returns>
    public static uint GetTypeHash<T>() where T : unmanaged
    {
        var name = typeof(T).Name;
        var hash = FNV1aHash(name);
        return hash;
    }
}

