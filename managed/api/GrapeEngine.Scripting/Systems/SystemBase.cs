/* Start Header *****************************************************************/
/*!
\file   SystemBase.cs
\brief  Base class for managed systems. Implements `ISystem` and provides
        query helpers.
*/
/* End Header *******************************************************************/

using System.Runtime.CompilerServices;
using GrapeEngine.Scripting.Internal.Query;
using GrapeEngine.Scripting.Services;

namespace GrapeEngine.Scripting.Systems;

/// <summary>
/// Base class for user systems. Implements the existing `ISystem` contract
/// so it can be discovered by `SystemDiscovery` and instantiated via Activator.
///
/// Provides:
/// - protected `World` property
/// - simple query helpers that wrap `World.Query<T...>()`
/// - optional override points: `OnCreate`, `OnUpdate`, `OnDestroy`
///
/// NOTES:
/// - `ISystem` methods are implemented explicitly to preserve the original
///   signatures used by the native host.
/// - Query caching is a convenience and not required for correctness.
/// </summary>
public abstract class SystemBase : ISystem
{
    /// <summary>
    /// The `World` instance assigned by the engine during creation.
    /// </summary>
    protected World? World { get; private set; }

    private Dictionary<string, object>? _queryCache;

    void ISystem.OnCreate(World world)
    {
        World = world;
        _queryCache = [];
        OnCreate();
    }

    /// <summary>
    /// Optional one-time initialization. Override in derived systems.
    /// </summary>
    protected virtual void OnCreate() { }

    void ISystem.OnUpdate(World world)
    {
        // Ensure the protected World property is up-to-date for derived systems
        if (World != world)
        {
            World = world;
            _queryCache?.Clear();
        }
        // Call derived OnUpdate for sequential execution
        OnUpdate();
    }

    /// <summary>
    /// Per-frame update. Override to implement system logic. Use `World` and
    /// the provided helpers to query and schedule work.
    /// </summary>
    protected virtual void OnUpdate() { }

    void ISystem.OnDestroy(World world)
    {
        try
        {
            OnDestroy();
        }
        finally
        {
            _queryCache?.Clear();
            _queryCache = null;
            World = null;
        }
    }

    /// <summary>
    /// Log a message at the specified level.
    /// </summary>
    /// <param name="message">Message text</param>
    /// <param name="level">Log level</param>
    protected static void Log(string message, LogLevel level = LogLevel.Info)
        => Services.Log.Write(message, level);

    /// <summary>
    /// Log a message using a factory function.
    /// </summary>
    /// <param name="messageFactory">Function that produces the message text</param>
    /// <param name="level">Log level</param>
    protected static void Log(Func<string> messageFactory, LogLevel level = LogLevel.Info)
        => Services.Log.Write(messageFactory, level);

    /// <summary>
    /// Log a message at the specified level with source location.
    /// </summary>
    /// <param name="message">Message text</param>
    /// <param name="level">Log level</param>
    /// <param name="file">Source file path (automatically provided)</param>
    /// <param name="line">Source line number (automatically provided)</param>
    protected static void LogFrom(
        string message,
        LogLevel level = LogLevel.Info,
        [CallerFilePath] string file = "",
        [CallerLineNumber] int line = 0)
        => Services.Log.Write(message, level, file, line);

    /// <summary>
    /// Log a message using a factory function with source location.
    /// </summary>
    /// <param name="messageFactory">Function that produces the message text</param>
    /// <param name="level">Log level</param>
    /// <param name="file">Source file path (automatically provided)</param>
    /// <param name="line">Source line number (automatically provided)</param>
    public static void LogFrom(
        Func<string> messageFactory,
        LogLevel level = LogLevel.Info,
        [CallerFilePath] string file = "",
        [CallerLineNumber] int line = 0)
        => Services.Log.Write(messageFactory, level, file, line);

    /// <summary>
    /// Override for cleanup logic.
    /// </summary>
    protected virtual void OnDestroy() { }

    // ------------------------ Query helpers ------------------------

    /// <summary>
    /// Get or create a cached `Query&lt;T1&gt;` for this system's world.
    /// </summary>
    protected Query<T1> Query<T1>() 
        where T1 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= [];
        string key = typeof(T1).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1>)obj;

        var q = World.Query<T1>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2> Query<T1, T2>()
        where T1 : unmanaged
        where T2 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= [];
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2>)obj;

        var q = World.Query<T1, T2>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2, T3> Query<T1, T2, T3>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= [];
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName! + ":" + typeof(T3).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2, T3>)obj;

        var q = World.Query<T1, T2, T3>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2, T3, T4> Query<T1, T2, T3, T4>()
        where T1 : unmanaged 
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= [];
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName! + ":" + typeof(T3).FullName! + ":" + typeof(T4).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2, T3, T4>)obj;

        var q = World.Query<T1, T2, T3, T4>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2, T3, T4, T5> Query<T1, T2, T3, T4, T5>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
        where T5 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= [];
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName! + ":" + typeof(T3).FullName! + ":" + typeof(T4).FullName! + ":" + typeof(T5).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2, T3, T4, T5>)obj;
        var q = World.Query<T1, T2, T3, T4, T5>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2, T3, T4, T5, T6> Query<T1, T2, T3, T4, T5, T6>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
        where T5 : unmanaged
        where T6 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= [];
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName! + ":" + typeof(T3).FullName! + ":" + typeof(T4).FullName! + ":" + typeof(T5).FullName! + ":" + typeof(T6).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2, T3, T4, T5, T6>)obj;
        var q = World.Query<T1, T2, T3, T4, T5, T6>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2, T3, T4, T5, T6, T7> Query<T1, T2, T3, T4, T5, T6, T7>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
        where T5 : unmanaged
        where T6 : unmanaged
        where T7 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= [];
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName! + ":" + typeof(T3).FullName! + ":" + typeof(T4).FullName! + ":" + typeof(T5).FullName! + ":" + typeof(T6).FullName! + ":" + typeof(T7).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2, T3, T4, T5, T6, T7>)obj;
        var q = World.Query<T1, T2, T3, T4, T5, T6, T7>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2, T3, T4, T5, T6, T7, T8> Query<T1, T2, T3, T4, T5, T6, T7, T8>()
        where T1 : unmanaged
        where T2 : unmanaged
        where T3 : unmanaged
        where T4 : unmanaged
        where T5 : unmanaged
        where T6 : unmanaged
        where T7 : unmanaged
        where T8 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= [];
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName! + ":" + typeof(T3).FullName! + ":" + typeof(T4).FullName! + ":" + typeof(T5).FullName! + ":" + typeof(T6).FullName! + ":" + typeof(T7).FullName! + ":" + typeof(T8).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2, T3, T4, T5, T6, T7, T8>)obj;
        var q = World.Query<T1, T2, T3, T4, T5, T6, T7, T8>();
        _queryCache[key] = q;
        return q;
    }
}

