namespace GrapeEngine.Scripting.Core;

/// <summary>
/// Helper class for component type hashing (matches C++ FNV-1a implementation).
/// </summary>
internal static class ComponentTypeHelper
{
    private static readonly Dictionary<Type, uint> s_typeHashCache = new();

    public static uint GetTypeHash<T>()
    {
        return GetTypeHash(typeof(T));
    }

    public static uint GetTypeHash(Type type)
    {
        if (s_typeHashCache.TryGetValue(type, out uint hash))
        {
            return hash;
        }

        // FNV-1a hash algorithm - must match C++ implementation.
        // Hash only the type name to match native registration.
        string typeName = type.Name;
        hash = FNV1aHash(typeName);

        s_typeHashCache[type] = hash;
        return hash;
    }

    /// <summary>
    /// Clear the type hash cache during assembly unload.
    /// This is critical for hot reload: the cache holds Type references that prevent
    /// the AssemblyLoadContext from being garbage collected.
    /// </summary>
    internal static void ClearTypeHashCache()
    {
        try
        {
            int count = s_typeHashCache.Count;
            s_typeHashCache.Clear();
            if (count > 0)
            {
                Logging.LogInternal($"[ComponentTypeHelper] Cleared {count} type hash cache entries", LogLevel.Info);
            }
        }
        catch (Exception ex) when (ex is not OutOfMemoryException and not StackOverflowException)
        {
            Logging.LogInternal($"[ComponentTypeHelper] Error clearing type hash cache: {ex.Message}", LogLevel.Error);
        }
    }

    private static uint FNV1aHash(string str)
    {
        uint hash = 2166136261u;
        foreach (char c in str)
        {
            hash ^= (byte)c;
            hash *= 16777619u;
        }
        return hash;
    }
}
