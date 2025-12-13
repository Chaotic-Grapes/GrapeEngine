/* Start Header *****************************************************************/
/*!
\file   ThreadAffinity.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Managed wrapper for thread affinity utilities - bind threads to CPU cores
for better cache locality and performance.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting.Job;

/// <summary>
/// Utilities for managing thread affinity.
/// 
/// Thread affinity binds a thread to specific CPU cores, which can improve
/// performance by keeping thread data in the core's cache and reducing
/// context switching overhead.
/// 
/// Thread affinity is automatically configured by the JobManager, but this
/// class can be used for custom thread management.
/// </summary>
public static class ThreadAffinity
{
    /// <summary>
    /// Check if thread affinity is supported on this platform.
    /// </summary>
    public static bool IsSupported => JobSystemAPI.ThreadAffinityIsSupported();

    /// <summary>
    /// Get the number of available CPU cores.
    /// </summary>
    public static uint NumCores => JobSystemAPI.ThreadAffinityGetNumCores();

    /// <summary>
    /// Bind the current thread to a specific CPU core.
    /// </summary>
    /// <param name="coreIndex">The core to bind to (0-based)</param>
    /// <returns>true if successful, false if not supported or invalid core</returns>
    public static bool SetCurrentThreadAffinity(uint coreIndex)
    {
        if (!IsSupported || coreIndex >= NumCores) return false;
        
        return JobSystemAPI.ThreadAffinitySetCurrentThreadAffinity(coreIndex);
    }

    /// <summary>
    /// Get suggested thread affinity for a worker thread index.
    /// </summary>
    /// <param name="threadIndex">The worker thread index</param>
    /// <returns>Suggested CPU core (wraps around if needed)</returns>
    public static uint GetSuggestedAffinity(uint threadIndex)
    {
        return threadIndex % NumCores;
    }
}
