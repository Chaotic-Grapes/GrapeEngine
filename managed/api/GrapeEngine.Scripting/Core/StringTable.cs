/* Start Header *****************************************************************/
/*!
\file   StringTable.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Internal string interning table. Provides thread-safe string-to-ID mapping
for the StringId system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Collections.Concurrent;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Core;

/// <summary>
/// Internal string interning table for StringId system.
/// 
/// This class maintains bidirectional mappings between strings and uint identifiers:
/// - string -> uint (for interning)
/// - uint -> string (for resolution)
/// 
/// THREAD SAFETY:
/// - All operations are thread-safe using ConcurrentDictionary and lock
/// - Multiple threads can intern and resolve strings concurrently
/// 
/// HOT RELOAD SEMANTICS:
/// - Do NOT call Clear() on script hot reload
/// - StringIds stored in ECS components must remain valid
/// - Only clear when:
///   * World is destroyed
///   * New level is loaded
///   * Explicit reset is required
/// 
/// IMPLEMENTATION NOTES:
/// - Uses Ordinal string comparison for UTF-8 correctness
/// - ID 0 is reserved for invalid/null
/// - IDs start from 1 and increment sequentially
/// - Uses double-checked locking pattern for thread-safe interning
/// </summary>
internal static class StringTable
{
    private static readonly ConcurrentDictionary<string, uint> _stringToId = new(StringComparer.Ordinal);
    private static readonly ConcurrentDictionary<uint, string> _idToString = new();

    private static uint _nextId = 1; // 0 = invalid
    private static readonly object _lock = new();

    /// <summary>
    /// Intern a string and return its StringId.
    /// 
    /// If the string has been interned before, returns the existing ID.
    /// Otherwise, allocates a new ID and stores the mapping.
    /// 
    /// Thread-safe using double-checked locking pattern.
    /// </summary>
    /// <param name="value">The string to intern (must not be null)</param>
    /// <returns>StringId for the interned string</returns>
    public static StringId Intern(string value)
    {
        if (TryInternNative(value, out uint nativeId))
        {
            CacheMapping(value, nativeId);
            return new StringId(nativeId);
        }

        return InternManaged(value);
    }

    /// <summary>
    /// Resolve a StringId back to its original string.
    /// 
    /// Returns null if the ID is invalid (0) or not found.
    /// Thread-safe (read-only operation on concurrent dictionary).
    /// </summary>
    /// <param name="id">The StringId to resolve</param>
    /// <returns>The original string, or null if not found</returns>
    public static string? Resolve(StringId id)
    {
        if (id.Value == 0)
            return null;

        if (_idToString.TryGetValue(id.Value, out var cached))
            return cached;

        string? resolved = TryResolveNative(id.Value);
        if (!string.IsNullOrEmpty(resolved))
        {
            CacheMapping(resolved, id.Value);
            return resolved;
        }

        return null;
    }

    /// <summary>
    /// Clear all interned strings and reset the ID counter.
    /// 
    /// WARNING: Only call this when:
    /// - World is destroyed
    /// - New level is loaded
    /// - Explicit reset is required
    /// 
    /// DO NOT call during hot reload - StringIds in ECS components must remain valid.
    /// </summary>
    internal static void Clear()
    {
        lock (_lock)
        {
            _stringToId.Clear();
            _idToString.Clear();
            _nextId = 1;
        }
    }

    private static StringId InternManaged(string value)
    {
        // Fast path: check if already interned (no lock required)
        if (_stringToId.TryGetValue(value, out uint existing))
            return new StringId(existing);

        // Slow path: need to allocate new ID (requires lock)
        lock (_lock)
        {
            // Double-check: another thread may have interned it while we waited
            if (_stringToId.TryGetValue(value, out existing))
                return new StringId(existing);

            // Allocate new ID and store bidirectional mapping
            uint id = _nextId++;
            _stringToId[value] = id;
            _idToString[id] = value;
            return new StringId(id);
        }
    }

    private static void CacheMapping(string value, uint id)
    {
        if (id == 0)
            return;

        lock (_lock)
        {
            _stringToId[value] = id;
            _idToString[id] = value;
            if (id >= _nextId)
            {
                _nextId = id + 1;
            }
        }
    }

    private static bool TryInternNative(string value, out uint id)
    {
        try
        {
            id = StringAPI.Intern(value);
            return true;
        }
        catch (DllNotFoundException)
        {
            id = 0;
            return false;
        }
        catch (EntryPointNotFoundException)
        {
            id = 0;
            return false;
        }
    }

    private static string? TryResolveNative(uint id)
    {
        try
        {
            nint ptr = StringAPI.Resolve(id);
            if (ptr == nint.Zero)
                return null;

            string? value = Marshal.PtrToStringUTF8(ptr);
            StringAPI.FreeString(ptr);
            return value;
        }
        catch (DllNotFoundException)
        {
            return null;
        }
        catch (EntryPointNotFoundException)
        {
            return null;
        }
    }
}
