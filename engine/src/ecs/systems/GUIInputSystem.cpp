/* Start Header *****************************************************************/
/*!
\file   GUIInputSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of GUI input handling and interaction updates.
*/
/* End Header *******************************************************************/

#include "ecs/systems/GUIInputSystem.h"
#include "ecs/ui/GUIContext.h"
#include "ecs/ui/GUIUtilities.h"
#include "ecs/ui/GUILayout.h"
#include "core/Logger.h"
#include "helpers/MathUtils.h"
#include "services/Input.h"
#include "services/TimeSystem.h"
#include <algorithm>
#include <glm/vec2.hpp>

namespace {
    using ECS::Entity;
    using ECS::World;
    using ECS::Components::GUIButton;
    using ECS::Components::GUIElement;
    using ECS::Components::GUIInputField;
    using ECS::Components::GUIScrollView;
    using ECS::Components::GUISlider;

    void BuildSpatialGrid(World& world) {
        auto& ctx = ECS::UI::GUIContext::Get();
        for (uint32_t y = 0; y < ECS::UI::GUIContext::GridHeight; ++y) {
            for (uint32_t x = 0; x < ECS::UI::GUIContext::GridWidth; ++x) {
                ctx.SpatialGrid[x][y].Elements.clear();
            }
        }

        auto elements = ECS::UI::GetSortedGUIElements(world);

        for (Entity entity : elements) {
            if (!world.Has<GUIElement>(entity)) {
                continue;
            }

            const auto& element = world.Get<GUIElement>(entity);
            if (!element.Active || !element.Visible) {
                continue;
            }

            Vector2D minPos = element.WorldPosition;
            Vector2D maxPos = element.WorldPosition + element.Size;

            float minGridX = minPos.X / ctx.LayoutCanvasSize.X;
            float minGridY = minPos.Y / ctx.LayoutCanvasSize.Y;
            float maxGridX = maxPos.X / ctx.LayoutCanvasSize.X;
            float maxGridY = maxPos.Y / ctx.LayoutCanvasSize.Y;

            minGridX = std::max(0.0f, std::min(1.0f, minGridX));
            minGridY = std::max(0.0f, std::min(1.0f, minGridY));
            maxGridX = std::max(0.0f, std::min(1.0f, maxGridX));
            maxGridY = std::max(0.0f, std::min(1.0f, maxGridY));

            uint32_t minCellX = static_cast<uint32_t>(minGridX * (ECS::UI::GUIContext::GridWidth - 1));
            uint32_t minCellY = static_cast<uint32_t>(minGridY * (ECS::UI::GUIContext::GridHeight - 1));
            uint32_t maxCellX = static_cast<uint32_t>(maxGridX * (ECS::UI::GUIContext::GridWidth - 1));
            uint32_t maxCellY = static_cast<uint32_t>(maxGridY * (ECS::UI::GUIContext::GridHeight - 1));

            for (uint32_t y = minCellY; y <= maxCellY; ++y) {
                for (uint32_t x = minCellX; x <= maxCellX; ++x) {
                    ctx.SpatialGrid[x][y].Elements.push_back(entity);
                }
            }
        }
    }

    Entity RaycastGUI(World& world, Vector2D point, Entity skipEntity = ECS::NULL_ENTITY) {
        auto& ctx = ECS::UI::GUIContext::Get();

        float gridX = point.X / ctx.LayoutCanvasSize.X;
        float gridY = point.Y / ctx.LayoutCanvasSize.Y;
        gridX = std::max(0.0f, std::min(1.0f, gridX));
        gridY = std::max(0.0f, std::min(1.0f, gridY));

        uint32_t cellX = static_cast<uint32_t>(gridX * (ECS::UI::GUIContext::GridWidth - 1));
        uint32_t cellY = static_cast<uint32_t>(gridY * (ECS::UI::GUIContext::GridHeight - 1));
        cellX = std::min(cellX, ECS::UI::GUIContext::GridWidth - 1);
        cellY = std::min(cellY, ECS::UI::GUIContext::GridHeight - 1);

        const auto& cellElements = ctx.SpatialGrid[cellX][cellY].Elements;

        for (auto it = cellElements.rbegin(); it != cellElements.rend(); ++it) {
            Entity entity = *it;
            if (entity == skipEntity) {
                continue;
            }

            if (!world.IsAlive(entity) || !world.Has<GUIElement>(entity)) {
                continue;
            }

            const auto& element = world.Get<GUIElement>(entity);
            if (!element.Active || !element.Visible) {
                continue;
            }

            if (ECS::UI::IsPointInElement(point, element)) {
                return entity;
            }
        }

        return ECS::NULL_ENTITY;
    }

    void UpdateButtonState(World& world, Entity entity, GUIButton& button,
                           bool mouseOver, bool mousePressed) {
        auto& ctx = ECS::UI::GUIContext::Get();
        auto& runtimeState = ctx.RuntimeStates[entity.Index];
        runtimeState.Hovered = mouseOver;

        if (!button.Interactable) {
            button.State = ECS::Components::ButtonState::Disabled;
            runtimeState.Pressed = false;
            button.Pressed = false;
            button.Released = false;
            button.Hovered = false;
            return;
        }

        if (mousePressed) {
            button.State = ECS::Components::ButtonState::Pressed;
            runtimeState.Pressed = true;
        } else if (mouseOver) {
            button.State = ECS::Components::ButtonState::Hovered;
            runtimeState.Pressed = false;
        } else {
            button.State = ECS::Components::ButtonState::Normal;
            runtimeState.Pressed = false;
        }

        button.Hovered = mouseOver;
        button.Pressed = runtimeState.Pressed;

        if (ctx.MouseReleased && mouseOver && ctx.PressedElement == entity) {
            if (button.ActionID != 0) {
                auto it = ctx.ActionRegistry.find(button.ActionID);
                if (it != ctx.ActionRegistry.end()) {
                    it->second(world, entity);
                }
            }
            runtimeState.Released = true;
        } else {
            runtimeState.Released = false;
        }

        button.Released = runtimeState.Released;
    }

    void UpdateSliderInteraction(World& world, Entity entity, GUISlider& slider,
                                 const GUIElement& element,
                                 bool mouseOver, Vector2D mousePos) {
        (void)world;
        auto& ctx = ECS::UI::GUIContext::Get();
        if (!slider.Interactable) {
            return;
        }

        auto& runtimeState = ctx.RuntimeStates[entity.Index];

        if (ctx.MousePressed && mouseOver && ctx.DraggedElement == entity) {
            runtimeState.Dragging = true;
            runtimeState.DragOffset = mousePos.X - element.WorldPosition.X;
        }

        if (runtimeState.Dragging && ctx.MouseDown) {
            float sliderX = mousePos.X - element.WorldPosition.X;
            float sliderWidth = element.Size.X;
            float ratio = std::max(0.0f, std::min(1.0f, sliderX / sliderWidth));
            slider.CurrentValue = slider.MinValue + (slider.MaxValue - slider.MinValue) * ratio;

            if (slider.ActionID != 0) {
                auto it = ctx.ActionRegistry.find(slider.ActionID);
                if (it != ctx.ActionRegistry.end()) {
                    it->second(world, entity);
                }
            }
        }

        if (ctx.MouseReleased) {
            runtimeState.Dragging = false;
        }

        slider.Dragging = runtimeState.Dragging;
        slider.DragOffset = runtimeState.DragOffset;
    }

    void UpdateInputField(Entity entity, GUIInputField& input,
                          bool focused, char inputChar) {
        (void)entity;
        (void)inputChar;
        input.Focused = focused;
        if (!focused || !input.Interactable) {
            return;
        }
    }

    void UpdateScrollView(Entity entity, GUIScrollView& scroll,
                          const GUIElement& element,
                          World& world) {
        auto& ctx = ECS::UI::GUIContext::Get();
        if (!scroll.VerticalScroll && !scroll.HorizontalScroll) {
            return;
        }

        if (ctx.HoveredElement != entity) {
            return;
        }

        const float scrollX = static_cast<float>(Input::GetScrollX());
        const float scrollY = static_cast<float>(Input::GetScrollY());
        if (scrollX == 0.0f && scrollY == 0.0f) {
            return;
        }

        const float maxScrollX = std::max(0.0f, scroll.ContentSize.X - element.Size.X);
        const float maxScrollY = std::max(0.0f, scroll.ContentSize.Y - element.Size.Y);

        if (scroll.HorizontalScroll) {
            scroll.ScrollPosition.X = std::max(0.0f, std::min(maxScrollX,
                scroll.ScrollPosition.X - scrollX * scroll.ScrollSensitivity));
        }
        if (scroll.VerticalScroll) {
            scroll.ScrollPosition.Y = std::max(0.0f, std::min(maxScrollY,
                scroll.ScrollPosition.Y - scrollY * scroll.ScrollSensitivity));
        }

        ECS::UI::GUILayout::InvalidateLayoutRecursive(world, entity);
        ctx.SpatialGridDirty = true;
    }

    void UpdateInteraction(World& world, float deltaTime) {
        (void)deltaTime;
        auto& ctx = ECS::UI::GUIContext::Get();

        world.Each<GUIButton>([&](Entity entity, GUIButton& button) {
            bool isHovered = entity == ctx.HoveredElement;
            bool isPressed = isHovered && ctx.MousePressed;
            UpdateButtonState(world, entity, button, isHovered, isPressed);
        });

        world.Each<GUISlider>([&](Entity entity, GUISlider& slider) {
            bool isHovered = entity == ctx.HoveredElement;
            if (world.Has<GUIElement>(entity)) {
                const auto& element = world.Get<GUIElement>(entity);
                UpdateSliderInteraction(world, entity, slider, element, isHovered, ctx.MousePosition);
            }
        });

        if (!ctx.FocusedElement.IsNull() && world.Has<GUIInputField>(ctx.FocusedElement)) {
            auto& input = world.Get<GUIInputField>(ctx.FocusedElement);
            UpdateInputField(ctx.FocusedElement, input, true, 0);
        }

        world.Each<GUIScrollView>([&](Entity entity, GUIScrollView& scroll) {
            if (world.Has<GUIElement>(entity)) {
                const auto& element = world.Get<GUIElement>(entity);
                UpdateScrollView(entity, scroll, element, world);
            }
        });
    }

    void ApplyVisualFeedback(World& world) {
        auto& ctx = ECS::UI::GUIContext::Get();

        world.Each<GUIElement>([&](Entity entity, GUIElement& element) {
            if (!element.Active || !element.Visible) {
                return;
            }

            uint32_t entityId = entity.Index;
            auto& visualState = ctx.VisualStates[entityId];

            bool isHovered = entity == ctx.HoveredElement;
            bool isPressed = entity == ctx.PressedElement;
            bool isFocused = entity == ctx.FocusedElement;
            bool isDisabled = false;

            if (world.Has<GUIButton>(entity)) {
                isDisabled = !world.Get<GUIButton>(entity).Interactable;
            }

            float targetScale = 1.0f;
            if (isDisabled) {
                targetScale = 1.0f;
            } else if (isPressed) {
                targetScale = ctx.VisualConfig.ActiveScale;
            } else if (isHovered) {
                targetScale = ctx.VisualConfig.HoverScale;
            }

            if (ctx.VisualConfig.EnableSmoothTransitions) {
                visualState.CurrentScale = MathUtils::Lerp(
                    visualState.CurrentScale,
                    targetScale,
                    ctx.VisualConfig.TransitionSpeed * static_cast<float>(TimeSystem::Instance().GetFixedTimeStep())
                );
            } else {
                visualState.CurrentScale = targetScale;
            }

            visualState.HasOverlay = false;
            if (isDisabled) {
                visualState.OverlayColor = ctx.VisualConfig.HoverOverlay;
                visualState.OverlayColor.A = static_cast<uint8_t>(255.0f * ctx.VisualConfig.DisabledAlpha);
                visualState.HasOverlay = true;
            } else if (isFocused) {
                visualState.OverlayColor = ctx.VisualConfig.FocusOverlay;
                visualState.HasOverlay = true;
            } else if (isPressed) {
                visualState.OverlayColor = ctx.VisualConfig.ActiveOverlay;
                visualState.HasOverlay = true;
            } else if (isHovered) {
                visualState.OverlayColor = ctx.VisualConfig.HoverOverlay;
                visualState.HasOverlay = true;
            }
        });
    }
}

namespace ECS {

    void GUIInputSystem::OnCreate(World& world) {
        (void)world;
        LOG_INFO("GUIInputSystem initialized");
    }

    void GUIInputSystem::OnUpdate(World& world) {
        auto& ctx = UI::GUIContext::Get();
        ctx.ResetFrame();

        if (ctx.SpatialGridDirty) {
            BuildSpatialGrid(world);
            ctx.SpatialGridDirty = false;
        }

        const float deltaTime = static_cast<float>(TimeSystem::Instance().GetDeltaTime());

        glm::dvec2 mousePosDouble;
        Input::GetMousePosition(mousePosDouble.x, mousePosDouble.y);
        Vector2D screenPos{ static_cast<float>(mousePosDouble.x), static_cast<float>(mousePosDouble.y) };
        bool mouseInViewport = true;
        if (ctx.UseViewportBounds && ctx.ViewportSize.X > 0.0f && ctx.ViewportSize.Y > 0.0f) {
            screenPos.X *= ctx.ViewportDisplayScale.X;
            screenPos.Y *= ctx.ViewportDisplayScale.Y;
            screenPos.X -= ctx.ViewportOrigin.X;
            screenPos.Y -= ctx.ViewportOrigin.Y;
            mouseInViewport = (screenPos.X >= 0.0f && screenPos.Y >= 0.0f &&
                screenPos.X <= ctx.ViewportSize.X && screenPos.Y <= ctx.ViewportSize.Y);
            if (mouseInViewport) {
                const float scaleX = (ctx.CanvasSize.X > 0.0f) ? (ctx.CanvasSize.X / ctx.ViewportSize.X) : 1.0f;
                const float scaleY = (ctx.CanvasSize.Y > 0.0f) ? (ctx.CanvasSize.Y / ctx.ViewportSize.Y) : 1.0f;
                screenPos.X *= scaleX;
                screenPos.Y *= scaleY;
            } else {
                screenPos = { -1.0f, -1.0f };
            }
        }
        if (ctx.CanvasScale > 0.0f) {
            screenPos.X = (screenPos.X - ctx.CanvasOffset.X) / ctx.CanvasScale;
            screenPos.Y = (screenPos.Y - ctx.CanvasOffset.Y) / ctx.CanvasScale;
        }
        ctx.MousePosition = screenPos;

        ctx.MousePressed = Input::IsMousePressed(MOUSE_LEFT);
        ctx.MouseReleased = Input::IsMouseUp(MOUSE_LEFT);
        ctx.MouseDown = Input::IsMouseDown(MOUSE_LEFT);

        Entity previousHovered = ctx.HoveredElement;
        ctx.HoveredElement = mouseInViewport ? RaycastGUI(world, ctx.MousePosition) : ECS::NULL_ENTITY;
        if (previousHovered != ctx.HoveredElement) {
            if (!previousHovered.IsNull()) {
                ctx.EventQueue.Push(UI::GUIEventType::HoverExited, previousHovered, ctx.MousePosition);
            }
            if (!ctx.HoveredElement.IsNull()) {
                ctx.EventQueue.Push(UI::GUIEventType::HoverEntered, ctx.HoveredElement, ctx.MousePosition);
            }
        }

        if (!ctx.HoveredElement.IsNull()) {
            ctx.RuntimeStates[ctx.HoveredElement.Index].Hovered = true;
        }

        if (ctx.MousePressed && !ctx.HoveredElement.IsNull()) {
            ctx.PressedElement = ctx.HoveredElement;
            ctx.RuntimeStates[ctx.PressedElement.Index].Pressed = true;
            ctx.EventQueue.Push(UI::GUIEventType::Pressed, ctx.PressedElement, ctx.MousePosition);
        }

        if (ctx.MouseReleased) {
            if (!ctx.PressedElement.IsNull()) {
                ctx.RuntimeStates[ctx.PressedElement.Index].Released = true;
                ctx.EventQueue.Push(UI::GUIEventType::Released, ctx.PressedElement, ctx.MousePosition);
                if (ctx.PressedElement == ctx.HoveredElement) {
                    ctx.EventQueue.Push(UI::GUIEventType::Clicked, ctx.PressedElement, ctx.MousePosition);
                }
            }
            ctx.PressedElement = ECS::NULL_ENTITY;
        }

        if (ctx.Tooltip.Visible) {
            ctx.Tooltip.Timer -= deltaTime;
            if (ctx.Tooltip.Timer <= 0.0f) {
                ctx.Tooltip.Visible = false;
            }
        }

        if (ctx.MousePressed) {
            ctx.MouseDragStart = ctx.MousePosition;

            const double currentTime = TimeSystem::Instance().GetRealTimeSinceStart();
            const float doubleClickTime = static_cast<float>(currentTime - ctx.LastInteractionTime);
            if (doubleClickTime < ctx.DoubleClickThreshold && ctx.LastClickedElement == ctx.HoveredElement) {
                // Double-click detected
            }

            ctx.LastInteractionTime = static_cast<float>(currentTime);
            ctx.LastClickedElement = ctx.HoveredElement;

            if (!ctx.HoveredElement.IsNull()) {
                ctx.DraggedElement = ctx.HoveredElement;
            }

            Entity previousFocused = ctx.FocusedElement;
            if (!ctx.HoveredElement.IsNull() && world.Has<GUIInputField>(ctx.HoveredElement)) {
                ctx.FocusedElement = ctx.HoveredElement;
            } else {
                ctx.FocusedElement = ECS::NULL_ENTITY;
            }

            if (previousFocused != ctx.FocusedElement) {
                if (!previousFocused.IsNull()) {
                    ctx.EventQueue.Push(UI::GUIEventType::Unfocused, previousFocused, ctx.MousePosition);
                    ctx.RuntimeStates[previousFocused.Index].Focused = false;
                }
                if (!ctx.FocusedElement.IsNull()) {
                    ctx.EventQueue.Push(UI::GUIEventType::Focused, ctx.FocusedElement, ctx.MousePosition);
                    ctx.RuntimeStates[ctx.FocusedElement.Index].Focused = true;
                }
            }
        }

        if (ctx.MouseReleased) {
            ctx.DraggedElement = ECS::NULL_ENTITY;
        }

        UpdateInteraction(world, deltaTime);
        ApplyVisualFeedback(world);
    }

    void GUIInputSystem::OnDestroy(World& world) {
        (void)world;
        LOG_INFO("GUIInputSystem destroyed");
    }

    SystemMetadata GUIInputSystem::GetMetadata() const {
        return ComponentAccessBuilder("GUIInputSystem")
            .WriteComponent<Components::GUIElement>()
            .WriteComponent<Components::GUIButton>()
            .WriteComponent<Components::GUISlider>()
            .WriteComponent<Components::GUIInputField>()
            .SetExecutionOrder(1)
            .SetGroup(SystemGroup::PreRender)
            .SetRunMode(SystemRunMode::Always)
            .SetEnabled(true)
            .Build();
    }

} // namespace ECS
