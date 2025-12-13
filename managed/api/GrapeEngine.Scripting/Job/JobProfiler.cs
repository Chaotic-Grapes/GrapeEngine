/* Start Header *****************************************************************/
/*!
\file   JobProfiler.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Managed wrapper for job profiling and performance metrics collection.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Unsafe;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Job;

/// <summary>
/// Performance metrics for a single job execution.
/// </summary>
public struct JobMetrics
{
    /// <summary>
    /// Name of the job
    /// </summary>
    public string JobName { get; set; }

    /// <summary>
    /// How long the job took to execute (microseconds)
    /// </summary>
    public long ExecutionTimeMicros { get; set; }

    /// <summary>
    /// How long the job waited before execution (microseconds)
    /// </summary>
    public long WaitTimeMicros { get; set; }

    /// <summary>
    /// Which thread executed this job
    /// </summary>
    public uint ExecutingThreadId { get; set; }

    /// <summary>
    /// Number of entities processed by this job
    /// </summary>
    public ulong EntitiesProcessed { get; set; }

    /// <summary>
    /// Whether this job was stolen from another thread
    /// </summary>
    public bool WasStolen { get; set; }

    /// <summary>
    /// Get execution time as a TimeSpan
    /// </summary>
    public readonly TimeSpan ExecutionTime
        => TimeSpan.FromMicroseconds(ExecutionTimeMicros);

    /// <summary>
    /// Get wait time as a TimeSpan
    /// </summary>
    public readonly TimeSpan WaitTime
        => TimeSpan.FromMicroseconds(WaitTimeMicros);
}

/// <summary>
/// Aggregated statistics for a job type.
/// </summary>
public struct JobTypeStats
{
    /// <summary>
    /// Name of the job type
    /// </summary>
    public string JobName { get; set; }

    /// <summary>
    /// Total number of executions
    /// </summary>
    public ulong ExecutionCount { get; set; }

    /// <summary>
    /// Total execution time across all runs (microseconds)
    /// </summary>
    public long TotalExecutionTimeMicros { get; set; }

    /// <summary>
    /// Minimum execution time (microseconds)
    /// </summary>
    public long MinExecutionTimeMicros { get; set; }

    /// <summary>
    /// Maximum execution time (microseconds)
    /// </summary>
    public long MaxExecutionTimeMicros { get; set; }

    /// <summary>
    /// Average execution time (microseconds)
    /// </summary>
    public long AvgExecutionTimeMicros { get; set; }

    /// <summary>
    /// How many times this job was stolen
    /// </summary>
    public ulong StolenCount { get; set; }

    /// <summary>
    /// Total entities processed
    /// </summary>
    public ulong EntitiesProcessedTotal { get; set; }

    /// <summary>
    /// Get average execution time as TimeSpan
    /// </summary>
    public readonly TimeSpan AvgExecutionTime 
        => TimeSpan.FromMicroseconds(AvgExecutionTimeMicros);

    /// <summary>
    /// Get theft rate (0-1)
    /// </summary>
    public readonly double TheftRate 
        => ExecutionCount > 0 
            ? (double)StolenCount / ExecutionCount 
            : 0;
}

/// <summary>
/// Profiler for job system performance metrics.
/// 
/// Collects and analyzes performance data for job execution including:
/// - Execution times
/// - Queue wait times
/// - Work stealing statistics
/// - Entity processing throughput
/// 
/// Access through JobManager.Profiler.
/// </summary>
public class JobProfiler
{
    private readonly nint _nativeJobManager;

    /// <summary>
    /// Create a profiler for a job manager.
    /// </summary>
    internal JobProfiler(nint nativeJobManager)
    {
        _nativeJobManager = nativeJobManager;
    }

    /// <summary>
    /// Check if profiling is currently enabled.
    /// </summary>
    public bool IsEnabled
    {
        get
        {
            unsafe
            {
                return JobSystemAPI.JobProfilerIsEnabled(_nativeJobManager.ToPointer());
            }
        }
    }

    /// <summary>
    /// Get statistics for a specific job type.
    /// </summary>
    public JobTypeStats? GetStats(string jobName)
    {
        if (string.IsNullOrEmpty(jobName))
            return null;

        unsafe
        {
            JobSystemAPI.JobTypeStats nativeStats = JobSystemAPI.JobProfilerGetStats(
                _nativeJobManager.ToPointer(),
                jobName
            );

            return new JobTypeStats
            {
                JobName = jobName,
                ExecutionCount = nativeStats.ExecutionCount,
                TotalExecutionTimeMicros = nativeStats.TotalExecutionTimeMicros,
                MinExecutionTimeMicros = nativeStats.MinExecutionTimeMicros,
                MaxExecutionTimeMicros = nativeStats.MaxExecutionTimeMicros,
                AvgExecutionTimeMicros = nativeStats.AvgExecutionTimeMicros,
                StolenCount = nativeStats.StolenCount,
                EntitiesProcessedTotal = nativeStats.EntitiesProcessedTotal
            };
        }
    }

    /// <summary>
    /// Get statistics for all tracked job types.
    /// </summary>
    public JobTypeStats[] GetAllStats()
    {
        unsafe
        {
            uint count = 0;
            JobSystemAPI.JobTypeStats* statsPtr = JobSystemAPI.JobProfilerGetAllStats(
                _nativeJobManager.ToPointer(),
                &count
            );

            if (statsPtr == null || count == 0)
                return [];

            JobTypeStats[] result = new JobTypeStats[count];
            for (var i = 0; i < count; i++)
            {
                result[i] = new JobTypeStats
                {
                    JobName = Marshal.PtrToStringAnsi((nint)statsPtr[i].JobNamePtr) ?? "",
                    ExecutionCount = statsPtr[i].ExecutionCount,
                    TotalExecutionTimeMicros = statsPtr[i].TotalExecutionTimeMicros,
                    MinExecutionTimeMicros = statsPtr[i].MinExecutionTimeMicros,
                    MaxExecutionTimeMicros = statsPtr[i].MaxExecutionTimeMicros,
                    AvgExecutionTimeMicros = statsPtr[i].AvgExecutionTimeMicros,
                    StolenCount = statsPtr[i].StolenCount,
                    EntitiesProcessedTotal = statsPtr[i].EntitiesProcessedTotal
                };
            }

            JobSystemAPI.FreeJobStatsArray(statsPtr, count);
            return result;
        }
    }

    /// <summary>
    /// Get all recorded individual metrics.
    /// </summary>
    public JobMetrics[] GetAllMetrics()
    {
        unsafe
        {
            uint count = 0;
            JobSystemAPI.JobMetrics* metricsPtr = JobSystemAPI.JobProfilerGetAllMetrics(
                _nativeJobManager.ToPointer(),
                &count
            );

            if (metricsPtr == null || count == 0)
                return [];

            JobMetrics[] result = new JobMetrics[count];
            for (var i = 0; i < count; i++)
            {
                result[i] = new JobMetrics
                {
                    JobName = Marshal.PtrToStringAnsi((nint)metricsPtr[i].JobNamePtr) ?? "",
                    ExecutionTimeMicros = metricsPtr[i].ExecutionTimeMicros,
                    WaitTimeMicros = metricsPtr[i].WaitTimeMicros,
                    ExecutingThreadId = metricsPtr[i].ExecutingThreadId,
                    EntitiesProcessed = metricsPtr[i].EntitiesProcessed,
                    WasStolen = metricsPtr[i].WasStolen
                };
            }

            JobSystemAPI.FreeJobMetricsArray(metricsPtr, count);
            return result;
        }
    }

    /// <summary>
    /// Reset all profiling data.
    /// </summary>
    public void Reset()
    {
        unsafe
        {
            JobSystemAPI.JobProfilerReset(_nativeJobManager.ToPointer());
        }
    }

    /// <summary>
    /// Mark the start of a frame for profiling.
    /// </summary>
    public void MarkFrameStart()
    {
        unsafe
        {
            JobSystemAPI.JobProfilerMarkFrameStart(_nativeJobManager.ToPointer());
        }
    }

    /// <summary>
    /// Mark the end of a frame and finalize statistics.
    /// </summary>
    public void MarkFrameEnd()
    {
        unsafe
        {
            JobSystemAPI.JobProfilerMarkFrameEnd(_nativeJobManager.ToPointer());
        }
    }

    /// <summary>
    /// Get a formatted report of profiling data.
    /// </summary>
    public override string ToString()
    {
        unsafe
        {
            byte* reportPtr = JobSystemAPI.JobProfilerGenerateReport(_nativeJobManager.ToPointer());
            if (reportPtr == null)
                return string.Empty;
            
            string report = Marshal.PtrToStringAnsi((nint)reportPtr) ?? string.Empty;
            JobSystemAPI.FreeString(reportPtr);
            return report;
        }
    }
}
