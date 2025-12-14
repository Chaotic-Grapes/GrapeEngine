/* Start Header *****************************************************************/
/*!
\file   StructuralChangeStats.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Statistics tracking for structural changes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges;

/// <summary>
/// Statistics tracking for structural changes.
/// </summary>
public class StructuralChangeStats
{
    /// <summary>
    /// Number of entities created
    /// </summary>
    public int EntitiesCreated { get; set; }

    /// <summary>
    /// Number of entities destroyed
    /// </summary>
    public int EntitiesDestroyed { get; set; }

    /// <summary>
    /// Number of components added
    /// </summary>
    public int ComponentsAdded { get; set; }

    /// <summary>
    /// Number of components removed
    /// </summary>
    public int ComponentsRemoved { get; set; }

    /// <summary>
    /// Number of component updates
    /// </summary>
    public int ComponentsUpdated { get; set; }

    /// <summary>
    /// Total time spent applying changes (ms)
    /// </summary>
    public long TotalTimeMs { get; set; }

    /// <summary>
    /// Get total structural changes
    /// </summary>
    public int GetTotalChanges()
    {
        return EntitiesCreated + EntitiesDestroyed + ComponentsAdded + 
               ComponentsRemoved + ComponentsUpdated;
    }

    /// <summary>
    /// Get readable summary
    /// </summary>
    public string GetSummary()
    {
        return $"Created: {EntitiesCreated}, Destroyed: {EntitiesDestroyed}, " +
               $"Added: {ComponentsAdded}, Removed: {ComponentsRemoved}, " +
               $"Updated: {ComponentsUpdated}, Time: {TotalTimeMs}ms";
    }
}
