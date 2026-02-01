/* Start Header *****************************************************************/
/*!
\file    GUIEventQueue.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Defines a lightweight event queue for GUI input events.

The queue allows GUI input processing to be decoupled from other systems
that may want to consume GUI events later in the frame.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_EVENT_QUEUE_H
#define GUI_EVENT_QUEUE_H

#include "ecs/Entity.h"
#include "math/Vector2D.h"
#include <cstdint>
#include <vector>

namespace ECS {
    namespace UI {

        enum class GUIEventType : uint8_t {
            HoverEntered,
            HoverExited,
            Pressed,
            Released,
            Clicked,
            Focused,
            Unfocused
        };

        struct GUIEvent {
            GUIEventType Type = GUIEventType::Pressed;
            Entity Target{NULL_ENTITY};
            Vector2D Position{0.0f, 0.0f};
        };

        class GUIEventQueue {
        public:
            void Clear() { m_events.clear(); }

            const std::vector<GUIEvent>& Events() const { return m_events; }

            void Push(GUIEventType type, Entity target, Vector2D position) {
                m_events.push_back(GUIEvent{type, target, position});
            }

        private:
            std::vector<GUIEvent> m_events;
        };

    } // namespace UI
} // namespace ECS

#endif
