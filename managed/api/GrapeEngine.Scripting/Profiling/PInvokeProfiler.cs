/* Start Header *****************************************************************/
/*!
\file   PInvokeProfiler.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Profiler for measuring P/Invoke call overhead and performance bottlenecks.
Tracks call counts, timings, and identifies expensive native transitions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Math;
using System.Diagnostics;

namespace GrapeEngine.Scripting.Profiling;

/// <summary>
/// Tracks P/Invoke call performance metrics.
/// </summary>
public class PInvokeMetrics
{
    public string FunctionName { get; set; } = "";
    public long TotalCallCount { get; set; }
    public long TotalMicroseconds { get; set; }
    public long MinMicroseconds { get; set; } = long.MaxValue;
    public long MaxMicroseconds { get; set; }

    public double AverageMicroseconds => TotalCallCount > 0 ? (double)TotalMicroseconds / TotalCallCount : 0;
    public double CallsPerSecond => TotalCallCount > 0 ? TotalCallCount * 1_000_000.0 / TotalMicroseconds : 0;
}

/// <summary>
/// Profiles P/Invoke call performance across the entire session.
/// </summary>
public static class PInvokeProfiler
{
    private static readonly Dictionary<string, PInvokeMetrics> _metrics = [];
    private static readonly Lock _lock = new();
    private static bool _enabled = false;

    /// <summary>
    /// Enable or disable profiling.
    /// </summary>
    public static void SetEnabled(bool enabled)
    {
        _enabled = enabled;
        if (enabled)
        {
            Console.WriteLine("[PInvokeProfiler] Profiling enabled");
        }
        else
        {
            Console.WriteLine("[PInvokeProfiler] Profiling disabled");
        }
    }

    /// <summary>
    /// Record a P/Invoke call's execution time.
    /// </summary>
    public static void RecordCall(string functionName, long microseconds)
    {
        if (!_enabled) return;

        lock (_lock)
        {
            if (!_metrics.TryGetValue(functionName, out var metrics))
            {
                metrics = new PInvokeMetrics { FunctionName = functionName };
                _metrics[functionName] = metrics;
            }

            metrics.TotalCallCount++;
            metrics.TotalMicroseconds += microseconds;
            metrics.MinMicroseconds = GMath.Min(metrics.MinMicroseconds, microseconds);
            metrics.MaxMicroseconds = GMath.Max(metrics.MaxMicroseconds, microseconds);
        }
    }

    /// <summary>
    /// Get metrics for a specific function, or null if not found.
    /// </summary>
    public static PInvokeMetrics? GetMetrics(string functionName)
    {
        lock (_lock)
        {
            return _metrics.TryGetValue(functionName, out var metrics) ? metrics : null;
        }
    }

    /// <summary>
    /// Get all recorded metrics, sorted by total time (descending).
    /// </summary>
    public static List<PInvokeMetrics> GetAllMetrics()
    {
        lock (_lock)
        {
            return [.._metrics.Values.OrderByDescending(m => m.TotalMicroseconds)];
        }
    }

    /// <summary>
    /// Clear all recorded metrics.
    /// </summary>
    public static void Reset()
    {
        lock (_lock)
        {
            _metrics.Clear();
        }
    }

    /// <summary>
    /// Print a formatted report of all metrics.
    /// </summary>
    public static void PrintReport()
    {
        lock (_lock)
        {
            if (_metrics.Count == 0)
            {
                Console.WriteLine("[PInvokeProfiler] No metrics recorded");
                return;
            }

            Console.WriteLine("\n" + new string('=', 100));
            Console.WriteLine("P/Invoke Performance Report".PadRight(100));
            Console.WriteLine(new string('=', 100));
            Console.WriteLine(
                "Function Name".PadRight(40) +
                "Calls".PadRight(12) +
                "Total (μs)".PadRight(15) +
                "Avg (μs)".PadRight(12) +
                "Min (μs)".PadRight(12) +
                "Max (μs)".PadRight(12)
            );
            Console.WriteLine(new string('-', 100));

            var sorted = _metrics.Values.OrderByDescending(m => m.TotalMicroseconds);
            foreach (var metric in sorted)
            {
                Console.WriteLine(
                    metric.FunctionName.PadRight(40) +
                    metric.TotalCallCount.ToString().PadRight(12) +
                    metric.TotalMicroseconds.ToString().PadRight(15) +
                    metric.AverageMicroseconds.ToString("F2").PadRight(12) +
                    metric.MinMicroseconds.ToString().PadRight(12) +
                    metric.MaxMicroseconds.ToString().PadRight(12)
                );
            }

            Console.WriteLine(new string('=', 100) + "\n");
        }
    }

    /// <summary>
    /// Get total time spent in P/Invoke calls across all functions.
    /// </summary>
    public static long GetTotalMicroseconds()
    {
        lock (_lock)
        {
            return _metrics.Values.Sum(m => m.TotalMicroseconds);
        }
    }

    /// <summary>
    /// Get total number of P/Invoke calls.
    /// </summary>
    public static long GetTotalCalls()
    {
        lock (_lock)
        {
            return _metrics.Values.Sum(m => m.TotalCallCount);
        }
    }
}

/// <summary>
/// Convenience class for timing a P/Invoke call using a using statement.
/// Usage: using (var timer = PInvokeTimer.Start("FunctionName")) { /* call P/Invoke */ }
/// </summary>
public class PInvokeTimer : IDisposable
{
    private readonly string _functionName;
    private readonly Stopwatch _stopwatch;

    private PInvokeTimer(string functionName)
    {
        _functionName = functionName;
        _stopwatch = Stopwatch.StartNew();
    }

    /// <summary>
    /// Start timing a P/Invoke call.
    /// </summary>
    public static PInvokeTimer Start(string functionName)
    {
        return new PInvokeTimer(functionName);
    }

    public void Dispose()
    {
        _stopwatch.Stop();
        var microseconds = _stopwatch.Elapsed.Ticks / 10;
        PInvokeProfiler.RecordCall(_functionName, microseconds);

        GC.SuppressFinalize(this);
    }
}
