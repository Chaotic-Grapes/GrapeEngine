/* Start Header *****************************************************************/
/*!
\file   StructuralChangePatterns.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Common patterns for structural change scenarios.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Query;
using GrapeEngine.Scripting.Core.StructuralChanges.Commands;

namespace GrapeEngine.Scripting.Core.StructuralChanges;

/// <summary>
/// Patterns for common structural change scenarios.
/// </summary>
public static class StructuralChangePatterns
{
    /// <summary>
    /// Pattern: Destroy all entities matching a condition.
    /// 
    /// Safe pattern for removing entities from a parallel job.
    /// </summary>
    public static int DestroyAllMatching<T>(
        Query<T> query,
        CommandBuffer buffer,
        Func<Entity, bool> predicate) where T : unmanaged
    {
        var destroyedCount = 0;

        // Record destroy operations (don't execute yet)
        foreach (var (entity, _) in query)
        {
            if (predicate(entity))
            {
                buffer.DestroyEntity(entity);
                destroyedCount++;
            }
        }

        return destroyedCount;
    }

    /// <summary>
    /// Pattern: Transform entities by modifying components.
    /// 
    /// Safe pattern for complex entity modifications that require
    /// removing old components and adding new ones.
    /// </summary>
    public static void TransformEntities<T1, T2>(
        Query<T1> query,
        CommandBuffer buffer,
        Func<T1, T2> transformer) where T1 : unmanaged where T2 : unmanaged
    {
        foreach (var (entity, oldComp) in query)
        {
            var newComp = transformer(oldComp);

            buffer.RemoveComponent<T1>(entity);
            buffer.AddComponent(entity, newComp);
        }
    }

    /// <summary>
    /// Pattern: Spawn entities in bulk.
    /// 
    /// Safe pattern for creating many entities from a job.
    /// </summary>
    public static List<Entity> SpawnBulk(
        CommandBuffer buffer,
        int count,
        uint archetypeId)
    {
        var spawnedEntities = new List<Entity>(count);

        for (int i = 0; i < count; i++)
        {
            var entity = buffer.Instantiate(archetypeId);
            spawnedEntities.Add(entity);
        }

        return spawnedEntities;
    }

    /// <summary>
    /// Pattern: Clone entities matching a condition.
    /// 
    /// Safe pattern for duplicating entities from a parallel job.
    /// </summary>
    public static int CloneMatching<T>(
        Query<T> query,
        CommandBuffer buffer,
        Func<Entity, bool> predicate) where T : unmanaged
    {
        int clonedCount = 0;

        foreach (var (entity, _) in query)
        {
            if (predicate(entity))
            {
                buffer.Clone(entity);
                clonedCount++;
            }
        }

        return clonedCount;
    }

    /// <summary>
    /// Pattern: Staged destruction with validation.
    /// 
    /// Safe pattern for destroying entities with pre-check validation.
    /// </summary>
    public static int DestroyIfValid<T>(
        Query<T> query,
        CommandBuffer buffer,
        Func<Entity, T, bool> isValid) where T : unmanaged
    {
        int destroyedCount = 0;

        foreach (var (entity, component) in query)
        {
            if (!isValid(entity, component))
            {
                buffer.DestroyEntity(entity);
                destroyedCount++;
            }
        }

        return destroyedCount;
    }

    /// <summary>
    /// Pattern: Conditional component swapping.
    /// 
    /// Safe pattern for replacing components conditionally.
    /// </summary>
    public static int SwapComponentsIf<T1, T2>(
        Query<T1> query,
        CommandBuffer buffer,
        Func<Entity, T1, bool> shouldSwap,
        Func<T1, T2> converter) where T1 : unmanaged where T2 : unmanaged
    {
        int swappedCount = 0;

        foreach (var (entity, oldComp) in query)
        {
            if (shouldSwap(entity, oldComp))
            {
                var newComp = converter(oldComp);
                buffer.RemoveComponent<T1>(entity);
                buffer.AddComponent(entity, newComp);
                swappedCount++;
            }
        }

        return swappedCount;
    }
}
