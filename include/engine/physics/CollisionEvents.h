/**
 * @Name: Dalton Koh, 2403250
 * @email: d.koh@digipen.edu
 * @file CollisionEvents.h
 * @brief Collision event system interface for physics-to-script communication.
 *
 * @details
 * Defines the collision event data structures and management interface used to
 * communicate collision state changes from the physics system to gameplay scripts.
 * Provides three collision event types (Enter, Stay, Exit) representing the full
 * lifecycle of entity interactions, and a static event queue for efficient per-entity
 * collision lookup.
 */

#ifndef COLLISION_EVENTS_H
#define COLLISION_EVENTS_H

#include "ecs/Entity.h"
#include <vector>
#include <unordered_map>

namespace ECS {

    enum class CollisionEventType {
        Enter,  // Collision just started
        Stay,   // Collision ongoing
        Exit    // Collision just ended
    };

    struct CollisionEvent {
        Entity SelfEntity;      // The entity this event is for
        Entity OtherEntity;     // The entity we collided with
        CollisionEventType Type;
    };

    // Global collision event queue (filled by PhysicsSystem each frame)
    class CollisionEventQueue {
    public:
        static void Clear();
        static void AddEvent(const CollisionEvent& event);
        static const std::vector<CollisionEvent>& GetEvents(Entity entity);

    private:
        // Map: Entity -> List of collision events for that entity this frame
        static std::unordered_map<uint32_t, std::vector<CollisionEvent>> s_events;
    };

} // namespace ECS
#endif