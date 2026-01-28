/* Start Header *****************************************************************/
/*!
\file   StructuralChangePatterns.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Common patterns and utilities for safe structural changes using command buffers.

Provides fluent APIs, batch operations, and safety-checked patterns
for structural modifications through deferred command execution.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Internal.Query;
using GrapeEngine.Scripting.Core.StructuralChanges.Commands;

namespace GrapeEngine.Scripting.Core.StructuralChanges;

/// <summary>
/// Fluent builder for recording structural changes.
/// 
/// Provides a convenient way to queue multiple related changes
/// and batch them together.
/// </summary>
/// <remarks>
/// Create a structural change builder.
/// </remarks>
public class StructuralChangeBuilder(CommandBuffer buffer)
{
    private readonly CommandBuffer _buffer = buffer ?? throw new ArgumentNullException(nameof(buffer));
    private readonly List<Action> _changes = [];
    private string _batchName = string.Empty;

    /// <summary>
    /// Set batch name for tracking.
    /// </summary>
    public StructuralChangeBuilder WithBatchName(string name)
    {
        _batchName = name;
        return this;
    }

    /// <summary>
    /// Add change to create entity.
    /// </summary>
    public StructuralChangeBuilder CreateEntity(out Entity createdEntity)
    {
        createdEntity = _buffer.CreateEntity();
        return this;
    }

    /// <summary>
    /// Add change to destroy entity.
    /// </summary>
    public StructuralChangeBuilder DestroyEntity(Entity entity)
    {
        _changes.Add(() => _buffer.DestroyEntity(entity));
        return this;
    }

    /// <summary>
    /// Add change to add component.
    /// </summary>
    public StructuralChangeBuilder AddComponent<T>(Entity entity, T component) where T : unmanaged
    {
        _changes.Add(() => _buffer.AddComponent(entity, component));
        return this;
    }

    /// <summary>
    /// Add change to remove component.
    /// </summary>
    public StructuralChangeBuilder RemoveComponent<T>(Entity entity) where T : unmanaged
    {
        _changes.Add(() => _buffer.RemoveComponent<T>(entity));
        return this;
    }

    /// <summary>
    /// Build and record all changes.
    /// </summary>
    /// <returns>Number of changes recorded</returns>
    public int Build()
    {
        int count = 0;
        foreach (var change in _changes)
        {
            change();
            count++;
        }

        _changes.Clear();
        return count;
    }

    /// <summary>
    /// Get count of pending changes.
    /// </summary>
    public int GetPendingChangeCount() => _changes.Count;
}

