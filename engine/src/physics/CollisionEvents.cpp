/**
 * @Name: Dalton Koh, 2403250
 * @email: d.koh@digipen.edu
 * @file CollisionEvents.cpp
 * @brief Implementation of the collision event queue management system.
 *
 * @details
 * Implements the static methods for the CollisionEventQueue class, managing
 * global collision event storage through an entity-indexed hash map. The queue
 * serves as the bridge between the physics system (which generates collision
 * events each frame) and gameplay scripts (which query events to implement
 * custom collision responses).
 */
#include "physics/CollisionEvents.h"

namespace ECS {

    // Static member definition: Global hash map storing collision events per entity
    std::unordered_map<uint32_t, std::vector<CollisionEvent>> CollisionEventQueue::s_events;

    // Called at the start of each physics frame to wipe stale collision data
    void CollisionEventQueue::Clear() {
        s_events.clear();
    }

    // Stores the event in the hash map using the self entity's index as the key
    void CollisionEventQueue::AddEvent(const CollisionEvent& event) {
        s_events[event.SelfEntity.Index].push_back(event);
    }

    // Retrieve all collision events for a specific entity this frame.
    const std::vector<CollisionEvent>& CollisionEventQueue::GetEvents(Entity entity) {

        // Static empty vector for "no events" case
        static std::vector<CollisionEvent> empty;
        // O(1) lookup in hash map
        auto it = s_events.find(entity.Index);
        // Entity has no collisions this frame
        if (it == s_events.end()) return empty;
        // Return reference to event vector        
        return it->second;
    }

} // namespace ECS