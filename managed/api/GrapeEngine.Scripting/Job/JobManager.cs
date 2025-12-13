/* Start Header *****************************************************************/
/*!
\file   JobManager.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Managed wrapper for the C++ JobManager. Provides job scheduling and execution
from C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Unsafe;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting.Job;

/// <summary>
/// Configuration for the job system at runtime.
/// </summary>
public class JobSystemConfig
{
    /// <summary>Number of worker threads (0 = use hardware concurrency)</summary>
    public uint NumWorkerThreads { get; set; } = 0;

    /// <summary>Enable work stealing for load balancing</summary>
    public bool EnableWorkStealing { get; set; } = true;

    /// <summary>Enable profiling and timing data collection</summary>
    public bool EnableProfilingData { get; set; } = false;

    /// <summary>Enable strict dependency validation</summary>
    public bool ValidateDependencies { get; set; } = true;

    /// <summary>Default job priority (0 = normal)</summary>
    public int DefaultJobPriority { get; set; } = 0;
}

/// <summary>
/// Central manager for job scheduling, execution, and synchronization.
/// 
/// The JobManager handles:
/// - Scheduling jobs to worker threads
/// - Managing job dependencies
/// - Worker thread pool
/// - Job profiling and metrics
/// 
/// Access the JobManager through World.JobManager or ScriptHost.JobManager.
/// </summary>
public class JobManager
{
    private readonly nint _nativeJobManager;
    private readonly JobProfiler _profiler;

    /// <summary>
    /// Create a job manager (typically done by the engine).
    /// </summary>
    internal JobManager(nint nativeJobManager)
    {
        _nativeJobManager = nativeJobManager;
        _profiler = new JobProfiler(nativeJobManager);
    }

    /// <summary>
    /// Get the native job manager pointer for interop.
    /// </summary>
    internal nint NativePtr => _nativeJobManager;

    /// <summary>
    /// Get the job profiler for performance analysis.
    /// </summary>
    public JobProfiler Profiler => _profiler;

    // ============================================================================
    // Job Scheduling
    // ============================================================================

    /// <summary>
    /// Schedule a job to execute on the job system.
    /// 
    /// The job will be marshalled to the native job system and executed on a worker thread.
    /// </summary>
    /// <param name="job">The job to schedule</param>
    /// <param name="dependsOn">Optional job handle this job depends on</param>
    /// <param name="priority">Job priority (higher = earlier execution)</param>
    /// <returns>Handle to track job completion</returns>
    public JobHandle Schedule(IJob job, JobHandle? dependsOn = null, int priority = 0)
    {
        ArgumentNullException.ThrowIfNull(job);

        var adapter = new ManagedJobAdapter(job);
        
        unsafe
        {
            // Convert job name to null-terminated UTF-8 string
            var jobNameBytes = System.Text.Encoding.UTF8.GetBytes(adapter.JobName);
            fixed (byte* namePtr = jobNameBytes)
            {
                // Get dependency handle (null if no dependency)
                void* dependOnPtr = (void*)(dependsOn?.NativeHandle ?? 0);

                // Schedule the managed job
                nint resultHandle = JobSystemAPI.JobManagerScheduleManagedJob(
                    _nativeJobManager.ToPointer(),
                    adapter.GetNativeHandle(),
                    namePtr,
                    dependOnPtr,
                    priority
                );

                return new JobHandle(resultHandle);
            }
        }
    }

    /// <summary>
    /// Schedule multiple jobs to run in parallel.
    /// 
    /// All jobs in the batch will be scheduled together and may execute in parallel
    /// depending on their component access patterns.
    /// </summary>
    /// <param name="jobs">Jobs to schedule</param>
    /// <param name="priority">Job priority</param>
    /// <returns>Handle that completes when all jobs complete</returns>
    public JobHandle ScheduleParallel(IJob[] jobs, int priority = 0)
    {
        ArgumentNullException.ThrowIfNull(jobs);
        
        if (jobs.Length == 0)
            return JobHandle.CreateInvalid();

        // Create adapters for all jobs
        var adapters = new ManagedJobAdapter[jobs.Length];
        try
        {
            for (int i = 0; i < jobs.Length; i++)
            {
                adapters[i] = new ManagedJobAdapter(jobs[i]);
            }

            unsafe
            {
                // Prepare arrays for native call
                var handles = new nint[jobs.Length];
                var namePointers = new byte*[jobs.Length];
                var nameBytes = new byte[jobs.Length][];

                for (int i = 0; i < jobs.Length; i++)
                {
                    handles[i] = adapters[i].GetNativeHandle();
                    nameBytes[i] = System.Text.Encoding.UTF8.GetBytes(adapters[i].JobName);
                }

                fixed (nint* handlesPtr = handles)
                fixed (byte** namesPtr = namePointers)
                {
                    for (int i = 0; i < jobs.Length; i++)
                    {
                        fixed (byte* namePtr = nameBytes[i])
                        {
                            namePointers[i] = namePtr;
                        }
                    }

                    // Schedule batch
                    nint resultHandle = JobSystemAPI.JobManagerScheduleManagedJobBatch(
                        _nativeJobManager.ToPointer(),
                        handlesPtr,
                        namesPtr,
                        (uint)jobs.Length,
                        priority
                    );

                    return new JobHandle(resultHandle);
                }
            }
        }
        catch
        {
            // Cleanup adapters on error
            foreach (var adapter in adapters)
            {
                adapter?.Dispose();
            }
            throw;
        }
    }

    // ============================================================================
    // Synchronization
    // ============================================================================

    /// <summary>
    /// Wait for a specific job to complete.
    /// </summary>
    public void Complete(JobHandle handle)
    {
        handle?.Complete();
    }

    /// <summary>
    /// Wait for all scheduled jobs to complete.
    /// 
    /// This is typically called at frame boundaries to ensure all work
    /// is done before structural changes or rendering.
    /// </summary>
    public void CompleteAllJobs()
    {
        unsafe
        {
            JobSystemAPI.JobManagerCompleteAllJobs(_nativeJobManager.ToPointer());
        }
    }

    // ============================================================================
    // Job System State
    // ============================================================================

    /// <summary>
    /// Check if the job system is running.
    /// </summary>
    public bool IsRunning
    {
        get
        {
            unsafe
            {
                return JobSystemAPI.JobManagerIsRunning(_nativeJobManager.ToPointer());
            }
        }
    }

    /// <summary>
    /// Get the number of worker threads.
    /// </summary>
    public uint NumWorkerThreads
    {
        get
        {
            unsafe
            {
                return JobSystemAPI.JobManagerGetNumWorkerThreads(_nativeJobManager.ToPointer());
            }
        }
    }

    /// <summary>
    /// Get the number of pending jobs in the queue.
    /// </summary>
    public ulong PendingJobCount
    {
        get
        {
            unsafe
            {
                return JobSystemAPI.JobManagerGetPendingJobCount(_nativeJobManager.ToPointer());
            }
        }
    }

    // ============================================================================
    // Profiling
    // ============================================================================

    /// <summary>
    /// Enable or disable profiling.
    /// </summary>
    public bool ProfilingEnabled
    {
        get
        {
            unsafe
            {
                return JobSystemAPI.JobManagerIsProfilingEnabled(_nativeJobManager.ToPointer());
            }
        }
        set
        {
            unsafe
            {
                JobSystemAPI.JobManagerSetProfilingEnabled(_nativeJobManager.ToPointer(), value);
            }
        }
    }

    /// <summary>
    /// Reset all profiling data.
    /// </summary>
    public void ResetProfiler()
    {
        unsafe
        {
            JobSystemAPI.JobManagerResetProfiler(_nativeJobManager.ToPointer());
        }
    }

    /// <summary>
    /// Get a text report of job profiling statistics.
    /// </summary>
    public string GetProfilingReport()
    {
        unsafe
        {
            byte* reportPtr = JobSystemAPI.JobManagerGetProfilingReport(_nativeJobManager.ToPointer());
            if (reportPtr == null)
                return string.Empty;
            
            string report = Marshal.PtrToStringAnsi((nint)reportPtr) ?? "";
            JobSystemAPI.FreeString(reportPtr);
            return report;
        }
    }
}
