/* Start Header *****************************************************************/
/*!
\file   GUISystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of the GUISystem for ECS-based GUI management.

The GUISystem replaces the legacy UISystem with a complete, feature-rich
implementation that includes:
- Hierarchical layout calculation with multiple layout modes
- Complete input handling (mouse, keyboard, touch)
- Interactive element state management
- Rendering integration with RendererSystem
- Tooltip and modal dialog support
- Focus management for input fields

This system operates as a standard ISystem within the ECS framework,
executing in the PreRender phase to ensure all GUI state is prepared
before rendering occurs.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/GUISystem.h"
#include "ecs/ui/GUILayout.h"
#include "ecs/ui/GUIHelpers.h"
#include "ecs/ui/GUIContext.h"
#include "ecs/World.h"
#include "ecs/systems/RendererSystem.h"
#include "services/Input.h"
#include "services/TimeSystem.h"
#include "core/Logger.h"
#include "core/Application.h"
#include "graphics/renderer.hpp"
#include "graphics/shader.hpp"
#include "graphics/font.hpp"
#include "helpers/MathUtils.h"
#include "ecs/StringTable.h"
#include <algorithm>
#include <glm/vec2.hpp>
#include <memory>
#include <unordered_map>
#include <string>

// Resolve namespace conflicts from glm includes
using std::max;
using std::min;

namespace ECS {

    // ========================================================================
    // ISystem Interface Implementation
    // ========================================================================

    void GUISystem::OnCreate(World& world) {
        // Initialize any per-world resources here if needed
    }

    void GUISystem::OnUpdate(World& world) {
        const float deltaTime = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        m_eventQueue.Clear();
        m_renderCommandBuffer.Clear();
        m_stringCache.Clear();
        for (auto& [id, state] : m_runtimeStates) {
            state.Hovered = false;
            state.Pressed = false;
            state.Released = false;
        }

        // Phase 0: Build spatial grid for hit-testing acceleration
        if (m_spatialGridDirty) {
            BuildSpatialGrid(world);
            m_spatialGridDirty = false;
        }

        // Phase 1: Update layout for all GUI elements
        UpdateLayout(world);

        // Phase 2: Handle input and update interactive state
        HandleInput(world, deltaTime);

        // Phase 3: Update interactive element states
        UpdateInteraction(world, deltaTime);

        // Phase 4: Apply visual feedback (hover, focus, active states)
        RendererSystem* rendererSystem = GetRendererSystem(world);
        if (rendererSystem) {
            ApplyVisualFeedback(world, rendererSystem);
        }

        // Phase 5: Submit rendering commands
        RenderGUI(world);
    }

    void GUISystem::OnDestroy(World& world) {
        // Clean up any per-world resources
        m_modals.clear();
        m_actionRegistry.clear();
    }

    SystemMetadata GUISystem::GetMetadata() const {
        return ComponentAccessBuilder("GUISystem")
            .WriteComponent<Components::GUIElement>()
            .SetExecutionOrder(0)
            .SetGroup(SystemGroup::PreRender)
            .SetRunMode(SystemRunMode::Always)
            .SetEnabled(true)
            .Build();
    }

    // ========================================================================
    // Layout System
    // ========================================================================

    void GUISystem::UpdateLayout(World& world) {
        // Use the layout engine to calculate positions and sizes
        UI::GUILayout::CalculateLayout(world, m_canvasSize);
    }

    void GUISystem::UpdateElementLayout(World& world, Entity entity,
                                       Vector2D parentPos, Vector2D parentSize) {
        if (entity.IsNull() || !world.IsAlive(entity)) {
            return;
        }

        if (!world.Has<Components::GUIElement>(entity)) {
            return;
        }

        auto& element = world.Get<Components::GUIElement>(entity);

        // Calculate world position based on anchoring
        Vector2D worldPos = UI::GUILayout::CalculateAnchoredPosition(
            element.Position,
            element.AnchorMin,
            element.AnchorMax,
            parentPos,
            parentSize,
            element.Size
        );

        element.WorldPosition = worldPos;
        element.DirtyLayout = false;

        // If this is a container, layout children
        if (world.Has<Components::GUIContainer>(entity)) {
            CalculateContainerLayout(world, entity, element, world.Get<Components::GUIContainer>(entity));
        }
    }

    void GUISystem::CalculateContainerLayout(World& world, Entity containerEntity,
                                            const Components::GUIElement& containerElement,
                                            const Components::GUIContainer& container) {
        // Layout children based on container's layout type
        (void)container;
        if (!world.Has<Components::GUIChildList>(containerEntity)) {
            return;
        }

        const auto& childList = world.Get<Components::GUIChildList>(containerEntity);
        for (uint16_t i = 0; i < childList.ChildCount && i < Components::GUIChildList::MaxChildren; ++i) {
            Entity child = childList.Children[i];
            if (!child.IsNull()) {
                UpdateElementLayout(world, child, containerElement.WorldPosition, containerElement.Size);
            }
        }
    }

    // ========================================================================
    // Input Query and Blocking API
    // ========================================================================

    bool GUISystem::IsElementPressed(Entity entity) const {
        return m_pressedElement == entity;
    }

    bool GUISystem::ShouldBlockInput() const {
        // Block input if:
        // 1. Mouse is over any GUI element
        // 2. A GUI element has focus (input field)
        // 3. A modal is active
        return IsMouseOverGUI() || !m_focusedElement.IsNull() || HasActiveModal();
    }

    // ========================================================================
    // Spatial Partitioning Grid
    // ========================================================================

    void GUISystem::BuildSpatialGrid(World& world) {
        // Clear all grid cells
        for (uint32_t y = 0; y < GRID_HEIGHT; ++y) {
            for (uint32_t x = 0; x < GRID_WIDTH; ++x) {
                m_spatialGrid[x][y].elements.clear();
            }
        }

        // Get sorted elements by Z-order (highest first for top-down hit testing)
        auto elements = GetSortedGUIElements(world);
        
        // Insert each element into grid cells it overlaps
        for (Entity entity : elements) {
            if (!world.Has<Components::GUIElement>(entity)) {
                continue;
            }

            const auto& element = world.Get<Components::GUIElement>(entity);
            if (!element.Active || !element.Visible) {
                continue;
            }

            // Calculate grid cell bounds for this element
            Vector2D minPos = element.WorldPosition;
            Vector2D maxPos = element.WorldPosition + element.Size;

            // Normalize to grid coordinates [0, 1]
            float minGridX = minPos.X / m_canvasSize.X;
            float minGridY = minPos.Y / m_canvasSize.Y;
            float maxGridX = maxPos.X / m_canvasSize.X;
            float maxGridY = maxPos.Y / m_canvasSize.Y;

            // Clamp to valid range
            minGridX = std::max(0.0f, std::min(1.0f, minGridX));
            minGridY = std::max(0.0f, std::min(1.0f, minGridY));
            maxGridX = std::max(0.0f, std::min(1.0f, maxGridX));
            maxGridY = std::max(0.0f, std::min(1.0f, maxGridY));

            // Calculate grid cell indices
            uint32_t minCellX = static_cast<uint32_t>(minGridX * (GRID_WIDTH - 1));
            uint32_t minCellY = static_cast<uint32_t>(minGridY * (GRID_HEIGHT - 1));
            uint32_t maxCellX = static_cast<uint32_t>(maxGridX * (GRID_WIDTH - 1));
            uint32_t maxCellY = static_cast<uint32_t>(maxGridY * (GRID_HEIGHT - 1));

            // Insert into all overlapping cells
            for (uint32_t y = minCellY; y <= maxCellY; ++y) {
                for (uint32_t x = minCellX; x <= maxCellX; ++x) {
                    m_spatialGrid[x][y].elements.push_back(entity);
                }
            }
        }
    }

    // ========================================================================
    // Visual Feedback System
    // ========================================================================

    void GUISystem::ApplyVisualFeedback(World& world, RendererSystem* rendererSystem) {
        if (!rendererSystem) return;

        // Update visual states for all GUI elements
        world.Each<Components::GUIElement>([&](Entity entity, Components::GUIElement& element) {
            if (!element.Active || !element.Visible) {
                return;
            }

            uint32_t entityId = entity.Index;
            auto& visualState = m_visualStates[entityId];

            // Determine target state based on interaction
            bool isHovered = entity == m_hoveredElement;
            bool isPressed = entity == m_pressedElement;
            bool isFocused = entity == m_focusedElement;
            bool isDisabled = false;

            // Check if element is disabled
            if (world.Has<Components::GUIButton>(entity)) {
                isDisabled = !world.Get<Components::GUIButton>(entity).Interactable;
            }

            // Calculate target scale
            float targetScale = 1.0f;
            if (isDisabled) {
                targetScale = 1.0f;  // Disabled elements don't scale, but alpha is reduced
            } else if (isPressed) {
                targetScale = m_visualFeedbackConfig.activeScale;
            } else if (isHovered) {
                targetScale = m_visualFeedbackConfig.hoverScale;
            }

            // Smoothly transition to target scale
            if (m_visualFeedbackConfig.enableSmoothTransitions) {
                visualState.currentScale = MathUtils::Lerp(
                    visualState.currentScale,
                    targetScale,
                    m_visualFeedbackConfig.transitionSpeed * static_cast<float>(TimeSystem::Instance().GetFixedTimeStep())  // Assume 60fps
                );
            } else {
                visualState.currentScale = targetScale;
            }

            // Set overlay color based on state
            visualState.hasOverlay = false;
            if (isDisabled) {
                visualState.overlayColor = m_visualFeedbackConfig.hoverOverlay;
                visualState.overlayColor.A = static_cast<uint8_t>(255.0f * m_visualFeedbackConfig.disabledAlpha);
                visualState.hasOverlay = true;
            } else if (isFocused) {
                visualState.overlayColor = m_visualFeedbackConfig.focusOverlay;
                visualState.hasOverlay = true;
            } else if (isPressed) {
                visualState.overlayColor = m_visualFeedbackConfig.activeOverlay;
                visualState.hasOverlay = true;
            } else if (isHovered) {
                visualState.overlayColor = m_visualFeedbackConfig.hoverOverlay;
                visualState.hasOverlay = true;
            }
        });
    }

    // ========================================================================
    // Input Handling
    // ========================================================================

    void GUISystem::HandleInput(World& world, float deltaTime) {
        // Get mouse position
        glm::dvec2 mousePosDouble;
        Input::GetMousePosition(mousePosDouble.x, mousePosDouble.y);
        m_mousePosition = { static_cast<float>(mousePosDouble.x), static_cast<float>(mousePosDouble.y) };

        // Get mouse button state
        bool mousePressed = Input::IsMousePressed(MOUSE_LEFT);
        bool mouseReleased = Input::IsMouseUp(MOUSE_LEFT);
        bool mouseDown = Input::IsMouseDown(MOUSE_LEFT);

        m_mousePressed = mousePressed;
        m_mouseReleased = mouseReleased;
        m_mouseDown = mouseDown;

        // Raycast to find element under mouse using spatial grid
        Entity previousHovered = m_hoveredElement;
        m_hoveredElement = RaycastGUI(world, m_mousePosition);
        if (previousHovered != m_hoveredElement) {
            if (!previousHovered.IsNull()) {
                m_eventQueue.Push(UI::GUIEventType::HoverExited, previousHovered, m_mousePosition);
            }
            if (!m_hoveredElement.IsNull()) {
                m_eventQueue.Push(UI::GUIEventType::HoverEntered, m_hoveredElement, m_mousePosition);
            }
        }

        if (!m_hoveredElement.IsNull()) {
            m_runtimeStates[m_hoveredElement.Index].Hovered = true;
        }

        // Track pressed element
        if (mousePressed && !m_hoveredElement.IsNull()) {
            m_pressedElement = m_hoveredElement;
            m_runtimeStates[m_pressedElement.Index].Pressed = true;
            m_eventQueue.Push(UI::GUIEventType::Pressed, m_pressedElement, m_mousePosition);
        }
        if (mouseReleased) {
            if (!m_pressedElement.IsNull()) {
                m_runtimeStates[m_pressedElement.Index].Released = true;
                m_eventQueue.Push(UI::GUIEventType::Released, m_pressedElement, m_mousePosition);
                if (m_pressedElement == m_hoveredElement) {
                    m_eventQueue.Push(UI::GUIEventType::Clicked, m_pressedElement, m_mousePosition);
                }
            }
            m_pressedElement = NULL_ENTITY;
        }

        // Update tooltip
        if (m_tooltip.Visible) {
            m_tooltip.Timer -= deltaTime;
            if (m_tooltip.Timer <= 0.0f) {
                m_tooltip.Visible = false;
            }
        }

        // Handle mouse pressed
        if (mousePressed) {
            m_mouseDragStart = m_mousePosition;

            // Check for double-click
            const double currentTime = TimeSystem::Instance().GetRealTimeSinceStart();
            const float doubleClickTime = static_cast<float>(currentTime - m_lastInteractionTime);
            
            if (doubleClickTime < m_doubleClickThreshold &&
                m_lastClickedElement == m_hoveredElement) {
                // Double-click detected
            }

            m_lastInteractionTime = static_cast<float>(currentTime);
            m_lastClickedElement = m_hoveredElement;

            // Set drag if hovering over draggable element
            if (!m_hoveredElement.IsNull()) {
                m_draggedElement = m_hoveredElement;
            }

            Entity previousFocused = m_focusedElement;
            if (!m_hoveredElement.IsNull() && world.Has<Components::GUIInputField>(m_hoveredElement)) {
                m_focusedElement = m_hoveredElement;
            } else {
                m_focusedElement = NULL_ENTITY;
            }

            if (previousFocused != m_focusedElement) {
                if (!previousFocused.IsNull()) {
                    m_eventQueue.Push(UI::GUIEventType::Unfocused, previousFocused, m_mousePosition);
                    m_runtimeStates[previousFocused.Index].Focused = false;
                }
                if (!m_focusedElement.IsNull()) {
                    m_eventQueue.Push(UI::GUIEventType::Focused, m_focusedElement, m_mousePosition);
                    m_runtimeStates[m_focusedElement.Index].Focused = true;
                }
            }
        }

        // Handle mouse released
        if (mouseReleased) {
            m_draggedElement = NULL_ENTITY;
        }
    }

    Entity GUISystem::GetElementAtPoint(Vector2D point) {
        // Simple point-in-rect test (used internally, requires world context)
        // This is kept for backwards compatibility but RaycastGUI is preferred
        return NULL_ENTITY;
    }

    Entity GUISystem::RaycastGUI(World& world, Vector2D point, Entity skipEntity) {
        // Normalize point to grid coordinates [0, 1]
        float gridX = point.X / m_canvasSize.X;
        float gridY = point.Y / m_canvasSize.Y;

        // Clamp to valid range
        gridX = std::max(0.0f, std::min(1.0f, gridX));
        gridY = std::max(0.0f, std::min(1.0f, gridY));

        // Calculate grid cell index
        uint32_t cellX = static_cast<uint32_t>(gridX * (GRID_WIDTH - 1));
        uint32_t cellY = static_cast<uint32_t>(gridY * (GRID_HEIGHT - 1));

        // Clamp cell indices
        cellX = std::min(cellX, GRID_WIDTH - 1);
        cellY = std::min(cellY, GRID_HEIGHT - 1);

        // Get candidates from spatial grid cell
        const auto& cellElements = m_spatialGrid[cellX][cellY].elements;

        // Test elements in reverse order (highest Z-order first)
        for (auto it = cellElements.rbegin(); it != cellElements.rend(); ++it) {
            Entity entity = *it;

            // Skip if requested
            if (entity == skipEntity) {
                continue;
            }

            // Skip if not alive or doesn't have GUIElement
            if (!world.IsAlive(entity) || !world.Has<Components::GUIElement>(entity)) {
                continue;
            }

            // Get element and check bounds
            const auto& element = world.Get<Components::GUIElement>(entity);
            if (!element.Active || !element.Visible) {
                continue;
            }

            // Detailed hit test on candidate
            if (IsPointInElement(point, element)) {
                return entity;
            }
        }

        return NULL_ENTITY;
    }

    // ========================================================================
    // Interaction Updates
    // ========================================================================

    void GUISystem::UpdateInteraction(World& world, float deltaTime) {
        // Update button states
        world.Each<Components::GUIButton>([&](Entity entity, Components::GUIButton& button) {
            bool isHovered = entity == m_hoveredElement;
            bool isPressed = isHovered && m_mousePressed;

            UpdateButtonState(world, entity, button, isHovered, isPressed);
        });

        // Update sliders
        world.Each<Components::GUISlider>([&](Entity entity, Components::GUISlider& slider) {
            bool isHovered = entity == m_hoveredElement;
            if (world.Has<Components::GUIElement>(entity)) {
                const auto& element = world.Get<Components::GUIElement>(entity);
                UpdateSliderInteraction(world, entity, slider, element, isHovered, m_mousePosition);
            }
        });

        // Update input fields
        if (!m_focusedElement.IsNull() && world.Has<Components::GUIInputField>(m_focusedElement)) {
            auto& input = world.Get<Components::GUIInputField>(m_focusedElement);
            // TODO: Handle text input from keyboard
            UpdateInputField(m_focusedElement, input, true, 0);
        }

        // Update scroll views
        world.Each<Components::GUIScrollView>([&](Entity entity, Components::GUIScrollView& scroll) {
            if (world.Has<Components::GUIElement>(entity)) {
                const auto& element = world.Get<Components::GUIElement>(entity);
                UpdateScrollView(entity, scroll, element, world);
            }
        });
    }

    void GUISystem::UpdateButtonState(World& world, Entity entity, Components::GUIButton& button,
                                     bool mouseOver, bool mousePressed) {
        auto& runtimeState = m_runtimeStates[entity.Index];
        runtimeState.Hovered = mouseOver;

        // Transition between states
        if (!button.Interactable) {
            button.State = Components::ButtonState::Disabled;
            runtimeState.Pressed = false;
            return;
        }

        if (mousePressed) {
            button.State = Components::ButtonState::Pressed;
            runtimeState.Pressed = true;
        } else if (mouseOver) {
            button.State = Components::ButtonState::Hovered;
            runtimeState.Pressed = false;
        } else {
            button.State = Components::ButtonState::Normal;
            runtimeState.Pressed = false;
        }

        // Check for release and trigger action
        if (m_mouseReleased && mouseOver && m_pressedElement == entity) {
            if (button.ActionID != 0) {
                auto& registry = UI::GUIContext::Get().ActionRegistry;
                auto it = registry.find(button.ActionID);
                if (it != registry.end()) {
                    it->second(world, entity);
                }
            }
            runtimeState.Released = true;
        } else {
            runtimeState.Released = false;
        }
    }

    void GUISystem::UpdateSliderInteraction(World& world, Entity entity, Components::GUISlider& slider,
                                           const Components::GUIElement& element,
                                           bool mouseOver, Vector2D mousePos) {
        if (!slider.Interactable) {
            return;
        }

        auto& runtimeState = m_runtimeStates[entity.Index];

        // Check if mouse started dragging on slider handle
        if (m_mousePressed && mouseOver && m_draggedElement == entity) {
            runtimeState.Dragging = true;
            runtimeState.DragOffset = mousePos.X - element.WorldPosition.X;
        }

        // Update slider value while dragging
        if (runtimeState.Dragging && m_mouseDown) {
            float sliderX = mousePos.X - element.WorldPosition.X;
            float sliderWidth = element.Size.X;
            float ratio = std::max(0.0f, std::min(1.0f, sliderX / sliderWidth));
            slider.CurrentValue = slider.MinValue + (slider.MaxValue - slider.MinValue) * ratio;

            // Trigger action
            if (slider.ActionID != 0) {
                auto it = m_actionRegistry.find(slider.ActionID);
                if (it != m_actionRegistry.end()) {
                    it->second(world, entity);
                }
            }
        }

        // Stop dragging on mouse release
        if (m_mouseReleased) {
            runtimeState.Dragging = false;
        }
    }

    void GUISystem::UpdateInputField(Entity entity, Components::GUIInputField& input,
                                    bool focused, char inputChar) {
        if (!focused || !input.Interactable) {
            return;
        }

        // TODO: Handle keyboard input
        // - Character input
        // - Backspace
        // - Arrow keys for caret movement
        // - Selection
    }

    void GUISystem::UpdateScrollView(Entity entity, Components::GUIScrollView& scroll,
                                    const Components::GUIElement& element,
                                    World& world) {
        if (!scroll.VerticalScroll && !scroll.HorizontalScroll) {
            return;
        }

        // TODO: Handle scroll wheel input
        // TODO: Handle drag-based scrolling
        // TODO: Apply inertia if enabled
    }

    // ========================================================================
    // Rendering
    // ========================================================================

    void GUISystem::RenderGUI(World& world) {
        // Get RendererSystem for submitting GUI render commands
        RendererSystem* rendererSystem = GetRendererSystem(world);

        if (!rendererSystem) {
            LOG_WARNING("GUISystem::RenderGUI: RendererSystem not available");
            return;
        }

        // Get all GUI entities and sort by Z-order
        std::vector<Entity> guiElements = GetSortedGUIElements(world);

        // Render each element
        for (Entity entity : guiElements) {
            if (world.IsAlive(entity)) {
                RenderElement(world, entity);
            }
        }

        // Render modals on top
        for (Entity modal : m_modals) {
            if (world.IsAlive(modal)) {
                RenderElement(world, modal);
            }
        }

        // Render tooltip last
        if (m_tooltip.Visible) {
            // TODO: Render tooltip via RendererSystem
        }

        FlushRenderCommands(rendererSystem);
    }

    void GUISystem::RenderElement(World& world, Entity entity) {
        if (!world.Has<Components::GUIElement>(entity)) {
            return;
        }

        auto& element = world.Get<Components::GUIElement>(entity);

        if (!element.Active || !element.Visible) {
            return;
        }

        // Render based on component type
        if (world.Has<Components::GUIPanel>(entity)) {
            RenderPanel(element, world.Get<Components::GUIPanel>(entity));
        }

        if (world.Has<Components::GUIButton>(entity)) {
            RenderButton(entity, element, world.Get<Components::GUIButton>(entity));
        }

        if (world.Has<Components::GUIText>(entity)) {
            RenderText(entity, element, world.Get<Components::GUIText>(entity));
        }

        if (world.Has<Components::GUISlider>(entity)) {
            RenderSlider(element, world.Get<Components::GUISlider>(entity));
        }

        if (world.Has<Components::GUICheckbox>(entity)) {
            RenderCheckbox(element, world.Get<Components::GUICheckbox>(entity));
        }

        if (world.Has<Components::GUIDropdown>(entity)) {
            RenderDropdown(element, world.Get<Components::GUIDropdown>(entity));
        }

        if (world.Has<Components::GUISeparator>(entity)) {
            RenderSeparator(element, world.Get<Components::GUISeparator>(entity));
        }
    }

    void GUISystem::RenderButton(Entity entity, const Components::GUIElement& element,
                                const Components::GUIButton& button) {
        // Determine button color based on state
        Color bgColor;
        switch (button.State) {
            case Components::ButtonState::Hovered:
                bgColor = button.ColorHovered;
                break;
            case Components::ButtonState::Pressed:
                bgColor = button.ColorPressed;
                break;
            case Components::ButtonState::Disabled:
                bgColor = button.ColorDisabled;
                break;
            case Components::ButtonState::Normal:
            default:
                bgColor = button.ColorNormal;
                break;
        }

        // Submit button panel rendering
        Vector2D center{
            element.WorldPosition.X + element.Size.X * 0.5f,
            element.WorldPosition.Y + element.Size.Y * 0.5f
        };
        m_renderCommandBuffer.SubmitPanel(center, element.Size, bgColor, 0.0f);
    }

    void GUISystem::RenderPanel(const Components::GUIElement& element,
                               const Components::GUIPanel& panel) {
        // Submit panel rendering to RendererSystem
        Vector2D center{
            element.WorldPosition.X + element.Size.X * 0.5f,
            element.WorldPosition.Y + element.Size.Y * 0.5f
        };
        m_renderCommandBuffer.SubmitPanel(center, element.Size,
                                          panel.BackgroundColor, panel.BorderRadius);
    }

    void GUISystem::RenderText(Entity entity, const Components::GUIElement& element,
                              const Components::GUIText& text) {
        (void)entity;
        const std::string content = ECS::StringTable::Resolve(text.Content);
        const std::string fontPath = ECS::StringTable::Resolve(text.FontPath);
        const Vector2D measured = UI::GUITextUtils::MeasureText(content, fontPath, text.FontSize);

        const float contentWidth = std::max(0.0f, element.Size.X - element.PaddingLeft - element.PaddingRight);
        const float contentHeight = std::max(0.0f, element.Size.Y - element.PaddingTop - element.PaddingBottom);

        float x = element.WorldPosition.X + element.PaddingLeft;
        switch (text.Alignment) {
            case Components::GUIText::TextAlignment::Center:
                x += (contentWidth - measured.X) * 0.5f;
                break;
            case Components::GUIText::TextAlignment::Right:
                x += contentWidth - measured.X;
                break;
            case Components::GUIText::TextAlignment::Justified:
            case Components::GUIText::TextAlignment::Left:
            default:
                break;
        }

        float y = element.WorldPosition.Y + element.PaddingTop;
        switch (element.VAlign) {
            case Components::VerticalAlignment::Middle:
                y += (contentHeight - measured.Y) * 0.5f;
                break;
            case Components::VerticalAlignment::Bottom:
                y += contentHeight - measured.Y;
                break;
            case Components::VerticalAlignment::Stretch:
            case Components::VerticalAlignment::Top:
            default:
                break;
        }

        m_renderCommandBuffer.SubmitText(text.FontPath, text.Content, { x, y },
                                         text.FontColor, text.FontSize,
                                         text.ShadowColor, text.ShadowOffset);
    }

    void GUISystem::RenderSlider(const Components::GUIElement& element,
                                const Components::GUISlider& slider) {
        // Normalize current value to 0.0-1.0 range for the slider handle position
        float normalizedValue = (slider.CurrentValue - slider.MinValue) / 
                                (slider.MaxValue - slider.MinValue);
        normalizedValue = std::max(0.0f, std::min(1.0f, normalizedValue));
        
        // Submit slider rendering via RendererSystem
        Vector2D center{
            element.WorldPosition.X + element.Size.X * 0.5f,
            element.WorldPosition.Y + element.Size.Y * 0.5f
        };
        m_renderCommandBuffer.SubmitSlider(center, element.Size,
                                           normalizedValue, slider.BackgroundColor,
                                           slider.HandleColor);
    }

    void GUISystem::RenderCheckbox(const Components::GUIElement& element,
                                  const Components::GUICheckbox& checkbox) {
        // Submit checkbox rendering via RendererSystem
        // Use appropriate color based on checked state
        Color boxColor = checkbox.IsChecked ? checkbox.CheckedColor : checkbox.UncheckedColor;
        Vector2D center{
            element.WorldPosition.X + element.Size.X * 0.5f,
            element.WorldPosition.Y + element.Size.Y * 0.5f
        };
        m_renderCommandBuffer.SubmitCheckbox(center, element.Size,
                                             checkbox.IsChecked, boxColor,
                                             checkbox.CheckedColor, checkbox.BorderColor);
    }

    void GUISystem::RenderDropdown(const Components::GUIElement& element,
                                  const Components::GUIDropdown& dropdown) {
        // Render dropdown button
        Vector2D center{
            element.WorldPosition.X + element.Size.X * 0.5f,
            element.WorldPosition.Y + element.Size.Y * 0.5f
        };
        m_renderCommandBuffer.SubmitPanel(center, element.Size,
                                          dropdown.BackgroundColor, 0.0f);

        // If dropdown is open, render the options list
        if (dropdown.IsOpen && dropdown.OptionCount > 0) {
            // Each option is rendered below the button
            float optionHeight = element.Size.Y;
            
            for (uint32_t i = 0; i < dropdown.OptionCount && i < Components::GUIDropdown::MaxOptions; ++i) {
                Vector2D optionPos = element.WorldPosition;
                optionPos.Y += element.Size.Y * (i + 1); // Offset each option below the button
                
                // Highlight selected option
                Color optionColor = (i == dropdown.SelectedIndex) ?
                                   dropdown.HighlightColor : dropdown.BackgroundColor;
                
                Vector2D optionCenter{
                    optionPos.X + element.Size.X * 0.5f,
                    optionPos.Y + element.Size.Y * 0.5f
                };
                m_renderCommandBuffer.SubmitPanel(optionCenter, element.Size, optionColor, 0.0f);
            }
        }
    }

    void GUISystem::RenderSeparator(const Components::GUIElement& element,
                                   const Components::GUISeparator& separator) {
        // Calculate start and end points based on orientation
        Vector2D startPos, endPos;
        
        if (separator.Orient == Components::GUISeparator::Orientation::Horizontal) {
            // Horizontal line across the width
            float centerY = element.WorldPosition.Y + element.Size.Y * 0.5f;
            startPos = { element.WorldPosition.X, centerY };
            endPos = { element.WorldPosition.X + element.Size.X, centerY };
        } else {
            // Vertical line across the height
            float centerX = element.WorldPosition.X + element.Size.X * 0.5f;
            startPos = { centerX, element.WorldPosition.Y };
            endPos = { centerX, element.WorldPosition.Y + element.Size.Y };
        }
        
        // Submit separator line via RendererSystem
        m_renderCommandBuffer.SubmitLine(startPos, endPos, separator.Color, separator.Thickness);
    }

    void GUISystem::FlushRenderCommands(RendererSystem* rendererSystem) {
        if (!rendererSystem) {
            return;
        }

        static const std::string kDefaultFont = "assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf";

        for (const auto& cmd : m_renderCommandBuffer.Commands()) {
            switch (cmd.Type) {
                case UI::GUIRenderCommandType::Panel:
                    rendererSystem->SubmitGUIPanel(cmd.Position, cmd.Size, cmd.PrimaryColor, cmd.ScalarA);
                    break;
                case UI::GUIRenderCommandType::Text: {
                    const std::string& resolvedFont = m_stringCache.Resolve(cmd.FontId);
                    const std::string& fontPath = resolvedFont.empty() ? kDefaultFont : resolvedFont;
                    const std::string& content = m_stringCache.Resolve(cmd.TextId);
                    rendererSystem->SubmitGUIText(fontPath, content, cmd.Position,
                                                  cmd.PrimaryColor, cmd.ScalarA,
                                                  cmd.SecondaryColor, cmd.Offset);
                    break;
                }
                case UI::GUIRenderCommandType::Slider:
                    rendererSystem->SubmitGUISlider(cmd.Position, cmd.Size, cmd.ScalarA,
                                                    cmd.PrimaryColor, cmd.SecondaryColor);
                    break;
                case UI::GUIRenderCommandType::Checkbox:
                    rendererSystem->SubmitGUICheckbox(cmd.Position, cmd.Size, cmd.Flag,
                                                      cmd.PrimaryColor, cmd.SecondaryColor,
                                                      cmd.TertiaryColor);
                    break;
                case UI::GUIRenderCommandType::Line:
                    rendererSystem->SubmitGUILine(cmd.Position, cmd.End, cmd.PrimaryColor, cmd.ScalarA);
                    break;
            }
        }
    }

    // ========================================================================
    // Utility Methods
    // ========================================================================

    bool GUISystem::IsPointInElement(Vector2D point, const Components::GUIElement& element) const {
        return point.X >= element.WorldPosition.X &&
               point.X <= element.WorldPosition.X + element.Size.X &&
               point.Y >= element.WorldPosition.Y &&
               point.Y <= element.WorldPosition.Y + element.Size.Y;
    }

    std::vector<Entity> GUISystem::GetSortedGUIElements(World& world) {
        std::vector<Entity> elements;
        
        // Collect all GUI elements
        world.Each<Components::GUIElement>([&](Entity entity, const Components::GUIElement& element) {
            if (element.Active && element.Visible) {
                elements.push_back(entity);
            }
        });

        // Sort by Z-order
        std::sort(elements.begin(), elements.end(), [&world](Entity a, Entity b) {
            auto& elemA = world.Get<Components::GUIElement>(a);
            auto& elemB = world.Get<Components::GUIElement>(b);
            return elemA.ZOrder < elemB.ZOrder;
        });

        return elements;
    }

    bool GUISystem::ValidateInputField(const Components::GUIInputField& input,
                                       const std::string& text) const {
        // TODO: Validate based on input type
        switch (input.Type) {
            case Components::GUIInputField::InputType::Integer:
                // Check if all characters are digits
                break;
            case Components::GUIInputField::InputType::Decimal:
                // Check if valid float
                break;
            case Components::GUIInputField::InputType::Alphanumeric:
                // Check if alphanumeric
                break;
            case Components::GUIInputField::InputType::Password:
            case Components::GUIInputField::InputType::Standard:
            default:
                // Accept all
                break;
        }
        
        // Check character limit
        if (input.MaxCharacters > 0 && text.length() > input.MaxCharacters) {
            return false;
        }

        return true;
    }

    // ========================================================================
    // RendererSystem Access
    // ========================================================================

    RendererSystem* GUISystem::GetRendererSystem(World& world) const {
        return RendererSystem::GetInstance();
    }

    // ========================================================================
    // Public API
    // ========================================================================

    void GUISystem::RegisterAction(uint32_t actionID, std::function<void(World&, Entity)> callback) {
        UI::GUIContext::Get().RegisterAction(actionID, std::move(callback));
        LOG_DEBUG("Registered GUI action ID: " << actionID);
    }

    void GUISystem::UnregisterAction(uint32_t actionID) {
        UI::GUIContext::Get().UnregisterAction(actionID);
        LOG_DEBUG("Unregistered GUI action ID: " << actionID);
    }

    void GUISystem::ShowTooltip(const std::string& text, Vector2D position, float duration) {
        m_tooltip.Text = text;
        m_tooltip.Position = position;
        m_tooltip.Duration = duration;
        m_tooltip.Timer = duration;
        m_tooltip.Visible = true;
    }

    void GUISystem::HideTooltip() {
        m_tooltip.Visible = false;
    }

    void GUISystem::ShowModal(Entity entity) {
        if (!entity.IsNull()) {
            m_modals.push_back(entity);
        }
    }

    void GUISystem::CloseModal(Entity entity) {
        auto it = std::find(m_modals.begin(), m_modals.end(), entity);
        if (it != m_modals.end()) {
            m_modals.erase(it);
        }
    }

    void GUISystem::SetCanvasSize(float width, float height) {
        m_canvasSize = {width, height};
    }

} // namespace ECS
