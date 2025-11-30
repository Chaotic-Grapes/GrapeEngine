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
#include "../engine/physics/CollisionEvents.h"

namespace ECS {

    std::unordered_map<uint32_t, std::vector<CollisionEvent>> CollisionEventQueue::s_events;

    void CollisionEventQueue::Clear() {
        s_events.clear();
    }

    void CollisionEventQueue::AddEvent(const CollisionEvent& event) {
        s_events[event.SelfEntity.Index].push_back(event);
    }

    const std::vector<CollisionEvent>& CollisionEventQueue::GetEvents(Entity entity) {
        static std::vector<CollisionEvent> empty;
        auto it = s_events.find(entity.Index);
        if (it == s_events.end()) return empty;
        return it->second;
    }

} // namespace ECS