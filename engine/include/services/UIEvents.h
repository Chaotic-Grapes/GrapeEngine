#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#include "ecs/Entity.h"
#include "Math/Vector2D.h"
#include <vector>
#include <unordered_map>
#include "services/Input.h"

namespace ECS {

    enum class UIEventType {
        Click,
        HoverEnter,
        HoverExit
    };

    struct UIEvent {
        Entity SelfEntity;          // The UI entity this event is for
        UIEventType Type;           // What happened
        Vector2D ScreenPosition;    // Mouse position when event occurred
        int Button;                 // MOUSE_LEFT, MOUSE_RIGHT, etc. (-1 for hover events)
    };

    // Global UI event queue (filled by UIEventSystem each frame)
    // Works exactly like CollisionEventQueue
    class UIEventQueue {
    public:
        static void Clear();
        static void AddEvent(const UIEvent& event);
        static const std::vector<UIEvent>& GetEvents(Entity entity);

        // Convenience helpers for checking specific events
        static bool WasClicked(Entity entity, int button = MOUSE_LEFT);
        static bool WasHoverEntered(Entity entity);
        static bool WasHoverExited(Entity entity);

    private:
        // Map: Entity Index -> List of UI events for that entity this frame
        static std::unordered_map<uint32_t, std::vector<UIEvent>> s_events;
    };

} // namespace ECS

#endif // UI_EVENTS_H