/* Start Header *****************************************************************/
/*!
\file   BatchingResult.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Result from batching operation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Job.Batching;

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
