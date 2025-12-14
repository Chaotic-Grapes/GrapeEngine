/* Start Header *****************************************************************/
/*!
\file   BatchingStrategy.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Batching strategy for distributing work across jobs.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Job.Batching;

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
