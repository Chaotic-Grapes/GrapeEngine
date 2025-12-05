/* Start Header *****************************************************************/
/*!
\file   CollisionEvents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
High-level collision events API for C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting;

/// <summary>
/// Type of collision event
/// </summary>
public enum CollisionEventType
{
    /// <summary>
    /// Collision just started this frame
    /// </summary>
    Enter = 0,
    
    /// <summary>
    /// Collision is ongoing (started in a previous frame)
    /// </summary>
    Stay = 1,
    
    /// <summary>
    /// Collision just ended this frame
    /// </summary>
    Exit = 2
}

/// <summary>
/// Represents a single collision event
/// </summary>
public readonly struct CollisionEvent
{
    /// <summary>
    /// The entity this collision event is for
    /// </summary>
    public Entity Self { get; }
    
    /// <summary>
    /// The other entity involved in the collision
    /// </summary>
    public Entity Other { get; }
    
    /// <summary>
    /// Type of collision event (Enter, Stay, Exit)
    /// </summary>
    public CollisionEventType Type { get; }

    internal CollisionEvent(World world, Entity self, ulong otherEntityId, CollisionEventType type)
    {
        Self = self;
        Other = Entity.FromId(world, otherEntityId);
        Type = type;
    }

    /// <summary>
    /// Check if this is a collision enter event
    /// </summary>
    public bool IsEnter => Type == CollisionEventType.Enter;

    /// <summary>
    /// Check if this is a collision stay event
    /// </summary>
    public bool IsStay => Type == CollisionEventType.Stay;

    /// <summary>
    /// Check if this is a collision exit event
    /// </summary>
    public bool IsExit => Type == CollisionEventType.Exit;
}

/// <summary>
/// Provides access to collision events for entities
/// </summary>
public static class CollisionEvents
{
    /// <summary>
    /// Clear all collision events (typically called at start of physics update)
    /// </summary>
    public static unsafe void Clear(World world)
    {
        CollisionAPI.Clear(world.NativePtr);
    }

    /// <summary>
    /// Get the number of collision events for an entity this frame
    /// </summary>
    public static unsafe int GetEventCount(World world, Entity entity)
    {
        return (int)CollisionAPI.GetEventCount(world.NativePtr, entity.EntityId);
    }

    /// <summary>
    /// Get a specific collision event by index
    /// </summary>
    public static unsafe CollisionEvent? GetEvent(World world, Entity entity, int index)
    {
        ulong otherEntityId;
        int eventType;
        
        bool success = CollisionAPI.GetEvent(
            world.NativePtr,
            entity.EntityId,
            (uint)index,
            &otherEntityId,
            &eventType
        );

        if (!success)
            return null;
        return new CollisionEvent(world, entity, otherEntityId, (CollisionEventType)eventType);
    }

    /// <summary>
    /// Get all collision events for an entity this frame
    /// </summary>
    public static unsafe List<CollisionEvent> GetEvents(World world, Entity entity)
    {
        var events = new List<CollisionEvent>();
        uint count = CollisionAPI.GetEventCount(world.NativePtr, entity.EntityId);

        if (count == 0)
            return events;

        const int BufferSize = 32;

        // Allocate buffers once outside the loop to avoid stack overflow
        Span<ulong> otherEntityIds = stackalloc ulong[BufferSize];
        Span<int> eventTypes = stackalloc int[BufferSize];

        if (count <= BufferSize)
        {
            uint actualCount = 0;

            fixed (ulong* otherPtr = otherEntityIds)
            fixed (int* typePtr = eventTypes)
            {
                CollisionAPI.GetEventsBulk(
                    world.NativePtr,
                    entity.EntityId,
                    otherPtr,
                    typePtr,
                    count,
                    &actualCount
                );
            }

            for (int i = 0; i < actualCount; i++)
                events.Add(new CollisionEvent(world, entity, otherEntityIds[i], (CollisionEventType)eventTypes[i]));
        }
        else
        {
            uint remaining = count;
            uint offset = 0;

            while (remaining > 0)
            {
                uint batchSize = System.Math.Min(remaining, BufferSize);
                uint actualCount = 0;

                fixed (ulong* otherPtr = otherEntityIds)
                fixed (int* typePtr = eventTypes)
                {
                    CollisionAPI.GetEventsBulk(
                        world.NativePtr,
                        entity.EntityId,
                        otherPtr,
                        typePtr,
                        batchSize,
                        &actualCount
                    );
                }

                for (int i = 0; i < actualCount; i++)
                    events.Add(new CollisionEvent(world, entity, otherEntityIds[i], (CollisionEventType)eventTypes[i]));

                offset += actualCount;
                remaining -= actualCount;

                if (actualCount < batchSize)
                    break; // No more events
            }
        }

        return events;
    }

    /// <summary>
    /// Check if an entity has any collision with another specific entity this frame
    /// </summary>
    public static unsafe bool HasCollisionWith(World world, Entity entity, Entity other)
    {
        return CollisionAPI.HasCollisionWith(world.NativePtr, entity.EntityId, other.EntityId);
    }

    /// <summary>
    /// Get the type of collision event between two entities (Enter, Stay, Exit)
    /// Returns null if no collision exists between them
    /// </summary>
    public static unsafe CollisionEventType? GetCollisionType(World world, Entity entity, Entity other)
    {
        int type = CollisionAPI.GetCollisionType(world.NativePtr, entity.EntityId, other.EntityId);
        
        if (type < 0)
            return null;
        return (CollisionEventType)type;
    }

    /// <summary>
    /// Check if an entity is currently colliding with another (Enter or Stay events)
    /// </summary>
    public static bool IsCollidingWith(World world, Entity entity, Entity other)
    {
        var type = GetCollisionType(world, entity, other);
        return type == CollisionEventType.Enter || type == CollisionEventType.Stay;
    }
}
