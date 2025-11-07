/* Start Header *****************************************************************/
/*!
\file   UISystem.cpp
\author Choi Meng Yew
\date   31st October 2025
\brief
Implementation of the UISystem, which manages user interface interaction
within the ECS. Handles per-frame updates of UIButton components, detecting
mouse hover, press, and release states, and triggering registered actions.

Responsibilities:
- Poll and process mouse input to determine UI interaction states
- Detect button clicks and execute registered callbacks through the action registry
- Update visual feedback (button colors) based on interaction states
- Maintain consistent bottom-left coordinate handling for screen-space logic

The UISystem acts as the bridge between low-level input handling and
high-level UI feedback, enabling interactive UI components without
requiring manual input polling in gameplay systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/UISystem.h"
#include "ecs/Components.h"
#include "services/Input.h"
#include "services/WindowManager.h"
#include "core/Logger.h"
#include <glm/vec2.hpp>

namespace ECS {

    void UISystem::Initialize(World& world) {
        (void)world; // unused for now
        LOG_INFO("UISystem initialized");
    }

    void UISystem::Update(World& world, float dt) {
        (void)dt; // unused

        // Process logical interactions (input, state)
        ProcessButtons(world);

        // Then update visuals to reflect those states
        UpdateButtonVisuals(world);
    }

    void UISystem::RegisterAction(uint32_t actionID, ActionCallback callback) {
        // Associates a unique action ID with its callback function
        m_actionRegistry[actionID] = callback;
        LOG_DEBUG("Registered UI action ID: " << actionID);
    }

    void UISystem::ProcessButtons(World& world) {
        // Get mouse position in screen space
        glm::dvec2 mousePos;
        Input::GetMousePosition(mousePos.x, mousePos.y);

        const auto& window = WindowManager::GetMainWindow();
        const float winHeight = static_cast<float>(window->Height());

        // Convert to bottom-left origin (OpenGL style)
        const float mouseX = static_cast<float>(mousePos.x);
        const float mouseY = winHeight - static_cast<float>(mousePos.y);

        // Mouse button state
        const bool isMouseDown = Input::IsMouseDown(MOUSE_LEFT);
        const bool mouseJustPressed = Input::IsMousePressed(MOUSE_LEFT);
        const bool mouseJustReleased = Input::IsMouseReleased(MOUSE_LEFT);

        // Process all buttons
        world.Each<Components::UIButton>([&](Entity e, Components::UIButton& btn) {
            // Check if mouse is over button
            const bool isInside = IsMouseInButton(mouseX, mouseY, btn.X, btn.Y, btn.W, btn.H);

            // Update hover state
            btn.Hovered = isInside;

            // Handle press start
            if (mouseJustPressed && isInside) {
                btn.Pressed = true;
            }

            // Handle press release
            if (mouseJustReleased) {
                if (btn.Pressed && isInside) {
                    // Button was clicked! Execute action
                    if (btn.ActionID != 0) {
                        auto it = m_actionRegistry.find(btn.ActionID);
                        if (it != m_actionRegistry.end()) {
                            LOG_DEBUG("Executing action for button ID " << btn.ID
                                << " (ActionID: " << btn.ActionID << ")");
                            it->second(world); // Call the registered callback
                        }
                        else {
                            LOG_WARNING("No action registered for ActionID: " << btn.ActionID);
                        }
                    }
                }
                btn.Pressed = false;
            }
            });

        m_wasMouseDown = isMouseDown;
    }

    void UISystem::UpdateButtonVisuals(World& world) {
        // Update ShapeBox2D colors based on button state
        world.Each<Components::UIButton, Components::ShapeBox2D>(
            [](Entity /*e*/, const Components::UIButton& btn, Components::ShapeBox2D& box) {
                // Choose color based on button state
                if (btn.Pressed) {
                    box.Color = Color{ 0.2f, 0.2f, 0.25f, 1.0f }; // Dark when pressed
                }
                else if (btn.Hovered) {
                    box.Color = Color{ 0.35f, 0.35f, 0.4f, 1.0f }; // Light gray when hovered
                }
                else {
                    box.Color = Color{ 0.18f, 0.18f, 0.2f, 1.0f }; // Default dark gray
                }
            }
        );
    }

    bool UISystem::IsMouseInButton(float mouseX, float mouseY,
        float btnX, float btnY,
        float btnW, float btnH) const {
        // Basic AABB check (axis-aligned bounding box)
        return (mouseX >= btnX && mouseX <= btnX + btnW &&
            mouseY >= btnY && mouseY <= btnY + btnH);
    }

} // namespace ECS