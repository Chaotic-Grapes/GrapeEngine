/* Start Header *****************************************************************/
/*!
\file   JobHandle.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Managed wrapper for job handles - used to track job completion and dependencies.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting.Job;

/// <summary>
/// Handle to a scheduled job for tracking completion and dependencies.
/// 
/// Use job handles to:
/// - Wait for a job to complete
/// - Schedule dependent jobs
/// - Track job status
/// </summary>
public class JobHandle
{
    private readonly nint _nativeHandle;
    private bool _isDisposed = false;

    /// <summary>
    /// Create a job handle from a native pointer.
    /// </summary>
    internal JobHandle(nint nativeHandle)
    {
        _nativeHandle = nativeHandle;
    }

    /// <summary>
    /// Get the native handle pointer for interop.
    /// </summary>
    internal nint NativeHandle => _nativeHandle;

    /// <summary>
    /// Check if this job has completed execution.
    /// </summary>
    public bool IsComplete
    {
        get
        {
            if (_isDisposed)
                return true;

            unsafe
            {
                return JobSystemAPI.JobHandleIsComplete(_nativeHandle.ToPointer());
            }
        }
    }

    /// <summary>
    /// Check if this is a valid job handle.
    /// </summary>
    public bool IsValid => _nativeHandle != nint.Zero && !_isDisposed;

    /// <summary>
    /// Block until this job completes.
    /// 
    /// This is a synchronous wait - the calling thread will block until
    /// the job finishes executing on a worker thread.
    /// </summary>
    public void Complete()
    {
        if (_isDisposed)
            return;
        
        unsafe
        {
            JobSystemAPI.JobHandleComplete(_nativeHandle.ToPointer());
        }
    }

    /// <summary>
    /// Wait for completion with a timeout.
    /// </summary>
    /// <param name="timeoutMs">Maximum milliseconds to wait</param>
    /// <returns>true if job completed, false if timeout</returns>
    public bool TryComplete(int timeoutMs)
    {
        if (_isDisposed)
            return true;
        
        unsafe
        {
            return JobSystemAPI.JobHandleTryComplete(_nativeHandle.ToPointer(), timeoutMs);
        }
    }

    /// <summary>
    /// Create an invalid/empty job handle.
    /// </summary>
    public static JobHandle CreateInvalid() => new(nint.Zero);

    public override string ToString()
    {
        return $"JobHandle(0x{_nativeHandle:X})";
    }

    public void Dispose()
    {
        if (!_isDisposed)
        {
            Complete();
            _isDisposed = true;
        }
    }
}
