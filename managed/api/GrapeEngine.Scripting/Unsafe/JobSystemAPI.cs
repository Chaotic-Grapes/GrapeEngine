/* Start Header *****************************************************************/
/*!
\file   JobSystemAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for job system interop functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for job system operations.
/// </summary>
internal static partial class JobSystemAPI
{
    // ============================================================================
    // Job Metrics Structure
    // ============================================================================

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct JobMetrics
    {
        public byte* JobNamePtr;                        // Pointer to job name string
        public long ExecutionTimeMicros;
        public long WaitTimeMicros;
        public uint ExecutingThreadId;
        public ulong EntitiesProcessed;
        [MarshalAs(UnmanagedType.Bool)]
        public bool WasStolen;
    }

    // ============================================================================
    // Job Type Statistics Structure
    // ============================================================================

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct JobTypeStats
    {
        public byte* JobNamePtr;                        // Pointer to job name string
        public ulong ExecutionCount;
        public long TotalExecutionTimeMicros;
        public long MinExecutionTimeMicros;
        public long MaxExecutionTimeMicros;
        public long AvgExecutionTimeMicros;
        public ulong StolenCount;
        public ulong EntitiesProcessedTotal;
    }

    // ============================================================================
    // Job Handle Operations
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_HandleIsComplete")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool JobHandleIsComplete(void* handlePtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_HandleComplete")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void JobHandleComplete(void* handlePtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_HandleTryComplete")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool JobHandleTryComplete(void* handlePtr, int timeoutMs);

    // ============================================================================
    // C# Managed Job Scheduling
    // ============================================================================

    /// <summary>
    /// Schedule a C# managed job on the native job system.
    /// The job handle (GCHandle) is passed to native code for lifetime management.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ScheduleManagedJob")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial nint JobManagerScheduleManagedJob(
        void* jobManagerPtr,
        nint managedJobHandle,      // GCHandle to C# IJob object
        byte* jobNamePtr,           // Job name for profiling
        void* dependsOnPtr,         // JobHandle dependency (null = no dependency)
        int priority                // Job priority
    );

    /// <summary>
    /// Schedule multiple C# managed jobs as a batch.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ScheduleManagedJobBatch")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial nint JobManagerScheduleManagedJobBatch(
        void* jobManagerPtr,
        nint* managedJobHandles,    // Array of GCHandles
        byte** jobNames,            // Array of job name pointers
        uint jobCount,              // Number of jobs
        int priority                // Job priority
    );

    // ============================================================================
    // Job Manager Operations
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ManagerCompleteAllJobs")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void JobManagerCompleteAllJobs(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ManagerIsRunning")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool JobManagerIsRunning(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ManagerGetNumWorkerThreads")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial uint JobManagerGetNumWorkerThreads(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ManagerGetPendingJobCount")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial ulong JobManagerGetPendingJobCount(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ManagerIsProfilingEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool JobManagerIsProfilingEnabled(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ManagerSetProfilingEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void JobManagerSetProfilingEnabled(void* jobManagerPtr, [MarshalAs(UnmanagedType.Bool)] bool enabled);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ManagerResetProfiler")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void JobManagerResetProfiler(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ManagerGetProfilingReport", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial byte* JobManagerGetProfilingReport(void* jobManagerPtr);

    // ============================================================================
    // Job Profiler Operations
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ProfilerIsEnabled")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static unsafe partial bool JobProfilerIsEnabled(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ProfilerGetStats", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial JobTypeStats JobProfilerGetStats(void* jobManagerPtr, string jobName);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ProfilerGetAllStats")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial JobTypeStats* JobProfilerGetAllStats(void* jobManagerPtr, uint* outCount);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ProfilerGetAllMetrics")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial JobMetrics* JobProfilerGetAllMetrics(void* jobManagerPtr, uint* outCount);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ProfilerReset")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void JobProfilerReset(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ProfilerMarkFrameStart")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void JobProfilerMarkFrameStart(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ProfilerMarkFrameEnd")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void JobProfilerMarkFrameEnd(void* jobManagerPtr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_ProfilerGenerateReport", StringMarshalling = StringMarshalling.Utf8)]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial byte* JobProfilerGenerateReport(void* jobManagerPtr);

    // ============================================================================
    // Memory Management
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_FreeString")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void FreeString(byte* ptr);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_FreeJobStatsArray")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void FreeJobStatsArray(JobTypeStats* ptr, uint count);

    [LibraryImport("GrapeEngineNative", EntryPoint = "JobInterop_FreeJobMetricsArray")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe partial void FreeJobMetricsArray(JobMetrics* ptr, uint count);

    // ============================================================================
    // Thread Affinity
    // ============================================================================

    [LibraryImport("GrapeEngineNative", EntryPoint = "ThreadAffinityInterop_IsSupported")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static partial bool ThreadAffinityIsSupported();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ThreadAffinityInterop_GetNumCores")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    public static partial uint ThreadAffinityGetNumCores();

    [LibraryImport("GrapeEngineNative", EntryPoint = "ThreadAffinityInterop_SetCurrentThreadAffinity")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static partial bool ThreadAffinitySetCurrentThreadAffinity(uint coreIndex);
}
