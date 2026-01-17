/* Start Header *****************************************************************/
/*!
\file   SystemBase.cs
\brief  Base class for managed systems. Implements `ISystem` and provides
        query helpers and job scheduling convenience methods.
*/
/* End Header *******************************************************************/

using System.Collections.Generic;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Query;
using GrapeEngine.Scripting.Job;

namespace GrapeEngine.Scripting.Systems;

/// <summary>
/// Base class for user systems. Implements the existing `ISystem` contract
/// so it can be discovered by `SystemDiscovery` and instantiated via Activator.
///
/// Provides:
/// - protected `World` property
/// - simple query helpers that wrap `World.Query<T...>()`
/// - scheduling helper for `IJob`
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
        _queryCache = new Dictionary<string, object>();
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
            World = world;
        // Keep compatibility: call derived OnUpdate which may schedule jobs
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
    /// Log a message prefixed with the system's type name.
    /// </summary>
    /// <param name="message">Message text</param>
    /// <param name="level">Log level</param>
    protected void Log(string message, LogLevel level = LogLevel.Info)
    {
        Logging.Log(message, level);
    }

    /// <summary>
    /// Override for cleanup logic.
    /// </summary>
    protected virtual void OnDestroy() { }

    // ------------------------ Query helpers ------------------------

    /// <summary>
    /// Get or create a cached `Query&lt;T1&gt;` for this system's world.
    /// </summary>
    protected Query<T1> Query<T1>() where T1 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= new Dictionary<string, object>();
        string key = typeof(T1).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1>)obj;

        var q = World.Query<T1>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2> Query<T1, T2>()
        where T1 : unmanaged where T2 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= new Dictionary<string, object>();
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2>)obj;

        var q = World.Query<T1, T2>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2, T3> Query<T1, T2, T3>()
        where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= new Dictionary<string, object>();
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName! + ":" + typeof(T3).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2, T3>)obj;

        var q = World.Query<T1, T2, T3>();
        _queryCache[key] = q;
        return q;
    }

    protected Query<T1, T2, T3, T4> Query<T1, T2, T3, T4>()
        where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged where T4 : unmanaged
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        _queryCache ??= new Dictionary<string, object>();
        string key = typeof(T1).FullName! + ":" + typeof(T2).FullName! + ":" + typeof(T3).FullName! + ":" + typeof(T4).FullName!;
        if (_queryCache.TryGetValue(key, out var obj))
            return (Query<T1, T2, T3, T4>)obj;

        var q = World.Query<T1, T2, T3, T4>();
        _queryCache[key] = q;
        return q;
    }

    // ------------------------ Job helpers ------------------------

    /// <summary>
    /// Schedule an <see cref="IJob"/> through the world's JobManager.
    /// Returns the created JobHandle.
    /// </summary>
    protected JobHandle Schedule(IJob job, JobHandle? dependsOn = null, int priority = 0)
    {
        if (World == null) throw new InvalidOperationException("World is not set");
        return World.JobManager.Schedule(job, dependsOn, priority);
    }

    /// <summary>
    /// Helper to complete a job handle synchronously.
    /// </summary>
    protected void Complete(JobHandle handle)
    {
        handle?.Complete();
    }
}
