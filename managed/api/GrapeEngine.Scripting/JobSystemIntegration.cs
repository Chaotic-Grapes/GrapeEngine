/* Start Header *****************************************************************/
/*!
\file   JobSystemIntegration.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Integration layer for scheduling system jobs and managing job dependencies.

Provides utilities for converting sequential entity iteration into scheduled jobs,
batching entities by chunk, and managing job dependencies within systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Job;
using GrapeEngine.Scripting.Query;

namespace GrapeEngine.Scripting;

/// <summary>
/// Result of a job execution with timing information.
/// </summary>
public struct JobExecutionResult
{
    /// <summary>
    /// Total time spent executing job
    /// </summary>
    public float ElapsedMilliseconds { get; set; }

    /// <summary>
    /// Number of entities processed
    /// </summary>
    public int EntitiesProcessed { get; set; }

    /// <summary>
    /// Whether job completed successfully
    /// </summary>
    public bool IsComplete { get; set; }

    /// <summary>
    /// Job handle for synchronization
    /// </summary>
    public JobHandle Handle { get; set; }
}

/// <summary>
/// Helper for executing system logic as parallel jobs.
/// 
/// Provides utilities to convert sequential entity iteration into
/// scheduled jobs on the job system, enabling parallelism and
/// better multi-core utilization.
/// 
/// Example:
/// <code>
/// public class MovementSystem : ISystem
/// {
///     private Query<Position, Velocity> m_query;
///     
///     public JobHandle OnUpdateAsJobs(World world, float deltaTime)
///     {
///         var helper = new JobSystemHelper(world);
///         return helper.ForEachEntity<Position, Velocity>(
///             m_query,
///             (ref Position pos, in Velocity vel) => {
///                 pos.Value += vel.Value * deltaTime;
///             }
///         );
///     }
/// }
/// </code>
/// </summary>
/// <remarks>
/// Create job system helper for a world.
/// </remarks>
/// <param name="world">The world to operate on</param>
public class JobSystemHelper(World world)
{
    private readonly World _world = world;
    private readonly JobManager _jobManager = world.JobManager;
    private readonly List<JobHandle> _pendingHandles = [];

    /// <summary>
    /// Get the underlying job manager for advanced scheduling.
    /// </summary>
    public JobManager JobManager => _jobManager;

    /// <summary>
    /// Apply the appropriate batching strategy based on configuration.
    /// 
    /// Handles all four batching strategies: FixedSize, Dynamic, PerThread, and Single.
    /// </summary>
    private JobHandle ApplyBatchingStrategy<T1>(
        Query<T1> query,
        EntityAction<T1> action,
        BatchingConfig config,
        JobHandle? dependsOn) where T1 : unmanaged
    {
        // Collect entities from query
        var entities = new List<Entity>();
        var enumerator = query.GetEnumerator();
        while (enumerator.MoveNext())
        {
            var (entity, _) = enumerator.Current;
            entities.Add(entity);
        }

        // If no entities or single strategy, schedule as single job
        if (entities.Count == 0 || config.Strategy == BatchingStrategy.Single)
        {
            var singleJob = new EntityIterationJob<T1>
            {
                Query = query,
                Action = action
            };
            return _jobManager.Schedule(singleJob, dependsOn ?? default);
        }

        // Apply batching strategy
        var batcher = new JobBatcher();
        var batches = batcher.CreateBatches(entities, config);

        // If only one batch, schedule as single job
        if (batches.Count == 1)
        {
            var singleJob = new EntityIterationJob<T1>
            {
                Query = query,
                Action = action,
                EntityBatch = batches[0]
            };
            return _jobManager.Schedule(singleJob, dependsOn ?? default);
        }

        // Schedule multiple jobs
        JobHandle lastHandle = dependsOn ?? default;
        foreach (var batch in batches)
        {
            var job = new EntityIterationJob<T1>
            {
                Query = query,
                Action = action,
                EntityBatch = batch
            };
            lastHandle = _jobManager.Schedule(job, lastHandle);
            _pendingHandles.Add(lastHandle);
        }

        return lastHandle;
    }

    /// <summary>
    /// Apply the appropriate batching strategy based on configuration for two components.
    /// </summary>
    private JobHandle ApplyBatchingStrategy<T1, T2>(
        Query<T1, T2> query,
        EntityAction<T1, T2> action,
        BatchingConfig config,
        JobHandle? dependsOn) where T1 : unmanaged where T2 : unmanaged
    {
        // Collect entities from query
        var entities = new List<Entity>();
        var enumerator = query.GetEnumerator();
        while (enumerator.MoveNext())
        {
            var (entity, _, _) = enumerator.Current;
            entities.Add(entity);
        }

        // If no entities or single strategy, schedule as single job
        if (entities.Count == 0 || config.Strategy == BatchingStrategy.Single)
        {
            var singleJob = new EntityIterationJob<T1, T2>
            {
                Query = query,
                Action = action
            };
            return _jobManager.Schedule(singleJob, dependsOn ?? default);
        }

        // Apply batching strategy
        var batcher = new JobBatcher();
        var batches = batcher.CreateBatches(entities, config);

        // If only one batch, schedule as single job
        if (batches.Count == 1)
        {
            var singleJob = new EntityIterationJob<T1, T2>
            {
                Query = query,
                Action = action,
                EntityBatch = batches[0]
            };
            return _jobManager.Schedule(singleJob, dependsOn ?? default);
        }

        // Schedule multiple jobs
        JobHandle lastHandle = dependsOn ?? default;
        foreach (var batch in batches)
        {
            var job = new EntityIterationJob<T1, T2>
            {
                Query = query,
                Action = action,
                EntityBatch = batch
            };
            lastHandle = _jobManager.Schedule(job, lastHandle);
            _pendingHandles.Add(lastHandle);
        }

        return lastHandle;
    }

    /// <summary>
    /// Apply the appropriate batching strategy based on configuration for three components.
    /// </summary>
    private JobHandle ApplyBatchingStrategy<T1, T2, T3>(
        Query<T1, T2, T3> query,
        EntityAction<T1, T2, T3> action,
        BatchingConfig config,
        JobHandle? dependsOn) where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged
    {
        // Collect entities from query
        var entities = new List<Entity>();
        var enumerator = query.GetEnumerator();
        while (enumerator.MoveNext())
        {
            var (entity, _, _, _) = enumerator.Current;
            entities.Add(entity);
        }

        // If no entities or single strategy, schedule as single job
        if (entities.Count == 0 || config.Strategy == BatchingStrategy.Single)
        {
            var singleJob = new EntityIterationJob<T1, T2, T3>
            {
                Query = query,
                Action = action
            };
            return _jobManager.Schedule(singleJob, dependsOn ?? default);
        }

        // Apply batching strategy
        var batcher = new JobBatcher();
        var batches = batcher.CreateBatches(entities, config);

        // If only one batch, schedule as single job
        if (batches.Count == 1)
        {
            var singleJob = new EntityIterationJob<T1, T2, T3>
            {
                Query = query,
                Action = action,
                EntityBatch = batches[0]
            };
            return _jobManager.Schedule(singleJob, dependsOn ?? default);
        }

        // Schedule multiple jobs
        JobHandle lastHandle = dependsOn ?? default;
        foreach (var batch in batches)
        {
            var job = new EntityIterationJob<T1, T2, T3>
            {
                Query = query,
                Action = action,
                EntityBatch = batch
            };
            lastHandle = _jobManager.Schedule(job, lastHandle);
            _pendingHandles.Add(lastHandle);
        }

        return lastHandle;
    }

    /// <summary>
    /// Schedule a job to execute a function for each entity in a query.
    /// 
    /// The function is scheduled as a parallel job and executed on worker threads.
    /// This is more efficient than sequential iteration for large entity counts.
    /// 
    /// Supports optional batching for better multi-core load distribution.
    /// </summary>
    /// <typeparam name="T1">First component type</typeparam>
    /// <param name="query">Query to iterate</param>
    /// <param name="action">Function to execute for each entity (ref parameters for writes)</param>
    /// <param name="dependsOn">Optional job to wait for before starting</param>
    /// <param name="batchingConfig">Optional batching configuration for load distribution</param>
    /// <returns>Job handle for synchronization</returns>
    /// 
    /// Example:
    /// <code>
    /// var handle = helper.ForEachEntity<Transform>(
    ///     query,
    ///     (ref Transform t) => t.Position *= 0.99f,  // Decay position
    ///     dependsOn: previousHandle,
    ///     batchingConfig: new BatchingConfig { Strategy = BatchingStrategy.Dynamic }
    /// );
    /// handle.Complete();  // Wait for completion
    /// </code>
    public JobHandle ForEachEntity<T1>(
        Query<T1> query,
        EntityAction<T1> action,
        JobHandle? dependsOn = default,
        BatchingConfig? batchingConfig = null) where T1 : unmanaged
    {
        // Use default configuration if not provided
        batchingConfig ??= new BatchingConfig();

        // Apply batching strategy
        return ApplyBatchingStrategy(query, action, batchingConfig, dependsOn);
    }

    /// <summary>
    /// Schedule a job for each entity with two components.
    /// 
    /// Supports optional batching for better multi-core load distribution.
    /// </summary>
    /// <param name="batchingConfig">Optional batching configuration for load distribution</param>
    public JobHandle ForEachEntity<T1, T2>(
        Query<T1, T2> query,
        EntityAction<T1, T2> action,
        JobHandle? dependsOn = default,
        BatchingConfig? batchingConfig = null) where T1 : unmanaged where T2 : unmanaged
    {
        // Use default configuration if not provided
        batchingConfig ??= new BatchingConfig();

        // Apply batching strategy
        return ApplyBatchingStrategy(query, action, batchingConfig, dependsOn);
    }

    /// <summary>
    /// Schedule a job for each entity with three components.
    /// 
    /// Supports optional batching for better multi-core load distribution.
    /// </summary>
    /// <param name="batchingConfig">Optional batching configuration for load distribution</param>
    public JobHandle ForEachEntity<T1, T2, T3>(
        Query<T1, T2, T3> query,
        EntityAction<T1, T2, T3> action,
        JobHandle? dependsOn = default,
        BatchingConfig? batchingConfig = null) 
        where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged
    {
        // Use default configuration if not provided
        batchingConfig ??= new BatchingConfig();

        // Apply batching strategy
        return ApplyBatchingStrategy(query, action, batchingConfig, dependsOn);
    }

    /// <summary>
    /// Schedule multiple jobs with dependencies between them.
    /// 
    /// Jobs are scheduled in order, with each job depending on the previous one.
    /// Useful for multi-stage processing pipelines.
    /// </summary>
    /// <param name="jobs">Array of jobs to schedule in sequence</param>
    /// <returns>Handle to the final job in the chain</returns>
    /// 
    /// Example:
    /// <code>
    /// var jobs = new IJob[] {
    ///     new UpdatePositionJob(),
    ///     new CheckCollisionsJob(),
    ///     new ApplyImpulsesJob()
    /// };
    /// var finalHandle = helper.ScheduleJobChain(jobs);
    /// finalHandle.Complete();  // Wait for all three
    /// </code>
    public JobHandle? ScheduleJobChain(params IJob[] jobs)
    {
        if (jobs.Length == 0)
        {
            return JobHandle.CreateInvalid();
        }

        JobHandle? handle = default;
        foreach (var job in jobs)
        {
            handle = _jobManager.Schedule(job, handle);
            _pendingHandles.Add(handle);
        }

        return handle;
    }

    /// <summary>
    /// Schedule multiple independent jobs to run in parallel.
    /// 
    /// All jobs are scheduled to run on worker threads concurrently.
    /// Useful when you have multiple independent operations.
    /// </summary>
    /// <param name="jobs">Jobs to schedule in parallel</param>
    /// <returns>Handle representing completion of all jobs</returns>
    /// 
    /// Example:
    /// <code>
    /// var jobs = new IJob[] {
    ///     new PhysicsUpdateJob(),
    ///     new AnimationUpdateJob(),
    ///     new ParticleUpdateJob()
    /// };
    /// var handle = helper.ScheduleJobsParallel(jobs);
    /// handle.Complete();
    /// </code>
    public JobHandle ScheduleJobsParallel(params IJob[] jobs)
    {
        if (jobs.Length == 0)
        {
            return JobHandle.CreateInvalid();
        }

        return _jobManager.ScheduleParallel(jobs);
    }

    /// <summary>
    /// Wait for a specific job to complete.
    /// 
    /// Blocks until the job finishes executing.
    /// This is safe to call from any thread.
    /// </summary>
    /// <param name="handle">Job handle to wait for</param>
    public void Complete(JobHandle handle)
    {
        handle.Complete();
    }

    /// <summary>
    /// Wait for all pending jobs to complete.
    /// 
    /// Blocks until all jobs scheduled through this helper finish.
    /// </summary>
    public void CompleteAll()
    {
        foreach (var handle in _pendingHandles)
        {
            handle.Complete();
        }
        _pendingHandles.Clear();
    }

    /// <summary>
    /// Get count of pending jobs.
    /// </summary>
    public int GetPendingJobCount()
    {
        return _pendingHandles.Count;
    }

    /// <summary>
    /// Clear all pending job handles.
    /// 
    /// Note: This does NOT cancel the jobs, just clears the tracking list.
    /// </summary>
    public void ClearPending()
    {
        _pendingHandles.Clear();
    }
}

/// <summary>
/// Action delegate for single-component entity iteration.
/// </summary>
/// <typeparam name="T1">Component type to modify</typeparam>
/// <param name="component">Component value (ref for writing)</param>
public delegate void EntityAction<T1>(ref T1 component) where T1 : unmanaged;

/// <summary>
/// Action delegate for two-component entity iteration.
/// </summary>
public delegate void EntityAction<T1, T2>(ref T1 component1, in T2 component2) 
    where T1 : unmanaged where T2 : unmanaged;

/// <summary>
/// Action delegate for three-component entity iteration.
/// </summary>
public delegate void EntityAction<T1, T2, T3>(ref T1 component1, in T2 component2, in T3 component3) 
    where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged;

/// <summary>
/// Job for iterating entities with a single component.
/// 
/// Supports batch-based execution when EntityBatch is set.
/// </summary>
internal class EntityIterationJob<T1> : IJob where T1 : unmanaged
{
    public required Query<T1> Query { get; set; }
    public EntityAction<T1>? Action { get; set; }
    public EntityBatch? EntityBatch { get; set; }

    public void Execute()
    {
        // Batch-based execution if EntityBatch is set
        if (EntityBatch != null && EntityBatch.Entities != null && EntityBatch.Entities.Count > 0)
        {
            // Get a fresh enumerator and skip to entities in this batch
            var batchSet = new HashSet<Entity>(EntityBatch.Entities);
            var enumerator = Query.GetEnumerator();
            
            while (enumerator.MoveNext())
            {
                var (entity, comp) = enumerator.Current;
                if (batchSet.Contains(entity))
                {
                    Action?.Invoke(ref comp);
                }
            }
        }
        else
        {
            // Non-batch execution (original behavior)
            var enumerator = Query.GetEnumerator();
            while (enumerator.MoveNext())
            {
                var (entity, comp) = enumerator.Current;
                Action?.Invoke(ref comp);
            }
        }
    }

    public string GetJobName() => $"EntityIteration<{typeof(T1).Name}>";
}

/// <summary>
/// Job for iterating entities with two components.
/// 
/// Supports batch-based execution when EntityBatch is set.
/// </summary>
internal class EntityIterationJob<T1, T2> : IJob where T1 : unmanaged where T2 : unmanaged
{
    public required Query<T1, T2> Query { get; set; }
    public EntityAction<T1, T2>? Action { get; set; }
    public EntityBatch? EntityBatch { get; set; }

    public void Execute()
    {
        // Batch-based execution if EntityBatch is set
        if (EntityBatch != null && EntityBatch.Entities != null && EntityBatch.Entities.Count > 0)
        {
            // Get a fresh enumerator and skip to entities in this batch
            var batchSet = new HashSet<Entity>(EntityBatch.Entities);
            var enumerator = Query.GetEnumerator();
            
            while (enumerator.MoveNext())
            {
                var (entity, comp1, comp2) = enumerator.Current;
                if (batchSet.Contains(entity))
                {
                    Action?.Invoke(ref comp1, in comp2);
                }
            }
        }
        else
        {
            // Non-batch execution (original behavior)
            var enumerator = Query.GetEnumerator();
            while (enumerator.MoveNext())
            {
                var (entity, comp1, comp2) = enumerator.Current;
                Action?.Invoke(ref comp1, in comp2);
            }
        }
    }

    public string GetJobName() => $"EntityIteration<{typeof(T1).Name},{typeof(T2).Name}>";
}

/// <summary>
/// Job for iterating entities with three components.
/// 
/// Supports batch-based execution when EntityBatch is set.
/// </summary>
internal class EntityIterationJob<T1, T2, T3> : IJob 
    where T1 : unmanaged where T2 : unmanaged where T3 : unmanaged
{
    public required Query<T1, T2, T3> Query { get; set; }
    public EntityAction<T1, T2, T3>? Action { get; set; }
    public EntityBatch? EntityBatch { get; set; }

    public void Execute()
    {
        // Batch-based execution if EntityBatch is set
        if (EntityBatch != null && EntityBatch.Entities != null && EntityBatch.Entities.Count > 0)
        {
            // Get a fresh enumerator and skip to entities in this batch
            var batchSet = new HashSet<Entity>(EntityBatch.Entities);
            var enumerator = Query.GetEnumerator();
            
            while (enumerator.MoveNext())
            {
                var (entity, comp1, comp2, comp3) = enumerator.Current;
                if (batchSet.Contains(entity))
                {
                    Action?.Invoke(ref comp1, in comp2, in comp3);
                }
            }
        }
        else
        {
            // Non-batch execution (original behavior)
            var enumerator = Query.GetEnumerator();
            while (enumerator.MoveNext())
            {
                var (entity, comp1, comp2, comp3) = enumerator.Current;
                Action?.Invoke(ref comp1, in comp2, in comp3);
            }
        }
    }

    public string GetJobName() => $"EntityIteration<{typeof(T1).Name},{typeof(T2).Name},{typeof(T3).Name}>";
}

/// <summary>
/// Simplifies implementation of systems that use job scheduling.
/// </summary>
/// <remarks>
/// Create a job system builder.
/// </remarks>
/// <param name="world">The world to operate on</param>
public class JobSystemBuilder(World world)
{
    private readonly World _world = world;
    private readonly JobSystemHelper _helper = new(world);
    private string _systemName = "JobSystem";
    private readonly List<(string Name, IJob Job, JobHandle? DependsOn)> _jobs = [];

    /// <summary>
    /// Set the system name (for profiling).
    /// </summary>
    public JobSystemBuilder WithName(string name)
    {
        _systemName = name;
        return this;
    }

    /// <summary>
    /// Add a job to the system.
    /// </summary>
    /// <param name="name">Job name (for profiling)</param>
    /// <param name="job">Job to schedule</param>
    /// <returns>This builder for chaining</returns>
    public JobSystemBuilder AddJob(string name, IJob job)
    {
        _jobs.Add((name, job, null));
        return this;
    }

    /// <summary>
    /// Add a job that depends on the previous job.
    /// </summary>
    public JobSystemBuilder ThenJob(string name, IJob job)
    {
        if (_jobs.Count == 0)
        {
            throw new InvalidOperationException("Cannot add dependent job with no previous job");
        }

        _jobs.Add((name, job, null));  // Dependency set during Build()
        return this;
    }

    /// <summary>
    /// Build and schedule all jobs.
    /// </summary>
    /// <returns>Handle to final job in chain</returns>
    public JobHandle? Build()
    {
        if (_jobs.Count == 0)
        {
            return JobHandle.CreateInvalid();
        }

        JobHandle? handle = default;
        foreach (var (name, job, _) in _jobs)
        {
            handle = _helper.JobManager.Schedule(job, handle);
        }

        return handle;
    }

    /// <summary>
    /// Get the job helper for manual control.
    /// </summary>
    public JobSystemHelper GetHelper() => _helper;
}
