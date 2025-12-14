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
