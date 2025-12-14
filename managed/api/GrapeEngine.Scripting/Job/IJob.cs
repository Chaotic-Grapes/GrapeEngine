/* Start Header *****************************************************************/
/*!
\file   IJob.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
C# interface for job system jobs. Provides base classes for defining parallel jobs
that can be scheduled on the job system's worker threads.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Job;

/// <summary>
/// Base interface for all jobs in the job system.
/// 
/// Implement this interface to define a unit of work that can be scheduled
/// and executed on worker threads. Jobs should be stateless or contain only
/// the data needed for execution.
/// 
/// For structural changes (adding/removing entities or components) during parallel 
/// execution, use the CommandBuffer property if provided. Do not directly modify 
/// the World to avoid race conditions.
/// </summary>
public interface IJob
{
    /// <summary>
    /// Optional command buffer for deferred structural changes.
    /// 
    /// When a job is scheduled via JobManager.Schedule(), a CommandBuffer is automatically
    /// provided if the job needs to modify entity/component structure. Use this buffer
    /// instead of directly modifying the World during job execution.
    /// 
    /// The buffer is automatically played back after the job completes, applying all
    /// recorded changes to the World.
    /// 
    /// Set automatically by JobManager - you only need to use it in Execute().
    /// </summary>
    CommandBuffer? Buffer { get; set; }

    /// <summary>
    /// Execute the job. Called by a worker thread from the job queue.
    /// 
    /// If you need to modify entity/component structure (create/destroy entities,
    /// add/remove components), use the Buffer property:
    /// <code>
    /// public void Execute()
    /// {
    ///     if (Buffer != null)
    ///     {
    ///         Buffer.CreateEntity();
    ///         Buffer.AddComponent&lt;Transform&gt;(entity, transform);
    ///     }
    /// }
    /// </code>
    /// 
    /// Do NOT call World methods that modify structure directly from parallel jobs.
    /// </summary>
    void Execute();

    /// <summary>
    /// Get the name of this job for profiling and debugging.
    /// </summary>
    string GetJobName() => GetType().Name;
}

/// <summary>
/// Job interface for processing individual entities in parallel.
/// 
/// Implement this to process entities matching a component signature.
/// The job system will automatically parallelize entity iteration.
/// 
/// Example:
/// <code>
/// public class UpdatePositionJob : IJobEntity
/// {
///     public float DeltaTime { get; set; }
///
///     public void Execute(Entity entity, ref Transform transform, in Velocity velocity)
///     {
///         transform.Position += velocity.Value * DeltaTime;
///     }
/// }
/// </code>
/// </summary>
public interface IJobEntity : IJob
{
    // Marker interface - actual implementation varies by entity signature
}

/// <summary>
/// Job interface for processing chunks in parallel.
/// 
/// Lower-level interface for processing entire chunks at once.
/// Useful for SIMD operations or when you need bulk access to component arrays.
/// </summary>
public interface IJobChunk : IJob
{
    /// <summary>
    /// Process a chunk of entities.
    /// </summary>
    /// <param name="chunkIndex">The index of this chunk</param>
    /// <param name="entityCount">Number of entities in this chunk</param>
    void ExecuteChunk(int chunkIndex, int entityCount);
}

/// <summary>
/// Job interface for parallel-for style iteration.
/// 
/// Use this when you need to parallelize iteration over a range of indices.
/// </summary>
public interface IJobParallelFor : IJob
{
    /// <summary>
    /// Process a single index in the parallel-for range.
    /// </summary>
    /// <param name="index">The index to process</param>
    void Execute(int index);
}

/// <summary>
/// Metadata about a job for the job system.
/// </summary>
public class JobMetadata
{
    public string Name { get; set; } = "";
    public int Priority { get; set; } = 0;
    public bool IsStolen { get; set; } = false;
}

/// <summary>
/// Specifies what components a job reads or writes.
/// Used for dependency resolution and conflict detection.
/// </summary>
[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
public class JobComponentAccessAttribute : Attribute
{
    public enum AccessType
    {
        /// <summary>Job only reads this component</summary>
        Read,
        /// <summary>Job writes this component exclusively</summary>
        Write,
        /// <summary>Job reads and writes this component</summary>
        ReadWrite
    }

    public Type[] ReadComponents { get; set; } = [];
    public Type[] WriteComponents { get; set; } = [];
    public Type[] ReadWriteComponents { get; set; } = [];
}
