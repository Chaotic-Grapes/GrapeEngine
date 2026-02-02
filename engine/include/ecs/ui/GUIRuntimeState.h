/* Start Header *****************************************************************/
/*!
\file    GUIRuntimeState.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Defines per-entity runtime UI state that should not live in serialized components.

This state is intended to be rebuilt each session and should not be persisted.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_RUNTIME_STATE_H
#define GUI_RUNTIME_STATE_H

#include "ecs/Entity.h"
#include <cstdint>
#include <unordered_map>

namespace ECS {
    namespace UI {

        struct GUIRuntimeState {
            bool Hovered = false;
            bool Pressed = false;
            bool Released = false;
            bool Focused = false;
            bool Dragging = false;
            float DragOffset = 0.0f;
            bool VerticalDragging = false;
            bool HorizontalDragging = false;
            float VerticalScrollVelocity = 0.0f;
            float HorizontalScrollVelocity = 0.0f;
        };

        using GUIRuntimeStateMap = std::unordered_map<uint32_t, GUIRuntimeState>;

    } // namespace UI
} // namespace ECS

#endif
