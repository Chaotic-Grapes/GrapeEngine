#include <algorithm>
#include <cmath>
#include "ecs/Components.h"
#include "ecs/systems/GUIInputSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "services/Input.h"

namespace ECS {
    void GUIInputSystem::OnCreate(World& world) {
        (void)world;
    }

    static bool PointInRect(const Vector2D& p, const Vector2D& pos, const Vector2D& size) {
        return p.X >= pos.X && p.Y >= pos.Y && p.X <= (pos.X + size.X) && p.Y <= (pos.Y + size.Y);
    }

    void GUIInputSystem::OnUpdate(World& world) {
        auto* renderer = RendererSystem::GetInstance();
        if (!renderer) {
            return;
        }

        RendererSystem::GUIViewport viewport = renderer->GetGUIViewport();
        if (!viewport.Active || viewport.Size.X <= 0.0f || viewport.Size.Y <= 0.0f) {
            const Vector2D renderSize = renderer->GetRenderTargetSize();
            viewport.Origin = { 0.0f, 0.0f };
            viewport.Size = renderSize;
        }

        double mouseX = 0.0;
        double mouseY = 0.0;
        Input::GetMousePosition(mouseX, mouseY);

        const Vector2D mouse = {
            static_cast<float>(mouseX / std::max(0.0001f, viewport.DisplayScale.X)),
            static_cast<float>(mouseY / std::max(0.0001f, viewport.DisplayScale.Y))
        };

        const bool mouseDown = Input::IsMouseDown(MOUSE_LEFT);
        const bool mousePressed = Input::IsMousePressed(MOUSE_LEFT);
        const bool mouseReleased = Input::IsMouseUp(MOUSE_LEFT);

        world.Each<Components::GUIElement, Components::GUIInput>([&](Entity entity, Components::GUIElement& element, Components::GUIInput& input) {
            input.Clicked = false;
            input.Released = false;

            if (!element.Visible) {
                input.Hovered = false;
                input.Pressed = false;
                input.Dragging = false;
                return;
            }

            const Vector2D pos = element.ResolvedPosition;
            const Vector2D size = element.ResolvedSize;
            const bool hovered = PointInRect(mouse, pos, size);
            input.Hovered = hovered;

            if (mousePressed && hovered) {
                input.Pressed = true;
                input.Dragging = true;
            }

            if (input.Pressed && mouseReleased) {
                input.Pressed = false;
                input.Dragging = false;
                input.Released = true;
                if (hovered) {
                    input.Clicked = true;
                }
            }

            if (!mouseDown && !mousePressed) {
                input.Dragging = false;
            }

            if (world.Has<Components::GUIButton>(entity)) {
                auto& button = world.Get<Components::GUIButton>(entity);
                if (button.Disabled) {
                    input.Hovered = false;
                    input.Pressed = false;
                    input.Dragging = false;
                    input.Clicked = false;
                    input.Released = false;
                } else if (input.Clicked && button.Toggle) {
                    button.Toggled = !button.Toggled;
                }
            }

            if (world.Has<Components::GUISlider>(entity)) {
                auto& slider = world.Get<Components::GUISlider>(entity);
                if (slider.Disabled) {
                    slider.ValueChanged = false;
                    return;
                }

                slider.ValueChanged = false;

                const float scaleX = element.Size.X > 0.0f ? (element.ResolvedSize.X / element.Size.X) : 1.0f;
                const float scaleY = element.Size.Y > 0.0f ? (element.ResolvedSize.Y / element.Size.Y) : 1.0f;
                const Vector4D padding = {
                    slider.Padding.X * scaleX,
                    slider.Padding.Y * scaleY,
                    slider.Padding.Z * scaleX,
                    slider.Padding.W * scaleY
                };
                Vector2D trackPos = {
                    element.ContentPosition.X + padding.X,
                    element.ContentPosition.Y + padding.Y
                };
                Vector2D trackSize = {
                    std::max(0.0f, element.ContentSize.X - padding.X - padding.Z),
                    std::max(0.0f, element.ContentSize.Y - padding.Y - padding.W)
                };

                const bool active = input.Dragging || (hovered && mousePressed);
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

                    float value = slider.Min + t * (slider.Max - slider.Min);
                    if (slider.Step > 0.0f) {
                        value = std::round(value / slider.Step) * slider.Step;
                    }
                    value = std::max(slider.Min, std::min(slider.Max, value));

                    if (std::abs(value - slider.Value) > 0.0001f) {
                        slider.Value = value;
                        slider.ValueChanged = true;
                    }
                }
            }
        });
    }

    void GUIInputSystem::OnDestroy(World& world) {
        (void)world;
    }

    SystemMetadata GUIInputSystem::GetMetadata() const {
        ComponentAccessBuilder builder("GUIInputSystem");
        builder.SetExecutionOrder(-15);
        return builder
            .ReadComponent<Components::GUIElement>()
            .WriteComponent<Components::GUIInput>()
            .WriteComponent<Components::GUIButton>()
            .WriteComponent<Components::GUISlider>()
            .Build();
    }
}
