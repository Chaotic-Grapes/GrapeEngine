/* Start Header *****************************************************************/
/*!
\file   CollisionEvents.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   26th November 2025
\brief
Managed representation of collision events for scripting code.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.ScriptAPI.Unsafe;
using GrapeEngine.Scripting;
using GrapeEngine.Math;

namespace GrapeEngine.Events;

/// <summary>
/// Managed representation of a collision event for scripting code.
/// </summary>
public readonly struct CollisionEvent
{
    public Entity Other { get; }
    public CollisionEventType Type { get; }

    public CollisionEvent(ulong otherEntityId, CollisionEventType type)
    {
        Other = Entity.FromId(otherEntityId);
        Type = type;
    }
}

/// <summary>
/// Public helper to access collision events in a safe, managed way.
/// This uses the internal Unsafe CollisionAPI but exposes a friendly API
/// for game scripts.
/// </summary>
public static class CollisionEvents
{
    /// <summary>
    /// Get collision events for an entity for the current frame.
    /// </summary>
    /// <param name="entity">Entity wrapper</param>
    /// <returns>List of collision events (may be empty)</returns>
    public static List<CollisionEvent> GetEvents(Entity entity)
    {
        var list = new List<CollisionEvent>();
        uint total = CollisionAPI.GetEventCount(entity.EntityId);
        if (total == 0) return list;

        const int BUFFER = 32;
        if (total <= BUFFER)
        {
            var others = new ulong[BUFFER];
            var types = new int[BUFFER];
            uint got = CollisionAPI.GetEventsBulk(entity.EntityId, others, types, (uint)BUFFER);
            var toProcess = (int)GMath.Min(got, (uint)BUFFER);

            for (var i = 0; i < toProcess; ++i)
            {
                list.Add(new CollisionEvent(others[i], (CollisionEventType)types[i]));
            }
        }
        else
        {
            // First batch
            var others = new ulong[BUFFER];
            var types = new int[BUFFER];
            uint got = CollisionAPI.GetEventsBulk(entity.EntityId, others, types, (uint)BUFFER);
            var toProcess = (int)GMath.Min(got, (uint)BUFFER);

            for (var i = 0; i < toProcess; ++i)
            {
                list.Add(new CollisionEvent(others[i], (CollisionEventType)types[i]));
            }

            // Remaining
            var remaining = (int)(total - got);
            if (remaining > 0)
            {
                var moreOthers = new ulong[remaining];
                var moreTypes = new int[remaining];
                uint gotMore = CollisionAPI.GetEventsBulk(entity.EntityId, moreOthers, moreTypes, (uint)remaining);
                var gotMoreCount = (int)GMath.Min(gotMore, (uint)remaining);

                for (var i = 0; i < gotMoreCount; ++i)
                {
                    list.Add(new CollisionEvent(moreOthers[i], (CollisionEventType)moreTypes[i]));
                }
            }
        }

        return list;
    }
}
