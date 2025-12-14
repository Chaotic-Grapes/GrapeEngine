/* Start Header *****************************************************************/
/*!
\file   ManagedJobAdapter.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Adapter layer that bridges C# IJob objects to the native C++ job system.
Manages GCHandle lifecycle and job marshalling.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting.Job;

/// <summary>
/// Marshals a C# IJob object to the native job system.
/// Manages the GCHandle lifetime and provides native interop interface.
/// </summary>
internal class ManagedJobAdapter : IDisposable
{
    private GCHandle _jobHandle;
    private nint _nativeJobPtr;
    private bool _isDisposed;

    /// <summary>
    /// Create an adapter for a C# job object.
    /// </summary>
    public ManagedJobAdapter(IJob job)
    {
        ArgumentNullException.ThrowIfNull(job, nameof(job));

        Job = job;
        JobName = JobReflectionHelper.GetJobName(job);
        ComponentAccess = JobReflectionHelper.GetComponentAccess(job.GetType());

        // Pin the C# job object in managed memory so GC doesn't move it
        _jobHandle = GCHandle.Alloc(job, GCHandleType.Normal);

        // Create native wrapper
        _nativeJobPtr = nint.Zero;
    }

    public IJob Job { get; }
    public string JobName { get; }
    public ComponentAccessInfo ComponentAccess { get; }

    /// <summary>
    /// Get the native pointer for this managed job (for passing to C++).
    /// </summary>
    internal nint GetNativeHandle()
    {
        ObjectDisposedException.ThrowIf(_isDisposed, nameof(ManagedJobAdapter));

        // Return the GCHandle as an IntPtr (safe for native storage)
        return GCHandle.ToIntPtr(_jobHandle);
    }

    /// <summary>
    /// Execute the job immediately (used for synchronous testing).
    /// </summary>
    public void ExecuteImmediate()
    {
        ObjectDisposedException.ThrowIf(_isDisposed, nameof(ManagedJobAdapter));

        try
        {
            Job.Execute();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Job {JobName} execution failed: {ex}");
            throw;
        }
    }

    /// <summary>
    /// Static helper to execute a job from a native GCHandle pointer.
    /// Called by native code.
    /// </summary>
    [UnmanagedCallersOnly(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    public static void ExecuteJobCallback(nint jobHandlePtr)
    {
        try
        {
            nint handle = jobHandlePtr;
            if (GCHandle.FromIntPtr(handle).IsAllocated)
            {
                var job = (IJob?)GCHandle.FromIntPtr(handle).Target;
                job?.Execute();
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Managed job execution failed: {ex}");
        }
    }

    public override string ToString()
    {
        return $"ManagedJobAdapter({JobName})";
    }

    public void Dispose()
    {
        if (_isDisposed)
            return;

        if (_jobHandle.IsAllocated)
        {
            _jobHandle.Free();
        }

        _isDisposed = true;
    }
}

/// <summary>
/// Manages a pool of job adapters to minimize allocations.
/// </summary>
internal class JobAdapterPool
{
    private readonly Stack<ManagedJobAdapter> _pool = new();
    private int _createdCount = 0;

    public ManagedJobAdapter Rent(IJob job)
    {
        // Note: Simple pool that creates adapters on demand
        // More complex pooling strategies could be implemented
        _createdCount++;
        return new ManagedJobAdapter(job);
    }

    public void Return(ManagedJobAdapter adapter)
    {
        adapter?.Dispose();
    }

    public int CreatedCount => _createdCount;
    public int PooledCount => _pool.Count;

    public void Clear()
    {
        while (_pool.Count > 0)
        {
            var adapter = _pool.Pop();
            adapter.Dispose();
        }
    }
}

/// <summary>
/// Callback signature for executing a managed job.
/// </summary>
internal delegate void ExecuteJobDelegate(nint jobHandlePtr);
