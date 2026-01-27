/* Start Header *****************************************************************/
/*!
\file   SafeStructuralChangeExtensions.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Extension methods for safe structural changes using command buffers.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Internal.Query;
using GrapeEngine.Scripting.Core.StructuralChanges.Commands;

namespace GrapeEngine.Scripting.Core.StructuralChanges;

/// <summary>
/// Extension methods for safe structural changes using command buffers.
/// </summary>
public static class SafeStructuralChangeExtensions
{
    /// <summary>
    /// Safely destroy entities matching condition using a command buffer.
    /// </summary>
    public static void DestroyAllIf<T>(
        this Query<T> query,
        CommandBuffer buffer,
        Func<Entity, T, bool> shouldDestroy) where T : unmanaged
    {
        foreach (var (entity, component) in query)
        {
            if (shouldDestroy(entity, component))
            {
                buffer.DestroyEntity(entity);
            }
        }
    }

    /// <summary>
    /// Safely add component to entities matching condition.
    /// </summary>
    public static void AddComponentIf<T1, T2>(
        this Query<T1> query,
        CommandBuffer buffer,
        Func<Entity, T1, bool> condition,
        T2 component) where T1 : unmanaged where T2 : unmanaged
    {
        foreach (var (entity, comp) in query)
        {
            if (condition(entity, comp))
            {
                buffer.AddComponent(entity, component);
            }
        }
    }

    /// <summary>
    /// Safely remove component from entities matching condition.
    /// </summary>
    public static int RemoveComponentIf<T1, T2>(
        this Query<T1> query,
        CommandBuffer buffer,
        Func<Entity, T1, bool> condition) where T1 : unmanaged where T2 : unmanaged
    {
        var count = 0;

        foreach (var (entity, comp) in query)
        {
            if (condition(entity, comp))
            {
                buffer.RemoveComponent<T2>(entity);
                count++;
            }
        }

        return count;
    }

    /// <summary>
    /// Create a batch destroy operation for the query.
    /// </summary>
    public static BatchDestroyOperation GetBatchDestroyOperation<T>(
        this Query<T> query,
        CommandBuffer buffer) where T : unmanaged
    {
        return new BatchDestroyOperation(buffer);
    }

    /// <summary>
    /// Create a structural change builder.
    /// </summary>
    public static StructuralChangeBuilder CreateBuilder(this CommandBuffer buffer)
    {
        return new StructuralChangeBuilder(buffer);
    }
}

