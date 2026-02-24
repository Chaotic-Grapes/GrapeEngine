/* Start Header *****************************************************************/
/*!
\file   GUIInputSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu

\brief
Definition of the GUIInputSystem class for processing input interactions with GUI
elements. Provides functionality for handling mouse events, hover states, and
pointer capture within the GUI system.
*/
/* End Header *******************************************************************/

#include <algorithm>
#include <cmath>
#include "ecs/Components.h"
#include "ecs/systems/GUIInputSystem.h"
#include "ecs/systems/GUIRenderUtils.h"
#include "ecs/systems/RendererSystem.h"
#include "services/Input.h"

namespace ECS {
    namespace {
        Entity s_captureEntity = NULL_ENTITY; // Tracks the element that owns pointer capture.
    }

    // Initialize GUI input system state for the world.
    void GUIInputSystem::OnCreate(World& world) {
        (void)world;
    }

    // Simple AABB hit test in GUI space.
    static bool PointInRect(const Vector2D& p, const Vector2D& pos, const Vector2D& size) {
        return p.X >= pos.X && p.Y >= pos.Y && p.X <= (pos.X + size.X) && p.Y <= (pos.Y + size.Y);
    }

    // Update GUI hover/press state from the current input.
    void GUIInputSystem::OnUpdate(World& world) {
        auto* renderer = RendererSystem::GetInstance();
        if (!renderer) {
            return;
        }

        if (!s_captureEntity.IsNull() && !world.IsAlive(s_captureEntity)) {
            s_captureEntity = NULL_ENTITY;
        }

        // Use the current GUI viewport (editor may override viewport per panel).
        RendererSystem::GUIViewport viewport = renderer->GetGUIViewport();
        if (!viewport.Active || viewport.Size.X <= 0.0f || viewport.Size.Y <= 0.0f) {
            const Vector2D renderSize = renderer->GetRenderTargetSize();
            viewport.Origin = { 0.0f, 0.0f };
            viewport.Size = renderSize;
        }

        // Convert raw cursor position into GUI viewport space.
        double mouseX = 0.0;
        double mouseY = 0.0;
        Input::GetMousePosition(mouseX, mouseY);

        const Vector2D mouse = {
            static_cast<float>(mouseX / std::max(0.0001f, viewport.DisplayScale.X)),
            static_cast<float>(mouseY / std::max(0.0001f, viewport.DisplayScale.Y))
        };

        // Snapshot mouse button state for this frame.
        const bool mouseDown = Input::IsMouseDown(MOUSE_LEFT);
        const bool mousePressed = Input::IsMousePressed(MOUSE_LEFT);
        const bool mouseReleased = Input::IsMouseUp(MOUSE_LEFT);

        // Iterate GUI elements and update per-entity input state.
        world.Each<Components::GUIElement, Components::GUIInput>([&](Entity entity, Components::GUIElement& element, Components::GUIInput& input) {
            // Clear one-frame flags before recomputing state.
            input.Clicked = false;
            input.Released = false;
            input.Entered = false;
            input.Exited = false;

            if (!world.IsActiveInHierarchy(entity)) {
                if (s_captureEntity == entity) {
                    s_captureEntity = NULL_ENTITY;
                }
                input.Hovered = false;
                input.Pressed = false;
                input.Dragging = false;
                return;
            }

            if (!element.Visible) {
                input.Hovered = false;
                input.Pressed = false;
                input.Dragging = false;
                return;
            }

            // Use resolved layout rects for hit testing.
            const bool isWorldSpace = (ResolveGUIRenderSpace(world, entity) == Components::GUIRenderSpace::World);
            const Vector2D pos = isWorldSpace ? element.ScreenPosition : element.ResolvedPosition;
            const Vector2D size = isWorldSpace ? element.ScreenSize : element.ResolvedSize;
            const bool hovered = PointInRect(mouse, pos, size);
            const bool wasHovered = input.Hovered;
            input.Hovered = hovered;
            input.Entered = (!wasHovered && hovered);
            input.Exited = (wasHovered && !hovered);

            // Capture pointer on press inside this element.
            if (mousePressed && hovered) {
                s_captureEntity = entity;
            }

            const bool captured = (s_captureEntity == entity);
            input.Pressed = captured && mouseDown;
            input.Dragging = captured && mouseDown;

            if (captured && mouseReleased) {
                input.Pressed = false;
                input.Dragging = false;
                input.Released = true;
                if (hovered) {
                    input.Clicked = true;
                }
                s_captureEntity = NULL_ENTITY;
            }

            // Clear capture if input was lost without a release event.
            if (captured && !mouseDown && !mousePressed) {
                input.Dragging = false;
                s_captureEntity = NULL_ENTITY;
            }

            if (world.Has<Components::GUIButton>(entity)) {
                auto& button = world.Get<Components::GUIButton>(entity);
                // Disabled buttons cannot be interacted with.
                if (button.Disabled) {
                    input.Hovered = false;
                    input.Pressed = false;
                    input.Dragging = false;
                    input.Clicked = false;
                    input.Released = false;
                } else if (input.Clicked && button.Toggle) {
                    // Toggle buttons flip state on click.
                    button.Toggled = !button.Toggled;
                }
            }

            if (world.Has<Components::GUISlider>(entity)) {
                auto& slider = world.Get<Components::GUISlider>(entity);
                // Disabled sliders ignore pointer input.
                if (slider.Disabled) {
                    slider.ValueChanged = false;
                    return;
                }

                slider.ValueChanged = false;

                // Calculate the slider track rect (interactive area) by applying padding to the element rect.
                Vector2D trackPos{};
                Vector2D trackSize{};

                // For world-space sliders, we need to scale padding based on the ratio between screen size and resolved size to keep input aligned with rendering.
                if (isWorldSpace) {
                    const float screenScaleX = element.ResolvedSize.X > 0.0f
                        ? (element.ScreenSize.X / element.ResolvedSize.X)
                        : 1.0f;
                    const float screenScaleY = element.ResolvedSize.Y > 0.0f
                        ? (element.ScreenSize.Y / element.ResolvedSize.Y)
                        : 1.0f;
                    const Vector4D padding = { // Scale padding from element space to screen space for hit testing.
                        slider.Padding.X * screenScaleX,
                        slider.Padding.Y * screenScaleY,
                        slider.Padding.Z * screenScaleX,
                        slider.Padding.W * screenScaleY
                    };
                    trackPos = { // Screen-space position of the track rect, accounting for padding.
                        element.ScreenPosition.X + padding.X,
                        element.ScreenPosition.Y + padding.Y
                    };
                    trackSize = { // Screen-space size of the track rect, accounting for padding.
                        std::max(0.0f, element.ScreenSize.X - padding.X - padding.Z),
                        std::max(0.0f, element.ScreenSize.Y - padding.Y - padding.W)
                    };
                } else {
                    // Scale padding based on resolved size so layout and input stay in sync.
                    const float scaleX = element.Size.X > 0.0f ? (element.ResolvedSize.X / element.Size.X) : 1.0f;
                    const float scaleY = element.Size.Y > 0.0f ? (element.ResolvedSize.Y / element.Size.Y) : 1.0f;
                    const Vector4D padding = {
                        slider.Padding.X * scaleX,
                        slider.Padding.Y * scaleY,
                        slider.Padding.Z * scaleX,
                        slider.Padding.W * scaleY
                    };
                    // Track rect is the interactive area of the slider.
                    trackPos = {
                        element.ContentPosition.X + padding.X,
                        element.ContentPosition.Y + padding.Y
                    };
                    trackSize = {
                        std::max(0.0f, element.ContentSize.X - padding.X - padding.Z),
                        std::max(0.0f, element.ContentSize.Y - padding.Y - padding.W)
                    };
                }

                // Update slider while dragging or on initial press.
                const bool active = captured || (hovered && mousePressed);
                if (active) {
                    float t = 0.0f;
                    if (slider.Horizontal) {
                        const float denom = std::max(0.0001f, trackSize.X);
                        t = (mouse.X - trackPos.X) / denom;
                    } else {
                        const float denom = std::max(0.0001f, trackSize.Y);
                        t = (mouse.Y - trackPos.Y) / denom;
                    }
                    t = std::max(0.0f, std::min(1.0f, t));

                    // Convert normalized position into value range.
                    float value = slider.Min + t * (slider.Max - slider.Min);
                    if (slider.Step > 0.0f) {
                        value = std::round(value / slider.Step) * slider.Step;
                    }
                    value = std::max(slider.Min, std::min(slider.Max, value));

                    // Mark when the slider value changes.
                    if (std::abs(value - slider.Value) > 0.0001f) {
                        slider.Value = value;
                        slider.ValueChanged = true;
                    }
                }
            }
        });
    }

    // Tear down GUI input system state.
    void GUIInputSystem::OnDestroy(World& world) {
        (void)world;
    }

    // Return metadata used for system registration.
    SystemMetadata GUIInputSystem::GetMetadata() const {
        ComponentAccessBuilder builder("GUIInputSystem");
        builder.SetExecutionOrder(-15);
        return builder
            .ReadComponent<Components::Active>()
            .ReadComponent<Components::Parent>()
            .ReadComponent<Components::GUIElement>()
            .WriteComponent<Components::GUIInput>()
            .WriteComponent<Components::GUIButton>()
            .WriteComponent<Components::GUISlider>()
            .Build();
    }
}
