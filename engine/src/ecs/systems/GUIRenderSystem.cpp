#include "ecs/systems/GUIRenderSystem.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "ecs/Components.h"
#include "ecs/systems/RendererSystem.h"
#include "graphics/font.hpp"
#include "graphics/texture.hpp"
#include "services/ResourceManager.h"

namespace ECS {
    namespace {
        enum class GUIState {
            Normal,
            Hovered,
            Pressed,
            Disabled
        };

        // Resolve interaction state from input + disabled flag.
        GUIState ResolveState(const Components::GUIInput* input, bool disabled) {
            if (disabled) {
                return GUIState::Disabled;
            }
            if (input && input->Pressed) {
                return GUIState::Pressed;
            }
            if (input && input->Hovered) {
                return GUIState::Hovered;
            }
            return GUIState::Normal;
        }

        // Pick a style color for the given GUI state (or fallback if no style component).
        Color ResolveStyleColor(const Components::GUIStateStyle* style, const Color& fallback, GUIState state) {
            if (!style) {
                return fallback;
            }

            switch (state) {
            case GUIState::Hovered:
                return style->HoverColor;
            case GUIState::Pressed:
                return style->PressedColor;
            case GUIState::Disabled:
                return style->DisabledColor;
            case GUIState::Normal:
            default:
                return style->NormalColor;
            }
        }

        // Approximate line width using glyph advances and tracking.
        float MeasureLineWidth(const Font& font, std::string_view line, float pixelSize) {
            const float scale = pixelSize / static_cast<float>(font.getPixelSize());
            const float tracking = 1.05f;
            float width = 0.0f;
            for (char c : line) {
                const Glyph& g = font.getGlyph(c);
                width += g.advance * scale * tracking;
            }
            return width;
        }

        // Split text into lines, optionally wrapping to maxWidth.
        std::vector<std::string> WrapText(const Font& font, const std::string& text, float pixelSize, float maxWidth, bool wrap) {
            std::vector<std::string> lines;
            if (text.empty()) {
                return lines;
            }

            const float scale = pixelSize / static_cast<float>(font.getPixelSize());
            const float tracking = 1.05f;

            // Measure a token (word or whitespace chunk).
            auto tokenWidth = [&](std::string_view token) {
                float width = 0.0f;
                for (char c : token) {
                    const Glyph& g = font.getGlyph(c);
                    width += g.advance * scale * tracking;
                }
                return width;
            };

            std::string line;
            float lineWidth = 0.0f;
            std::string token;

            // Append pending token to the line and handle wrapping.
            auto flushToken = [&](bool forceNewLine) {
                if (token.empty()) {
                    return;
                }

                const float tokenW = tokenWidth(token);
                const bool canWrap = wrap && maxWidth > 0.0f;
                if (canWrap && !line.empty() && (lineWidth + tokenW) > maxWidth && !forceNewLine) {
                    lines.push_back(line);
                    line.clear();
                    lineWidth = 0.0f;
                }

                line += token;
                lineWidth += tokenW;
                token.clear();
            };

            // Walk characters and build tokens (whitespace is handled separately).
            for (size_t i = 0; i < text.size(); ++i) {
                const char c = text[i];
                if (c == '\n') {
                    flushToken(true);
                    lines.push_back(line);
                    line.clear();
                    lineWidth = 0.0f;
                    continue;
                }

                if (std::isspace(static_cast<unsigned char>(c))) {
                    flushToken(false);
                    const std::string space(1, c);
                    const float spaceW = tokenWidth(space);
                    const bool canWrap = wrap && maxWidth > 0.0f;
                    if (canWrap && !line.empty() && (lineWidth + spaceW) > maxWidth) {
                        lines.push_back(line);
                        line.clear();
                        lineWidth = 0.0f;
                    }
                    if (!line.empty() || c != ' ') {
                        line += space;
                        lineWidth += spaceW;
                    }
                    continue;
                }

                token.push_back(c);
            }

            // Flush the final token and ensure we emit at least one line.
            flushToken(false);
            if (!line.empty() || lines.empty()) {
                lines.push_back(line);
            }

            return lines;
        }
    }

    void GUIRenderSystem::OnCreate(World& world) {
        (void)world;
    }

    void GUIRenderSystem::OnUpdate(World& world) {
        auto* renderer = RendererSystem::GetInstance();
        if (!renderer) {
            return;
        }

        // Flatten GUI components into a sortable list for stable draw order.
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

        // Sort all GUI elements by Z order before submitting to the renderer.
        std::sort(items.begin(), items.end(),
            [](const RenderItem& a, const RenderItem& b) { return a.zOrder < b.zOrder; });

        for (const auto& item : items) {
            const auto& element = world.Get<Components::GUIElement>(item.entity);
            if (element.ResolvedSize.X <= 0.0f || element.ResolvedSize.Y <= 0.0f) {
                continue;
            }

            // Use resolved size to compute per-element scale for padding, icons, and fonts.
            const float scaleX = element.Size.X > 0.0f ? (element.ResolvedSize.X / element.Size.X) : 1.0f;
            const float scaleY = element.Size.Y > 0.0f ? (element.ResolvedSize.Y / element.Size.Y) : 1.0f;

            // Cache common optional components for styling decisions.
            const Components::GUIInput* input = world.Has<Components::GUIInput>(item.entity)
                ? &world.Get<Components::GUIInput>(item.entity)
                : nullptr;
            const Components::GUIStateStyle* style = world.Has<Components::GUIStateStyle>(item.entity)
                ? &world.Get<Components::GUIStateStyle>(item.entity)
                : nullptr;

            // Detect disabled state from component type (used for styling).
            bool disabled = false;
            if (world.Has<Components::GUIButton>(item.entity)) {
                disabled = world.Get<Components::GUIButton>(item.entity).Disabled;
            } else if (world.Has<Components::GUISlider>(item.entity)) {
                disabled = world.Get<Components::GUISlider>(item.entity).Disabled;
            }
            const GUIState state = ResolveState(input, disabled);

            switch (item.type) {
            case RenderItem::Type::Panel: {
                const auto& panel = world.Get<Components::GUIPanel>(item.entity);
                const Color panelColor = ResolveStyleColor(style, panel.Color, state);
                renderer->SubmitGUIPanel(element.ResolvedPosition, element.ResolvedSize, panelColor, panel.CornerRadius);
                break;
            }
            case RenderItem::Type::Text: {
                const auto& text = world.Get<Components::GUIText>(item.entity);
                const std::string textValue = text.GetText();
                if (textValue.empty()) {
                    break;
                }

                // Load the font to measure text for wrapping/alignment.
                const std::string fontPath = text.GetFontPath();
                const float pixelSize = text.FontSize * scaleX;
                const int fontSize = std::max(1, static_cast<int>(std::round(pixelSize)));
                auto font = RM.GetFont(fontPath.empty() ? "assets/fonts/Roboto/Roboto-Regular.ttf" : fontPath, fontSize);
                if (!font) {
                    break;
                }

                // Build wrapped lines and compute total height for vertical alignment.
                const float maxWidth = text.Wrap ? element.ContentSize.X : 0.0f;
                const auto lines = WrapText(*font, textValue, pixelSize, maxWidth, text.Wrap);
                const float lineHeight = pixelSize * 1.2f;
                const float totalHeight = lineHeight * static_cast<float>(lines.size());

                // Adjust start Y based on vertical alignment inside the content rect.
                float startY = element.ContentPosition.Y;
                if (text.VAlign == Components::GUIText::VerticalAlign::Middle) {
                    startY += (element.ContentSize.Y - totalHeight) * 0.5f;
                } else if (text.VAlign == Components::GUIText::VerticalAlign::Bottom) {
                    startY += (element.ContentSize.Y - totalHeight);
                }

                const Color textColor = ResolveStyleColor(style, text.Color, state);
                for (size_t i = 0; i < lines.size(); ++i) {
                    const std::string& line = lines[i];
                    const float lineWidth = MeasureLineWidth(*font, line, pixelSize);
                    // Adjust start X based on horizontal alignment.
                    float startX = element.ContentPosition.X;
                    if (text.HAlign == Components::GUIText::HorizontalAlign::Center) {
                        startX += (element.ContentSize.X - lineWidth) * 0.5f;
                    } else if (text.HAlign == Components::GUIText::HorizontalAlign::Right) {
                        startX += (element.ContentSize.X - lineWidth);
                    }

                    // Submit each line as its own text draw.
                    const Vector2D linePos = { startX, startY + static_cast<float>(i) * lineHeight };
                    renderer->SubmitGUIText(linePos, line, fontPath, pixelSize, textColor);
                }
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

                // Start from content rect, then adjust for scale mode.
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

                // Apply state style color if present (tinting).
                const Color imageColor = ResolveStyleColor(style, image.Color, state);
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

                    // Build 3x3 slice grid for 9-slice rendering.
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
                        renderer->SubmitGUIImage(slice.pos, slice.size, textureId, slice.uv, imageColor,
                            image.TextureFilter);
                    }
                } else {
                    // Non-sliced image uses a single quad.
                    renderer->SubmitGUIImage(pos, size, textureId, uv, imageColor,
                        image.TextureFilter);
                }
                break;
            }
            case RenderItem::Type::Button: {
                const auto& button = world.Get<Components::GUIButton>(item.entity);
                // Pick background color based on state (or override with GUIStateStyle if present).
                Color bgColor = ResolveStyleColor(style, button.NormalColor, state);
                if (!style) {
                    if (button.Disabled) {
                        bgColor = button.DisabledColor;
                    } else if (button.Toggle && button.Toggled) {
                        bgColor = button.PressedColor;
                    } else if (input && input->Pressed) {
                        bgColor = button.PressedColor;
                    } else if (input && input->Hovered) {
                        bgColor = button.HoverColor;
                    }
                }

                // Background panel (button body).
                renderer->SubmitGUIPanel(element.ResolvedPosition, element.ResolvedSize, bgColor, button.CornerRadius);

                // Offset content by button padding to keep labels/icons aligned.
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
                    // Label text is anchored at the inner content origin.
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
                            // Icons are drawn as a tinted image over the button.
                            renderer->SubmitGUIImage(iconPos, iconSize, iconTex->ID(),
                                Vector4D{ 0.0f, 0.0f, 1.0f, 1.0f }, button.IconColor);
                        }
                    }
                }
                break;
            }
            case RenderItem::Type::Slider: {
                auto& slider = world.Get<Components::GUISlider>(item.entity);
                // Use padding to keep the track away from element edges.
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

                const Color trackColor = ResolveStyleColor(style, slider.TrackColor, state);
                renderer->SubmitGUIPanel(trackPos, trackSize, trackColor, slider.CornerRadius);

                // Fill length is derived from the normalized slider value.
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

                // Knob is centered at the end of the fill for consistent dragging.
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
            .ReadComponent<Components::GUIStateStyle>()
            .Build();
    }
}
