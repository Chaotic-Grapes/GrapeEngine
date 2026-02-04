#include "ecs/systems/GUIRenderSystem.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "ecs/Components.h"
#include "ecs/systems/RendererSystem.h"
#include "services/ResourceManager.h"
#include "graphics/texture.hpp"

namespace ECS {
    void GUIRenderSystem::OnCreate(World& world) {
        (void)world;
    }

    void GUIRenderSystem::OnUpdate(World& world) {
        auto* renderer = RendererSystem::GetInstance();
        if (!renderer) {
            return;
        }

        struct RenderItem {
            int16_t zOrder;
            Entity entity;
            enum class Type { Panel, Text, Image, Button, Slider } type;
        };

        std::vector<RenderItem> items;
        auto pushItem = [&](Entity entity, const Components::GUIElement& element, RenderItem::Type type) {
            if (!element.Visible) {
                return;
            }
            items.push_back(RenderItem{ element.ZOrder, entity, type });
        };

        world.Each<Components::GUIElement, Components::GUIPanel>(
            [&](Entity entity, const Components::GUIElement& element, const Components::GUIPanel&) {
                pushItem(entity, element, RenderItem::Type::Panel);
            });

        world.Each<Components::GUIElement, Components::GUIText>(
            [&](Entity entity, const Components::GUIElement& element, const Components::GUIText&) {
                pushItem(entity, element, RenderItem::Type::Text);
            });

        world.Each<Components::GUIElement, Components::GUIImage>(
            [&](Entity entity, const Components::GUIElement& element, const Components::GUIImage&) {
                pushItem(entity, element, RenderItem::Type::Image);
            });

        world.Each<Components::GUIElement, Components::GUIButton>(
            [&](Entity entity, const Components::GUIElement& element, const Components::GUIButton&) {
                pushItem(entity, element, RenderItem::Type::Button);
            });

        world.Each<Components::GUIElement, Components::GUISlider>(
            [&](Entity entity, const Components::GUIElement& element, const Components::GUISlider&) {
                pushItem(entity, element, RenderItem::Type::Slider);
            });

        std::sort(items.begin(), items.end(),
            [](const RenderItem& a, const RenderItem& b) { return a.zOrder < b.zOrder; });

        for (const auto& item : items) {
            const auto& element = world.Get<Components::GUIElement>(item.entity);
            if (element.ResolvedSize.X <= 0.0f || element.ResolvedSize.Y <= 0.0f) {
                continue;
            }

            const float scaleX = element.Size.X > 0.0f ? (element.ResolvedSize.X / element.Size.X) : 1.0f;
            const float scaleY = element.Size.Y > 0.0f ? (element.ResolvedSize.Y / element.Size.Y) : 1.0f;

            switch (item.type) {
            case RenderItem::Type::Panel: {
                const auto& panel = world.Get<Components::GUIPanel>(item.entity);
                renderer->SubmitGUIPanel(element.ResolvedPosition, element.ResolvedSize, panel.Color, panel.CornerRadius);
                break;
            }
            case RenderItem::Type::Text: {
                const auto& text = world.Get<Components::GUIText>(item.entity);
                const std::string textValue = text.GetText();
                if (textValue.empty()) {
                    break;
                }
                renderer->SubmitGUIText(element.ContentPosition, textValue, text.GetFontPath(),
                    text.FontSize * scaleX, text.Color);
                break;
            }
            case RenderItem::Type::Image: {
                const auto& image = world.Get<Components::GUIImage>(item.entity);
                const std::string path = ECS::StringTable::Resolve(image.TexturePathId);
                uint32_t textureId = image.TextureId;
                std::shared_ptr<Texture> texture;
                if (!path.empty()) {
                    texture = RM.Get<Texture>(path);
                    if (texture) {
                        textureId = texture->ID();
                    }
                }
                if (textureId == 0 && !texture) {
                    break;
                }

                Vector2D pos = element.ContentPosition;
                Vector2D size = element.ContentSize;
                Vector4D uv = image.UVRect;

                if (texture && (image.ScaleMode != Components::GUIImageScaleMode::Stretch)) {
                    const float texW = static_cast<float>(texture->Width());
                    const float texH = static_cast<float>(texture->Height());
                    if (texW > 0.0f && texH > 0.0f) {
                        const float scale = (image.ScaleMode == Components::GUIImageScaleMode::Fit)
                            ? std::min(size.X / texW, size.Y / texH)
                            : std::max(size.X / texW, size.Y / texH);
                        const Vector2D newSize = { texW * scale, texH * scale };
                        pos.X += (size.X - newSize.X) * 0.5f;
                        pos.Y += (size.Y - newSize.Y) * 0.5f;
                        size = newSize;
                    }
                }

                if (image.UseSlicing && texture) {
                    const float texW = static_cast<float>(texture->Width());
                    const float texH = static_cast<float>(texture->Height());
                    Vector4D border = image.SliceBorder;
                    Vector4D borderScaled = {
                        border.X * scaleX,
                        border.Y * scaleY,
                        border.Z * scaleX,
                        border.W * scaleY
                    };
                    const float left = std::min(borderScaled.X, size.X * 0.5f);
                    const float right = std::min(borderScaled.Z, size.X * 0.5f);
                    const float top = std::min(borderScaled.Y, size.Y * 0.5f);
                    const float bottom = std::min(borderScaled.W, size.Y * 0.5f);

                    const float u0 = uv.X;
                    const float v0 = uv.Y;
                    const float u1 = uv.Z;
                    const float v1 = uv.W;
                    const float uLeft = texW > 0.0f ? (u0 + (border.X / texW) * (u1 - u0)) : u0;
                    const float uRight = texW > 0.0f ? (u1 - (border.Z / texW) * (u1 - u0)) : u1;
                    const float vTop = texH > 0.0f ? (v0 + (border.Y / texH) * (v1 - v0)) : v0;
                    const float vBottom = texH > 0.0f ? (v1 - (border.W / texH) * (v1 - v0)) : v1;

                    const float x0 = pos.X;
                    const float x1 = pos.X + left;
                    const float x2 = pos.X + size.X - right;
                    const float x3 = pos.X + size.X;
                    const float y0 = pos.Y;
                    const float y1 = pos.Y + top;
                    const float y2 = pos.Y + size.Y - bottom;
                    const float y3 = pos.Y + size.Y;

                    const struct Slice {
                        Vector2D pos;
                        Vector2D size;
                        Vector4D uv;
                    } slices[] = {
                        { {x0, y0}, {x1 - x0, y1 - y0}, {u0, v0, uLeft, vTop} },
                        { {x1, y0}, {x2 - x1, y1 - y0}, {uLeft, v0, uRight, vTop} },
                        { {x2, y0}, {x3 - x2, y1 - y0}, {uRight, v0, u1, vTop} },
                        { {x0, y1}, {x1 - x0, y2 - y1}, {u0, vTop, uLeft, vBottom} },
                        { {x1, y1}, {x2 - x1, y2 - y1}, {uLeft, vTop, uRight, vBottom} },
                        { {x2, y1}, {x3 - x2, y2 - y1}, {uRight, vTop, u1, vBottom} },
                        { {x0, y2}, {x1 - x0, y3 - y2}, {u0, vBottom, uLeft, v1} },
                        { {x1, y2}, {x2 - x1, y3 - y2}, {uLeft, vBottom, uRight, v1} },
                        { {x2, y2}, {x3 - x2, y3 - y2}, {uRight, vBottom, u1, v1} }
                    };

                    for (const auto& slice : slices) {
                        if (slice.size.X <= 0.0f || slice.size.Y <= 0.0f) {
                            continue;
                        }
                        renderer->SubmitGUIImage(slice.pos, slice.size, textureId, slice.uv, image.Color);
                    }
                } else {
                    renderer->SubmitGUIImage(pos, size, textureId, uv, image.Color);
                }
                break;
            }
            case RenderItem::Type::Button: {
                const auto& button = world.Get<Components::GUIButton>(item.entity);
                const Components::GUIInput* input = world.Has<Components::GUIInput>(item.entity)
                    ? &world.Get<Components::GUIInput>(item.entity)
                    : nullptr;

                Color bgColor = button.NormalColor;
                if (button.Disabled) {
                    bgColor = button.DisabledColor;
                } else if (button.Toggle && button.Toggled) {
                    bgColor = button.PressedColor;
                } else if (input && input->Pressed) {
                    bgColor = button.PressedColor;
                } else if (input && input->Hovered) {
                    bgColor = button.HoverColor;
                }

                renderer->SubmitGUIPanel(element.ResolvedPosition, element.ResolvedSize, bgColor, button.CornerRadius);

                const Vector2D innerPos = {
                    element.ContentPosition.X + button.Padding.X * scaleX,
                    element.ContentPosition.Y + button.Padding.Y * scaleY
                };
                const Vector2D innerSize = {
                    std::max(0.0f, element.ContentSize.X - (button.Padding.X + button.Padding.Z) * scaleX),
                    std::max(0.0f, element.ContentSize.Y - (button.Padding.Y + button.Padding.W) * scaleY)
                };

                const std::string label = button.TextId ? ECS::StringTable::Resolve(button.TextId) : std::string();
                if (!label.empty()) {
                    renderer->SubmitGUIText(innerPos, label, button.FontPathId ? ECS::StringTable::Resolve(button.FontPathId) : "",
                        button.FontSize * scaleX, button.TextColor);
                }

                if (button.IconPathId != 0) {
                    const std::string iconPath = ECS::StringTable::Resolve(button.IconPathId);
                    if (!iconPath.empty()) {
                        auto iconTex = RM.Get<Texture>(iconPath);
                        if (iconTex) {
                            const Vector2D iconSize = { button.IconSize.X * scaleX, button.IconSize.Y * scaleY };
                            const Vector2D iconPos = {
                                innerPos.X + button.IconOffset.X * scaleX,
                                innerPos.Y + button.IconOffset.Y * scaleY
                            };
                            renderer->SubmitGUIImage(iconPos, iconSize, iconTex->ID(),
                                Vector4D{ 0.0f, 0.0f, 1.0f, 1.0f }, button.IconColor);
                        }
                    }
                }
                break;
            }
            case RenderItem::Type::Slider: {
                auto& slider = world.Get<Components::GUISlider>(item.entity);
                const Vector2D padding = {
                    slider.Padding.X * scaleX,
                    slider.Padding.Y * scaleY
                };
                const Vector2D paddingOpposite = {
                    slider.Padding.Z * scaleX,
                    slider.Padding.W * scaleY
                };
                Vector2D trackPos = {
                    element.ContentPosition.X + padding.X,
                    element.ContentPosition.Y + padding.Y
                };
                Vector2D trackSize = {
                    std::max(0.0f, element.ContentSize.X - padding.X - paddingOpposite.X),
                    std::max(0.0f, element.ContentSize.Y - padding.Y - paddingOpposite.Y)
                };

                renderer->SubmitGUIPanel(trackPos, trackSize, slider.TrackColor, slider.CornerRadius);

                const float range = std::max(0.0001f, slider.Max - slider.Min);
                const float t = std::max(0.0f, std::min(1.0f, (slider.Value - slider.Min) / range));
                Vector2D fillPos = trackPos;
                Vector2D fillSize = trackSize;
                if (slider.Horizontal) {
                    fillSize.X = trackSize.X * t;
                } else {
                    fillSize.Y = trackSize.Y * t;
                }
                renderer->SubmitGUIPanel(fillPos, fillSize, slider.FillColor, slider.CornerRadius);

                Vector2D knobSize = { slider.KnobSize.X * scaleX, slider.KnobSize.Y * scaleY };
                Vector2D knobPos = trackPos;
                if (slider.Horizontal) {
                    knobPos.X = trackPos.X + trackSize.X * t - knobSize.X * 0.5f;
                    knobPos.Y = trackPos.Y + (trackSize.Y - knobSize.Y) * 0.5f;
                } else {
                    knobPos.X = trackPos.X + (trackSize.X - knobSize.X) * 0.5f;
                    knobPos.Y = trackPos.Y + trackSize.Y * t - knobSize.Y * 0.5f;
                }
                renderer->SubmitGUIPanel(knobPos, knobSize, slider.KnobColor, slider.CornerRadius);
                break;
            }
            }
        }
    }

    void GUIRenderSystem::OnDestroy(World& world) {
        (void)world;
    }

    SystemMetadata GUIRenderSystem::GetMetadata() const {
        ComponentAccessBuilder builder("GUIRenderSystem");
        builder.SetExecutionOrder(-10);
        return builder
            .ReadComponent<Components::GUICanvas>()
            .ReadComponent<Components::GUIElement>()
            .ReadComponent<Components::GUIPanel>()
            .ReadComponent<Components::GUIText>()
            .ReadComponent<Components::GUIImage>()
            .ReadComponent<Components::GUIButton>()
            .ReadComponent<Components::GUISlider>()
            .ReadComponent<Components::GUIInput>()
            .Build();
    }
}
