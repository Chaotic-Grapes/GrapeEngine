/* Start Header *****************************************************************/
/*!
\file   BatchDestroyOperation.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Safe batch operation for destroying multiple entities.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Core.StructuralChanges.Commands;

namespace GrapeEngine.Scripting.Core.StructuralChanges;

/// <summary>
/// Safe batch operation for destroying multiple entities.
/// 
/// Records all destroy operations as a batch to be applied
/// at the synchronization point.
/// </summary>
/// <remarks>
/// Create a batch destroy operation.
/// </remarks>
public class BatchDestroyOperation(CommandBuffer buffer)
{
    private readonly CommandBuffer _buffer = buffer ?? throw new ArgumentNullException(nameof(buffer));
    private readonly List<Entity> _entitiesToDestroy = [];

    /// <summary>
    /// Mark entity for destruction.
    /// </summary>
    public void Destroy(Entity entity)
    {
        _entitiesToDestroy.Add(entity);
    }

    /// <summary>
    /// Mark multiple entities for destruction.
    /// </summary>
    public void DestroyRange(params Entity[] entities)
    {
        _entitiesToDestroy.AddRange(entities);
    }

    /// <summary>
    /// Mark entities from enumerable for destruction.
    /// </summary>
    public void DestroyRange(IEnumerable<Entity> entities)
    {
        _entitiesToDestroy.AddRange(entities);
    }

    /// <summary>
    /// Execute all destroy operations.
    /// </summary>
    /// <returns>Number of entities marked for destruction</returns>
    public int Execute()
    {
        var count = _entitiesToDestroy.Count;

        foreach (var entity in _entitiesToDestroy)
        {
            _buffer.DestroyEntity(entity);
        }

        _entitiesToDestroy.Clear();
        return count;
    }

    /// <summary>
    /// Get count of pending destroy operations.
    /// </summary>
    public int GetPendingCount() => _entitiesToDestroy.Count;

    /// <summary>
    /// Clear all pending operations without executing.
    /// </summary>
    public void Clear()
    {
        _entitiesToDestroy.Clear();
    }
}
