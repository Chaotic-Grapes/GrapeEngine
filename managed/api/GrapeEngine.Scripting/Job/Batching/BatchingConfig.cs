/* Start Header *****************************************************************/
/*!
\file   BatchingConfig.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Configuration for job batching.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Job.Batching;

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
