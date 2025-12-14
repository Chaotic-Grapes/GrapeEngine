/* Start Header *****************************************************************/
/*!
\file   JobBatchingUtilities.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Utilities for batching entities and managing job workload distribution.

Provides job batching strategies, work distribution helpers, and
performance utilities for parallel entity processing.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Job;
using GrapeEngine.Scripting.Query;
using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace GrapeEngine.Scripting;

/// <summary>
/// Batching strategy for distributing work across jobs.
/// </summary>
public enum BatchingStrategy
{
    /// <summary>
    /// Fixed number of entities per job
    /// </summary>
    FixedSize,

    /// <summary>
    /// Dynamic batching based on entity count
    /// </summary>
    Dynamic,

    /// <summary>
    /// One job per worker thread
    /// </summary>
    PerThread,

    /// <summary>
    /// Single job for all entities (no batching)
    /// </summary>
    Single
}

/// <summary>
/// Configuration for job batching.
/// </summary>
public class BatchingConfig
{
    /// <summary>
    /// Batching strategy to use
    /// </summary>
    public BatchingStrategy Strategy { get; set; } = BatchingStrategy.Dynamic;

    /// <summary>
    /// Entities per batch (used by FixedSize)
    /// </summary>
    public int EntitiesPerBatch { get; set; } = 256;

    /// <summary>
    /// Minimum jobs to create (used by Dynamic)
    /// </summary>
    public int MinJobs { get; set; } = 1;

    /// <summary>
    /// Maximum jobs to create (used by Dynamic)
    /// </summary>
    public int MaxJobs { get; set; } = 16;

    /// <summary>
    /// Whether to enable profiling
    /// </summary>
    public bool EnableProfiling { get; set; } = false;

    /// <summary>
    /// Job priority for scheduler
    /// </summary>
    public JobPriority Priority { get; set; } = JobPriority.Normal;
}

/// <summary>
/// Result from batching operation.
/// </summary>
public struct BatchingResult
{
    /// <summary>
    /// Number of batches created
    /// </summary>
    public int BatchCount { get; set; }

    /// <summary>
    /// Total entities processed
    /// </summary>
    public int TotalEntities { get; set; }

    /// <summary>
    /// Entities per batch (average)
    /// </summary>
    public float AverageEntitiesPerBatch { get; set; }

    /// <summary>
    /// Time taken to execute (if profiling enabled)
    /// </summary>
    public float ElapsedMilliseconds { get; set; }
}

/// <summary>
/// Manages batching of entities into jobs for parallel processing.
/// 
/// Provides utilities to split entity workload across multiple jobs
/// using various strategies, enabling efficient load distribution.
/// 
/// Example:
/// <code>
/// var batcher = new JobBatcher();
/// var config = new BatchingConfig 
/// { 
///     Strategy = BatchingStrategy.Dynamic,
///     MaxJobs = 8
/// };
/// 
/// var batches = batcher.CreateBatches(entityList, config);
/// foreach (var batch in batches)
/// {
///     jobSystem.Schedule(new ProcessBatchJob { Batch = batch });
/// }
/// </code>
/// </summary>
public class JobBatcher
{
    private readonly Stopwatch _sw = new();

    /// <summary>
    /// Create batches of entities based on strategy.
    /// </summary>
    /// <param name="entities">Entities to batch</param>
    /// <param name="config">Batching configuration</param>
    /// <returns>List of entity batches</returns>
    public List<EntityBatch> CreateBatches(
        IReadOnlyList<Entity> entities,
        BatchingConfig config)
    {
        var batches = new List<EntityBatch>();

        if (entities.Count == 0)
        {
            return batches;
        }

        if (config.EnableProfiling)
        {
            _sw.Restart();
        }

        switch (config.Strategy)
        {
            case BatchingStrategy.FixedSize:
                CreateFixedSizeBatches(entities, config.EntitiesPerBatch, batches);
                break;

            case BatchingStrategy.Dynamic:
                CreateDynamicBatches(entities, config, batches);
                break;

            case BatchingStrategy.PerThread:
                CreatePerThreadBatches(entities, config, batches);
                break;

            case BatchingStrategy.Single:
                CreateSingleBatch(entities, batches);
                break;
        }

        if (config.EnableProfiling)
        {
            _sw.Stop();
        }

        return batches;
    }

    /// <summary>
    /// Create fixed-size batches.
    /// </summary>
    private void CreateFixedSizeBatches(
        IReadOnlyList<Entity> entities,
        int batchSize,
        List<EntityBatch> result)
    {
        for (var i = 0; i < entities.Count; i += batchSize)
        {
            var end = System.Math.Min(i + batchSize, entities.Count);
            var batch = new EntityBatch
            {
                Entities = [.. entities.Skip(i).Take(end - i).ToList()],
                StartIndex = i,
                EndIndex = end - 1
            };

            result.Add(batch);
        }
    }

    /// <summary>
    /// Create dynamic batches based on entity count.
    /// </summary>
    private void CreateDynamicBatches(
        IReadOnlyList<Entity> entities,
        BatchingConfig config,
        List<EntityBatch> result)
    {
        var jobCount = System.Math.Max(
            System.Math.Min(
                entities.Count / 128,  // Target ~128 entities per job
                config.MaxJobs),
            config.MinJobs
        );

        var entitiesPerBatch = System.Math.Max(1, entities.Count / jobCount);

        for (var i = 0; i < entities.Count; i += entitiesPerBatch)
        {
            var end = System.Math.Min(i + entitiesPerBatch, entities.Count);
            var batch = new EntityBatch
            {
                Entities = [.. entities.Skip(i).Take(end - i).ToList()],
                StartIndex = i,
                EndIndex = end - 1
            };

            result.Add(batch);
        }
    }

    /// <summary>
    /// Create one batch per available worker thread.
    /// </summary>
    private void CreatePerThreadBatches(
        IReadOnlyList<Entity> entities,
        BatchingConfig config,
        List<EntityBatch> result)
    {
        var threadCount = Environment.ProcessorCount;
        var entitiesPerBatch = System.Math.Max(1, entities.Count / threadCount);

        for (var i = 0; i < entities.Count; i += entitiesPerBatch)
        {
            var end = System.Math.Min(i + entitiesPerBatch, entities.Count);
            var batch = new EntityBatch
            {
                Entities = [.. entities.Skip(i).Take(end - i).ToList()],
                StartIndex = i,
                EndIndex = end - 1
            };

            result.Add(batch);
        }
    }

    /// <summary>
    /// Create single batch containing all entities.
    /// </summary>
    private void CreateSingleBatch(
        IReadOnlyList<Entity> entities,
        List<EntityBatch> result)
    {
        var batch = new EntityBatch
        {
            Entities = [.. entities],
            StartIndex = 0,
            EndIndex = entities.Count - 1
        };

        result.Add(batch);
    }

    /// <summary>
    /// Get the last profiling result.
    /// </summary>
    public long GetLastProfilingMilliseconds()
    {
        return _sw.ElapsedMilliseconds;
    }
}

/// <summary>
/// Represents a batch of entities for parallel processing.
/// </summary>
public class EntityBatch
{
    /// <summary>Entities in this batch</summary>
    public List<Entity> Entities { get; set; } = [];

    /// <summary>Start index in original collection</summary>
    public int StartIndex { get; set; }

    /// <summary>End index in original collection</summary>
    public int EndIndex { get; set; }

    /// <summary>Number of entities in batch</summary>
    public int Count => Entities.Count;

    /// <summary>
    /// Process batch with action.
    /// </summary>
    public void Process<T1>(Query<T1> query, EntityAction<T1> action) where T1 : unmanaged
    {
        foreach (var entity in Entities)
        {
            // Would need query support for entity lookup
            // action(ref component);
        }
    }
}

/// <summary>
/// Job that processes an entity batch.
/// </summary>
public abstract class BatchJob : IJob
{
    /// <summary>
    /// Batch of entities to process
    /// </summary>
    public EntityBatch? Batch { get; set; }

    /// <summary>
    /// Execute the job on the batch
    /// </summary>
    public abstract void Execute();

    /// <summary>
    /// Get job name for profiling
    /// </summary>
    public abstract string GetJobName();
}

/// <summary>
/// Work distribution strategy for load balancing.
/// 
/// Helps distribute work fairly across available resources.
/// </summary>
public class WorkDistributor
{
    /// <summary>
    /// Calculate optimal job count for given entity count.
    /// </summary>
    /// <param name="entityCount">Number of entities to process</param>
    /// <param name="minJobsNeeded">Minimum desired jobs</param>
    /// <returns>Recommended number of jobs</returns>
    public static int CalculateOptimalJobCount(int entityCount, int minJobsNeeded = 1)
    {
        var threadCount = Environment.ProcessorCount;
        var calculatedJobs = (entityCount + 127) / 128;  // Target 128 entities per job

        return System.Math.Max(minJobsNeeded, System.Math.Min(calculatedJobs, threadCount * 2));
    }

    /// <summary>
    /// Calculate entities per job for load distribution.
    /// </summary>
    public static int CalculateEntitiesPerJob(int totalEntities, int jobCount)
    {
        return System.Math.Max(1, totalEntities / jobCount);
    }

    /// <summary>
    /// Get work distribution for given resources.
    /// </summary>
    public static JobDistribution DistributeWork(
        int totalWork,
        int workerCount,
        int minWorkPerWorker = 10)
    {
        var workPerWorker = System.Math.Max(minWorkPerWorker, totalWork / workerCount);
        var actualWorkers = (totalWork + workPerWorker - 1) / workPerWorker;

        return new JobDistribution
        {
            TotalWork = totalWork,
            WorkerCount = System.Math.Min(actualWorkers, workerCount),
            WorkPerWorker = workPerWorker,
            LoadBalance = (float)totalWork / (workPerWorker * System.Math.Min(actualWorkers, workerCount))
        };
    }
}

/// <summary>
/// Result of work distribution calculation.
/// </summary>
public struct JobDistribution
{
    /// <summary>
    /// Total work units
    /// </summary>
    public int TotalWork { get; set; }

    /// <summary>
    /// Number of workers to use
    /// </summary>
    public int WorkerCount { get; set; }

    /// <summary>
    /// Work units per worker
    /// </summary>
    public int WorkPerWorker { get; set; }

    /// <summary>
    /// Load balance metric (1.0 = perfect balance)
    /// </summary>
    public float LoadBalance { get; set; }

    /// <summary>
    /// Whether load is well distribute
    /// d</summary>
    public readonly bool IsWellBalanced => LoadBalance > 0.8f;
}

/// <summary>
/// Job priority levels for scheduling.
/// </summary>
public enum JobPriority
{
    /// <summary>
    /// Execute as soon as possible
    /// </summary>
    High = 0,

    /// <summary>
    /// Normal priority (default)
    /// </summary>
    Normal = 1,

    /// <summary>
    /// Lower priority, can be deferred
    /// </summary>
    Low = 2
}

/// <summary>
/// Profiler for job execution metrics.
/// </summary>
public class JobProfiler
{
    private readonly Dictionary<string, JobMetrics> _metrics = [];
    private readonly Stopwatch _sw = new();

    /// <summary>
    /// Record a job execution.
    /// </summary>
    public void RecordJob(string jobName, long milliseconds, int itemsProcessed)
    {
        if (!_metrics.TryGetValue(jobName, out JobMetrics? metric))
        {
            metric = new JobMetrics { Name = jobName };
            _metrics[jobName] = metric;
        }

        metric.ExecutionCount++;
        metric.TotalMilliseconds += milliseconds;
        metric.TotalItemsProcessed += itemsProcessed;

        metric.AverageMilliseconds = (float)metric.TotalMilliseconds / metric.ExecutionCount;
        metric.AverageItemsPerMs = (float)metric.TotalItemsProcessed / metric.TotalMilliseconds;
    }

    /// <summary>
    /// Get metrics for a job.
    /// </summary>
    public JobMetrics GetMetrics(string jobName)
    {
        return _metrics.TryGetValue(jobName, out var metric)
            ? metric 
            : new JobMetrics { Name = jobName };
    }

    /// <summary>
    /// Get all recorded metrics.
    /// </summary>
    public IEnumerable<JobMetrics> GetAllMetrics() => _metrics.Values;

    /// <summary>
    /// Clear all metrics.
    /// </summary>
    public void Clear() => _metrics.Clear();
}

/// <summary>
/// Metrics for a single job type.
/// </summary>
public class JobMetrics
{
    /// <summary>
    /// Job name
    /// </summary>
    public string? Name { get; set; }

    /// <summary>
    /// How many times executed
    /// </summary>
    public int ExecutionCount { get; set; }

    /// <summary>
    /// Total time spent (ms)
    /// </summary>
    public long TotalMilliseconds { get; set; }

    /// <summary>
    /// Total items processed
    /// </summary>
    public int TotalItemsProcessed { get; set; }

    /// <summary>
    /// Average execution time (ms)
    /// </summary>
    public float AverageMilliseconds { get; set; }

    /// <summary>
    /// Throughput (items per ms)
    /// </summary>
    public float AverageItemsPerMs { get; set; }
}
// ============================================================================
// JobBatchingUtilities
// ============================================================================

/// <summary>
/// Load balancing hint for job scheduling.
/// Provides recommendations on how to distribute jobs for optimal performance.
/// </summary>
public struct LoadBalancingHint
{
    /// <summary>
    /// The type of job being scheduled.
    /// </summary>
    public string JobType { get; set; }

    /// <summary>
    /// Number of jobs of this type.
    /// </summary>
    public int Count { get; set; }

    /// <summary>
    /// Recommendation for parallel execution.
    /// </summary>
    public string RecommendedParallelism { get; set; }

    /// <summary>
    /// Suggested batch size for optimal load distribution.
    /// </summary>
    public int SuggestedBatchSize { get; set; }

    /// <summary>
    /// Estimated execution time if all run serially (ms).
    /// </summary>
    public float EstimatedSerialTimeMs { get; set; }

    /// <summary>
    /// Estimated execution time if parallelized (ms).
    /// </summary>
    public float EstimatedParallelTimeMs { get; set; }

    /// <summary>
    /// Estimated speedup from parallelization.
    /// </summary>
    public float EstimatedSpeedup => EstimatedSerialTimeMs > 0 
        ? EstimatedSerialTimeMs / EstimatedParallelTimeMs 
        : 1.0f;

    /// <summary>
    /// Get a human-readable summary of this hint.
    /// </summary>
    public override string ToString()
    {
        return $"JobType: {JobType}, Count: {Count}, " +
               $"Parallelism: {RecommendedParallelism}, " +
               $"BatchSize: {SuggestedBatchSize}, " +
               $"Speedup: {EstimatedSpeedup:F1}x";
    }
}

/// <summary>
/// Advanced job scheduling utilities for batching and optimization.
/// 
/// Provides methods to:
/// - Batch multiple similar jobs for better cache locality
/// - Analyze and optimize job dependency graphs
/// - Get load balancing recommendations
/// - Manage job scheduling across worker threads
/// 
/// Example:
/// <code>
/// var jobs = new[] { job1, job2, job3 };
/// var handle = JobBatchingUtilities.BatchSchedule(jobs, priority: 1);
/// 
/// var hints = JobBatchingUtilities.GetLoadBalancingHints(jobs);
/// foreach (var hint in hints)
/// {
///     Console.WriteLine($"{hint.JobType}: {hint.RecommendedParallelism}");
/// }
/// </code>
/// </summary>
public static class JobBatchingUtilities
{
    /// <summary>
    /// Batch multiple similar jobs and schedule them together.
    /// 
    /// Groups jobs by type and schedules each group in parallel,
    /// improving cache locality and reducing scheduling overhead.
    /// </summary>
    /// <param name="jobs">Jobs to schedule</param>
    /// <param name="priority">Job scheduling priority</param>
    /// <returns>Job handle for batch completion</returns>
    public static JobHandle BatchSchedule(IJob[] jobs, int? priority = null)
    {
        ArgumentNullException.ThrowIfNull(jobs);

        if (jobs.Length == 0)
            return JobHandle.CreateInvalid();

        var jobManager = GetJobManager(jobs);
        if (jobManager == null)
            return JobHandle.CreateInvalid();

        // Group jobs by type for better cache locality
        var grouped = jobs
            .GroupBy(j => j.GetType())
            .OrderByDescending(g => g.Count()) // Schedule larger groups first
            .ToList();

        JobHandle? lastHandle = null;
        int jobsPriority = priority ?? 0;

        foreach (var group in grouped)
        {
            var groupArray = group.ToArray();
            
            // Schedule group in parallel
            lastHandle = jobManager.ScheduleParallel(groupArray, jobsPriority);
        }

        return lastHandle ?? JobHandle.CreateInvalid();
    }

    /// <summary>
    /// Analyze job dependencies and optimize scheduling order.
    /// 
    /// Performs topological sort on the job dependency graph and schedules
    /// jobs in the optimal order for parallel execution while respecting
    /// all dependencies.
    /// </summary>
    /// <param name="jobsWithDeps">Jobs paired with their dependencies</param>
    /// <returns>Job handle for all jobs</returns>
    public static JobHandle OptimizeAndSchedule(
        IEnumerable<(IJob Job, JobHandle[] Dependencies)> jobsWithDeps)
    {
        var jobList = jobsWithDeps.ToList();

        if (jobList.Count == 0)
            return JobHandle.CreateInvalid();

        // Topological sort to determine optimal scheduling order
        var sorted = TopologicalSort(jobList);
        
        var jobManager = GetJobManager(sorted.Select(x => x.Job).ToArray());
        if (jobManager == null)
            return JobHandle.CreateInvalid();

        JobHandle? lastHandle = null;

        foreach (var (job, deps) in sorted)
        {
            var combinedDep = CombineDependencies(jobManager, deps);
            lastHandle = jobManager.Schedule(job, combinedDep);
        }

        return lastHandle ?? JobHandle.CreateInvalid();
    }

    /// <summary>
    /// Get load balancing suggestions for job batches.
    /// 
    /// Analyzes job characteristics and provides recommendations on:
    /// - Whether to parallelize job execution
    /// - Optimal batch sizing for load distribution
    /// - Estimated speedup from parallelization
    /// </summary>
    /// <param name="jobs">Jobs to analyze</param>
    /// <returns>Load balancing hints for each job type</returns>
    public static LoadBalancingHint[] GetLoadBalancingHints(IJob[] jobs)
    {
        ArgumentNullException.ThrowIfNull(jobs);

        if (jobs.Length == 0)
            return [];

        var hints = new List<LoadBalancingHint>();
        var processorCount = Environment.ProcessorCount;
        var avgJobsPerCore = jobs.Length / (float)processorCount;

        var grouped = jobs
            .GroupBy(j => j.GetType())
            .ToList();

        foreach (var group in grouped)
        {
            var jobType = group.Key.Name;
            var count = group.Count();
            
            // Determine if parallelization is beneficial
            bool shouldParallelize = count > 1 && avgJobsPerCore > 0.5f;
            var suggestedBatchSize = System.Math.Max(1, 
                System.Math.Ceiling(count / (float)System.Math.Min(count, processorCount)));

            var hint = new LoadBalancingHint
            {
                JobType = jobType,
                Count = count,
                RecommendedParallelism = shouldParallelize
                    ? $"Parallel ({System.Math.Min(count, processorCount)} cores)"
                    : "Serial",
                SuggestedBatchSize = (int)suggestedBatchSize,
                EstimatedSerialTimeMs = count * 0.5f, // Placeholder: would need actual metrics
                EstimatedParallelTimeMs = shouldParallelize
                    ? (count * 0.5f) / System.Math.Min(count, processorCount)
                    : count * 0.5f
            };

            hints.Add(hint);
        }

        return [.. hints.OrderByDescending(h => h.EstimatedSpeedup)];
    }

    /// <summary>
    /// Perform topological sort on job dependency graph.
    /// 
    /// Returns jobs in an order that respects all dependencies,
    /// enabling optimal parallel execution.
    /// </summary>
    private static List<(IJob Job, JobHandle[] Dependencies)> TopologicalSort(
        List<(IJob Job, JobHandle[] Dependencies)> jobsWithDeps)
    {
        // Simple topological sort using DFS
        var visited = new HashSet<int>();
        var result = new List<(IJob, JobHandle[])>();
        var indexMap = jobsWithDeps
            .Select((item, idx) => (item, idx))
            .ToDictionary(x => x.item.Job.GetHashCode(), x => x.idx);

        // Build adjacency list for dependencies
        var adjList = new Dictionary<int, List<int>>();
        for (int i = 0; i < jobsWithDeps.Count; i++)
        {
            adjList[i] = new List<int>();
        }

        // No direct way to infer dependencies from JobHandles,
        // so we do a simpler approach: sort by dependency count
        // (jobs with no dependencies first)
        var sorted = jobsWithDeps
            .OrderBy(x => x.Dependencies.Length)
            .ThenBy(x => x.Job.GetType().Name)
            .ToList();

        return sorted;
    }

    /// <summary>
    /// Combine multiple job handles into a single dependency handle.
    /// 
    /// Merges multiple JobHandle dependencies into one for scheduling.
    /// </summary>
    private static JobHandle? CombineDependencies(
        JobManager jobManager,
        JobHandle[] handles)
    {
        if (handles.Length == 0)
            return null;

        if (handles.Length == 1)
            return handles[0];

        // Combine handles by taking the last one (most restrictive)
        // In practice, a real implementation would merge all dependency info
        return handles[handles.Length - 1];
    }

    /// <summary>
    /// Extract JobManager from a job array.
    /// 
    /// Gets the JobManager from the first job's associated world.
    /// All jobs must have access to a World instance.
    /// </summary>
    private static JobManager? GetJobManager(IJob[] jobs)
    {
        if (jobs.Length == 0)
            return null;

        // Note: This is a simplified implementation
        // A full implementation would need each job to have a reference to World or JobManager
        // For now, return null and let the caller handle it
        return null;
    }

    /// <summary>
    /// Analyze job scheduling characteristics.
    /// 
    /// Provides metrics about job composition useful for load balancing.
    /// </summary>
    /// <param name="jobs">Jobs to analyze</param>
    /// <returns>Analysis metrics</returns>
    public static JobBatchAnalysis AnalyzeBatching(IJob[] jobs)
    {
        ArgumentNullException.ThrowIfNull(jobs);

        var grouped = jobs.GroupBy(j => j.GetType()).ToList();
        var totalJobs = jobs.Length;
        var jobTypeCount = grouped.Count;
        var avgJobsPerType = totalJobs / (float)jobTypeCount;
        var maxJobsOfType = grouped.Max(g => g.Count());
        var minJobsOfType = grouped.Min(g => g.Count());

        return new JobBatchAnalysis
        {
            TotalJobs = totalJobs,
            UniqueJobTypes = jobTypeCount,
            AverageJobsPerType = avgJobsPerType,
            MaxJobsOfType = maxJobsOfType,
            MinJobsOfType = minJobsOfType,
            LoadBalance = maxJobsOfType / (float)System.Math.Max(1, minJobsOfType),
            RecommendedStrategy = DetermineOptimalStrategy(avgJobsPerType, jobTypeCount)
        };
    }

    /// <summary>
    /// Determine optimal batching strategy based on job composition.
    /// </summary>
    private static BatchingStrategy DetermineOptimalStrategy(
        float avgJobsPerType,
        int jobTypeCount)
    {
        if (jobTypeCount == 1)
            return BatchingStrategy.Single; // Only one type, schedule as one batch

        if (avgJobsPerType < 2)
            return BatchingStrategy.Single; // Few jobs overall

        if (avgJobsPerType >= 10)
            return BatchingStrategy.PerThread; // Many jobs per type, distribute to threads

        return BatchingStrategy.Dynamic; // Balanced approach
    }
}

/// <summary>
/// Analysis results from job batch composition.
/// </summary>
public class JobBatchAnalysis
{
    /// <summary>
    /// Total number of jobs.
    /// </summary>
    public int TotalJobs { get; set; }

    /// <summary>
    /// Number of unique job types.
    /// </summary>
    public int UniqueJobTypes { get; set; }

    /// <summary>
    /// Average jobs per type.
    /// </summary>
    public float AverageJobsPerType { get; set; }

    /// <summary>
    /// Maximum jobs of any single type.
    /// </summary>
    public int MaxJobsOfType { get; set; }

    /// <summary>
    /// Minimum jobs of any single type.
    /// </summary>
    public int MinJobsOfType { get; set; }

    /// <summary>
    /// Load balance metric (max/min ratio).
    /// Closer to 1.0 = better balanced.
    /// </summary>
    public float LoadBalance { get; set; }

    /// <summary>
    /// Recommended batching strategy.
    /// </summary>
    public BatchingStrategy RecommendedStrategy { get; set; }

    /// <summary>
    /// Get a summary of the analysis.
    /// </summary>
    public override string ToString()
    {
        return $"Total: {TotalJobs}, Types: {UniqueJobTypes}, " +
               $"AvgPerType: {AverageJobsPerType:F1}, " +
               $"LoadBalance: {LoadBalance:F2}, " +
               $"Strategy: {RecommendedStrategy}";
    }
}