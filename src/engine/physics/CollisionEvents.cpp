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