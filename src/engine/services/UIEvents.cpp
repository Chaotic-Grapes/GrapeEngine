#include "../engine/services/UIEvents.h"
#include "services/Input.h"

namespace ECS {

    std::unordered_map<uint32_t, std::vector<UIEvent>> UIEventQueue::s_events;

    void UIEventQueue::Clear() {
        s_events.clear();
    }

    void UIEventQueue::AddEvent(const UIEvent& event) {
        s_events[event.SelfEntity.Index].push_back(event);
    }

    const std::vector<UIEvent>& UIEventQueue::GetEvents(Entity entity) {
        static std::vector<UIEvent> empty;
        auto it = s_events.find(entity.Index);
        if (it == s_events.end()) return empty;
        return it->second;
    }

    bool UIEventQueue::WasClicked(Entity entity, int button) {
        const auto& events = GetEvents(entity);
        for (const auto& event : events) {
            if (event.Type == UIEventType::Click && event.Button == button) {
                return true;
            }
        }
        return false;
    }

    bool UIEventQueue::WasHoverEntered(Entity entity) {
        const auto& events = GetEvents(entity);
        for (const auto& event : events) {
            if (event.Type == UIEventType::HoverEnter) {
                return true;
            }
        }
        return false;
    }

    bool UIEventQueue::WasHoverExited(Entity entity) {
        const auto& events = GetEvents(entity);
        for (const auto& event : events) {
            if (event.Type == UIEventType::HoverExit) {
                return true;
            }
        }
        return false;
    }

} // namespace ECS