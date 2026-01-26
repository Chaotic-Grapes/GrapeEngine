/* Start Header *****************************************************************/
/*!
\file   ProfiledQueryIterator.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\par    
\brief
Profiling-aware query iterators that track execution metrics during iteration.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Internal.Unsafe;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Internal.Query;

/// <summary>
/// Profiling-enabled query iterator for single-component queries.
/// 
/// Wraps a standard QueryEnumerator and tracks performance metrics during iteration.
/// Records entity processing counts and execution time for optimization analysis.
/// 
/// USAGE:
/// ```csharp
/// var iter = query.GetEnumeratorWithProfiling();
/// int count = 0;
/// while (iter.MoveNext())
/// {
///     var (entity, component) = iter.Current;
///     // Process entity...
///     count++;
/// }
/// // Automatically records: count entities processed, total execution time
/// ```
/// </summary>
public unsafe struct ProfiledQueryIterator<T1>
    where T1 : unmanaged
{
    private QueryEnumerator<T1> _enumerator;
    private readonly OptimizationProfiler _profiler;
    private readonly string _queryName;
    private readonly System.Diagnostics.Stopwatch _timer;
    private int _entitiesProcessed;

    internal ProfiledQueryIterator(
        World world,
        uint[] componentHashes,
        OptimizationProfiler profiler,
        string queryName)
    {
        _enumerator = new QueryEnumerator<T1>(world, componentHashes);
        _profiler = profiler;
        _queryName = queryName;
        _timer = System.Diagnostics.Stopwatch.StartNew();
        _entitiesProcessed = 0;
    }

    public QueryResult<T1> Current => _enumerator.Current;

    public bool MoveNext()
    {
        if (_enumerator.MoveNext())
        {
            _entitiesProcessed++;
            return true;
        }
        else
        {
            // Iteration complete, record metrics
            _timer.Stop();
            long nanoseconds = _timer.ElapsedTicks * 1_000_000_000 / System.Diagnostics.Stopwatch.Frequency;
            _profiler.RecordExecution(_queryName, nanoseconds, _entitiesProcessed, OptimizationSafety.Normal);
            return false;
        }
    }

    public void Reset() => throw new NotSupportedException();
    public void Dispose() { }
}

/// <summary>
/// Profiling-enabled query iterator for two-component queries.
/// </summary>
public unsafe struct ProfiledQueryIterator<T1, T2>
    where T1 : unmanaged
    where T2 : unmanaged
{
    private QueryEnumerator<T1, T2> _enumerator;
    private readonly OptimizationProfiler _profiler;
    private readonly string _queryName;
    private readonly System.Diagnostics.Stopwatch _timer;
    private int _entitiesProcessed;

    internal ProfiledQueryIterator(
        World world,
        uint[] componentHashes,
        OptimizationProfiler profiler,
        string queryName)
    {
        _enumerator = new QueryEnumerator<T1, T2>(world, componentHashes);
        _profiler = profiler;
        _queryName = queryName;
        _timer = System.Diagnostics.Stopwatch.StartNew();
        _entitiesProcessed = 0;
    }

    public QueryResult<T1, T2> Current => _enumerator.Current;

    public bool MoveNext()
    {
        if (_enumerator.MoveNext())
        {
            _entitiesProcessed++;
            return true;
        }
        else
        {
            // Iteration complete, record metrics
            _timer.Stop();
            long nanoseconds = _timer.ElapsedTicks * 1_000_000_000 / System.Diagnostics.Stopwatch.Frequency;
            _profiler.RecordExecution(_queryName, nanoseconds, _entitiesProcessed, OptimizationSafety.Normal);
            return false;
        }
    }

    public void Reset() => throw new NotSupportedException();
    public void Dispose() { }
}

/// <summary>
/// Profiling-enabled query iterator for three-component queries.
/// </summary>
public unsafe struct ProfiledQueryIterator<T1, T2, T3>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
{
    private QueryEnumerator<T1, T2, T3> _enumerator;
    private readonly OptimizationProfiler _profiler;
    private readonly string _queryName;
    private readonly System.Diagnostics.Stopwatch _timer;
    private int _entitiesProcessed;

    internal ProfiledQueryIterator(
        World world,
        uint[] componentHashes,
        OptimizationProfiler profiler,
        string queryName)
    {
        _enumerator = new QueryEnumerator<T1, T2, T3>(world, componentHashes);
        _profiler = profiler;
        _queryName = queryName;
        _timer = System.Diagnostics.Stopwatch.StartNew();
        _entitiesProcessed = 0;
    }

    public QueryResult<T1, T2, T3> Current => _enumerator.Current;

    public bool MoveNext()
    {
        if (_enumerator.MoveNext())
        {
            _entitiesProcessed++;
            return true;
        }
        else
        {
            // Iteration complete, record metrics
            _timer.Stop();
            long nanoseconds = _timer.ElapsedTicks * 1_000_000_000 / System.Diagnostics.Stopwatch.Frequency;
            _profiler.RecordExecution(_queryName, nanoseconds, _entitiesProcessed, OptimizationSafety.Normal);
            return false;
        }
    }

    public void Reset() => throw new NotSupportedException();
    public void Dispose() { }
}

/// <summary>
/// Profiling-enabled query iterator for four-component queries.
/// </summary>
public unsafe struct ProfiledQueryIterator<T1, T2, T3, T4>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
{
    private QueryEnumerator<T1, T2, T3, T4> _enumerator;
    private readonly OptimizationProfiler _profiler;
    private readonly string _queryName;
    private readonly System.Diagnostics.Stopwatch _timer;
    private int _entitiesProcessed;

    internal ProfiledQueryIterator(
        World world,
        uint[] componentHashes,
        OptimizationProfiler profiler,
        string queryName)
    {
        _enumerator = new QueryEnumerator<T1, T2, T3, T4>(world, componentHashes);
        _profiler = profiler;
        _queryName = queryName;
        _timer = System.Diagnostics.Stopwatch.StartNew();
        _entitiesProcessed = 0;
    }

    public QueryResult<T1, T2, T3, T4> Current => _enumerator.Current;

    public bool MoveNext()
    {
        if (_enumerator.MoveNext())
        {
            _entitiesProcessed++;
            return true;
        }
        else
        {
            // Iteration complete, record metrics
            _timer.Stop();
            long nanoseconds = _timer.ElapsedTicks * 1_000_000_000 / System.Diagnostics.Stopwatch.Frequency;
            _profiler.RecordExecution(_queryName, nanoseconds, _entitiesProcessed, OptimizationSafety.Normal);
            return false;
        }
    }

    public void Reset() => throw new NotSupportedException();
    public void Dispose() { }
}


