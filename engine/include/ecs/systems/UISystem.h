/* Start Header *****************************************************************/
/*!
\file   UISystem.h
\author Choi Meng Yew
\date   31st October 2025
\brief
Entity Component System (ECS) module responsible for handling user interface
interactions and button logic. The UISystem manages input detection, button
state updates, and triggers registered UI actions.

Responsibilities:
- Initialize and update all UI-related ECS entities
- Detect mouse interactions with UIButton components
- Manage button state transitions (hover, press, release)
- Execute registered callbacks through the action registry
- Support dynamic action binding via unique action IDs

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once
#include "ecs/World.h"

#include <unordered_map>
#include <functional>

namespace ECS {

    class UISystem {
    public:
        // Function pointer type for actions (can use std::function since UISystem is not a component)
        using ActionCallback = std::function<void(World&)>;

        void Initialize(World& world);
        void Update(World& world, float dt);

        // Register an action by ID
        void RegisterAction(uint32_t actionID, ActionCallback callback);

    private:
        // Process all UIButton components
        void ProcessButtons(World& world);

        // Update button visuals based on state
        void UpdateButtonVisuals(World& world);

        // Check if mouse is inside button bounds (screen-space pixels)
        bool IsMouseInButton(float mouseX, float mouseY,
            float btnX, float btnY,
            float btnW, float btnH) const;

        // Mouse state tracking
        bool m_wasMouseDown = false;

        // Action registry: ActionID -> callback
        std::unordered_map<uint32_t, ActionCallback> m_actionRegistry;
    };

} // namespace ECS
