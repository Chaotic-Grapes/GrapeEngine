/* Start Header *****************************************************************/
/*!
\file   GUIRenderSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu

\brief
Definition of the GUIRenderSystem class for rendering GUI elements. Provides
functionality for translating ECS GUI components into draw calls.
*/
/* End Header *******************************************************************/

#include "ecs/systems/GUIRenderSystem.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "ecs/Components.h"
#include "ecs/systems/GUIRenderUtils.h"
#include "ecs/systems/RendererSystem.h"
#include "graphics/font.hpp"
#include "graphics/renderer.hpp"
#include "graphics/texture.hpp"
#include "core/ProjectPaths.h"
#include "services/ResourceManager.h"

namespace ECS {
    namespace {

        /**
         * @brief GUI interaction states.
         */
        enum class GUIState {
            Normal,
            Hovered,
            Pressed,
            Disabled
        };

        /**
         * @brief Determine the GUI state based on input and disabled status.
         * @param input Pointer to GUIInput component (may be nullptr).
         * @param disabled Whether the element is disabled.
         * @return The resolved GUIState.
         */
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

        /**
         * @brief Resolve the color for a GUI element based on its style and state.
         * @param style Pointer to GUIStateStyle component (may be nullptr).
         * @param fallback The fallback color if no style is provided.
         * @param state The current GUIState.
         * @return The resolved Color.
         */
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

        /**
         * @brief Measure the width of a single line of text using the specified font and pixel size.
         * @param font The Font object to use for measurement.
         * @param line The line of text to measure.
         * @param pixelSize The desired pixel size for the font.
         * @return The measured width in pixels.
         */
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

        /**
         * @brief Wrap text into multiple lines based on maximum width and wrapping settings.
         * @param font The Font object to use for measurement.
         * @param text The input text to wrap.
         * @param pixelSize The desired pixel size for the font.
         * @param maxWidth The maximum width in pixels for each line.
         * @param wrap Whether to enable wrapping.
         * @return A vector of strings representing the wrapped lines.
         */
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

                // Check for wrapping.
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

                // Whitespace: flush current token and handle space separately.
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

        std::string ResolveProjectPathForLoad(const std::string& path) {
            if (path.empty() || !Engine::ProjectPaths::IsInitialized()) {
                return path;
            }

            std::filesystem::path fsPath(path);
            if (fsPath.is_absolute()) {
                return path;
            }

            return Engine::ProjectPaths::ToAbsolutePath(path);
        }

    }

    /**
     * @brief Initialize GUI render system state for the world.
     * @param world The ECS world to initialize.
     */
    void GUIRenderSystem::OnCreate(World& world) {
        (void)world;
    }

    /**
     * @brief Update and render all GUI elements in the world.
     * @param world The ECS world containing GUI components.
     */
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

        // Collect all GUI elements with their Z order.
        std::vector<RenderItem> items;
        auto pushItem = [&](Entity entity, const Components::GUIElement& element, RenderItem::Type type) {
            if (!world.IsActiveInHierarchy(entity)) {
                return;
            }
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
            const bool isWorldSpace = (ResolveGUIRenderSpace(world, item.entity) == Components::GUIRenderSpace::World);
            float scaleX = element.Size.X > 0.0f ? (element.ResolvedSize.X / element.Size.X) : 1.0f;
            float scaleY = element.Size.Y > 0.0f ? (element.ResolvedSize.Y / element.Size.Y) : 1.0f;
            const float ySign = isWorldSpace ? -1.0f : 1.0f;
            if (isWorldSpace) {
                scaleX = 1.0f;
                scaleY = 1.0f;
            }

            // Cache common optional components for styling decisions.
            const Components::GUIInput* input = world.TryGet<Components::GUIInput>(item.entity);
            const Components::GUIStateStyle* style = world.TryGet<Components::GUIStateStyle>(item.entity);

            // Detect disabled state from component type (used for styling).
            bool disabled = false;
            if (const auto* button = world.TryGet<Components::GUIButton>(item.entity)) {
                disabled = button->Disabled;
            } else if (const auto* slider = world.TryGet<Components::GUISlider>(item.entity)) {
                disabled = slider->Disabled;
            }
            const GUIState state = ResolveState(input, disabled);

            switch (item.type) {
            case RenderItem::Type::Panel: {
                const auto& panel = world.Get<Components::GUIPanel>(item.entity);
                const Color panelColor = ResolveStyleColor(style, panel.Color, state);
                if (isWorldSpace) {
                    renderer->SubmitWorldGUIPanel(element.ResolvedPosition, element.ResolvedSize, panelColor, panel.CornerRadius);
                } else {
                    renderer->SubmitGUIPanel(element.ResolvedPosition, element.ResolvedSize, panelColor, panel.CornerRadius);
                }
                break;
            }
            case RenderItem::Type::Text: {
                const auto& text = world.Get<Components::GUIText>(item.entity);
                const std::string textValue = text.GetText();
                if (textValue.empty()) {
                    break;
                }

                // Load the font to measure text for wrapping/alignment.
                auto textFontPath = text.GetFontPath();
                const std::string fontPath = textFontPath.empty()
                                                ? std::string("assets/fonts/Roboto/static/Roboto-Regular.ttf")
                                                : textFontPath;
                const std::string fontLoadPath = ResolveProjectPathForLoad(fontPath);
                // Keep screen-space text size in absolute pixels; do not scale with layout.
                const float pixelSize = isWorldSpace ? PixelsToUnits(text.FontSize) : text.FontSize;
                const float fontPixelSize = isWorldSpace ? text.FontSize : pixelSize;
                const int fontSize = std::max(1, static_cast<int>(std::round(fontPixelSize)));
                auto font = RM.GetFont(fontLoadPath, fontSize);

                if (!font) break;

                // Build wrapped lines and compute total height for vertical alignment.
                const float maxWidth = text.Wrap ? element.ContentSize.X : 0.0f;
                const auto lines = WrapText(*font, textValue, pixelSize, maxWidth, text.Wrap);
                const float lineHeight = pixelSize * 1.2f;
                const float totalHeight = lineHeight * static_cast<float>(lines.size());

                // Adjust start Y based on vertical alignment inside the content rect.
                float startY = element.ContentPosition.Y;
                if (text.VAlign == Components::GUIText::VerticalAlign::Middle) {
                    startY += ySign * (element.ContentSize.Y - totalHeight) * 0.5f;
                } else if (text.VAlign == Components::GUIText::VerticalAlign::Bottom) {
                    startY += ySign * (element.ContentSize.Y - totalHeight);
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
                    const Vector2D linePos = { startX, startY + ySign * static_cast<float>(i) * lineHeight };
                    if (isWorldSpace) {
                        renderer->SubmitWorldGUIText(linePos, line, fontLoadPath, pixelSize, textColor);
                    } else {
                        renderer->SubmitGUIText(linePos, line, fontLoadPath, pixelSize, textColor);
                    }
                }
                break;
            }
            case RenderItem::Type::Image: {
                const auto& image = world.Get<Components::GUIImage>(item.entity);
                const std::string path = ECS::StringTable::Resolve(image.TexturePathId);
                const std::string loadPath = ResolveProjectPathForLoad(path);
                uint32_t textureId = image.TextureId;
                std::shared_ptr<Texture> texture;
                if (!loadPath.empty()) {
                    texture = RM.Get<Texture>(loadPath);
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
                    float texW = static_cast<float>(texture->Width());
                    float texH = static_cast<float>(texture->Height());
                    if (isWorldSpace) {
                        texW = PixelsToUnits(texW);
                        texH = PixelsToUnits(texH);
                    }
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
                    Vector4D borderScaled = isWorldSpace
                        ? Vector4D{
                            PixelsToUnits(border.X),
                            PixelsToUnits(border.Y),
                            PixelsToUnits(border.Z),
                            PixelsToUnits(border.W)
                        }
                        : Vector4D{
                            border.X * scaleX,
                            border.Y * scaleY,
                            border.Z * scaleX,
                            border.W * scaleY
                        };
                    const float left = std::min(borderScaled.X, size.X * 0.5f);
                    const float right = std::min(borderScaled.Z, size.X * 0.5f);
                    const float top = std::min(borderScaled.Y, size.Y * 0.5f);
                    const float bottom = std::min(borderScaled.W, size.Y * 0.5f);

                    // Compute adjusted UVs based on border sizes.
                    const float u0 = uv.X;
                    const float v0 = uv.Y;
                    const float u1 = uv.Z;
                    const float v1 = uv.W;
                    const float uLeft = texW > 0.0f ? (u0 + (border.X / texW) * (u1 - u0)) : u0;
                    const float uRight = texW > 0.0f ? (u1 - (border.Z / texW) * (u1 - u0)) : u1;
                    const float vTop = texH > 0.0f ? (v0 + (border.Y / texH) * (v1 - v0)) : v0;
                    const float vBottom = texH > 0.0f ? (v1 - (border.W / texH) * (v1 - v0)) : v1;

                    // Compute slice positions.
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
                        // {Position}, {Size}, {UV}
                        { {x0, y0}, {x1 - x0, y1 - y0}, {u0, v0, uLeft, vTop} },            // Top-left
                        { {x1, y0}, {x2 - x1, y1 - y0}, {uLeft, v0, uRight, vTop} },        // Top-center
                        { {x2, y0}, {x3 - x2, y1 - y0}, {uRight, v0, u1, vTop} },           // Top-right
                        { {x0, y1}, {x1 - x0, y2 - y1}, {u0, vTop, uLeft, vBottom} },       // Middle-left
                        { {x1, y1}, {x2 - x1, y2 - y1}, {uLeft, vTop, uRight, vBottom} },   // Middle-center
                        { {x2, y1}, {x3 - x2, y2 - y1}, {uRight, vTop, u1, vBottom} },      // Middle-right
                        { {x0, y2}, {x1 - x0, y3 - y2}, {u0, vBottom, uLeft, v1} },         // Bottom-left
                        { {x1, y2}, {x2 - x1, y3 - y2}, {uLeft, vBottom, uRight, v1} },     // Bottom-center
                        { {x2, y2}, {x3 - x2, y3 - y2}, {uRight, vBottom, u1, v1} }         // Bottom-right
                    };

                    // Submit each slice as its own quad.
                    for (const auto& slice : slices) {
                        if (slice.size.X <= 0.0f || slice.size.Y <= 0.0f) {
                            continue;
                        }
                        if (isWorldSpace) {
                            renderer->SubmitWorldGUIImage(slice.pos, slice.size, textureId, slice.uv, imageColor,
                                image.TextureFilter);
                        } else {
                            renderer->SubmitGUIImage(slice.pos, slice.size, textureId, slice.uv, imageColor,
                                image.TextureFilter);
                        }
                    }
                } else {
                    // Non-sliced image uses a single quad.
                    if (isWorldSpace) {
                        renderer->SubmitWorldGUIImage(pos, size, textureId, uv, imageColor,
                            image.TextureFilter);
                    } else {
                        renderer->SubmitGUIImage(pos, size, textureId, uv, imageColor,
                            image.TextureFilter);
                    }
                }
                break;
            }
            case RenderItem::Type::Button: {
                const auto& button = world.Get<Components::GUIButton>(item.entity);
                // Pick background color based on GUIStateStyle with sensible fallback palette.
                const Color fallbackNormal{ 0.25f, 0.25f, 0.25f, 1.0f };
                const Color fallbackHover{ 0.35f, 0.35f, 0.35f, 1.0f };
                const Color fallbackPressed{ 0.15f, 0.15f, 0.15f, 1.0f };
                const Color fallbackDisabled{ 0.2f, 0.2f, 0.2f, 0.6f };

                GUIState effectiveState = state;
                if (button.Toggle && button.Toggled && effectiveState != GUIState::Disabled) {
                    effectiveState = GUIState::Pressed;
                }

                Color bgColor = fallbackNormal;
                if (style) {
                    bgColor = ResolveStyleColor(style, fallbackNormal, effectiveState);
                } else {
                    switch (effectiveState) {
                    case GUIState::Hovered:
                        bgColor = fallbackHover;
                        break;
                    case GUIState::Pressed:
                        bgColor = fallbackPressed;
                        break;
                    case GUIState::Disabled:
                        bgColor = fallbackDisabled;
                        break;
                    case GUIState::Normal:
                    default:
                        bgColor = fallbackNormal;
                        break;
                    }
                }

                // Background panel (button body).
                if (isWorldSpace) {
                    renderer->SubmitWorldGUIPanel(element.ResolvedPosition, element.ResolvedSize, bgColor, button.CornerRadius);
                } else {
                    renderer->SubmitGUIPanel(element.ResolvedPosition, element.ResolvedSize, bgColor, button.CornerRadius);
                }

                // Offset content by button padding to keep labels/icons aligned.
                const float buttonPadLeft = isWorldSpace ? PixelsToUnits(button.Padding.X) : button.Padding.X * scaleX;
                const float buttonPadTop = isWorldSpace ? PixelsToUnits(button.Padding.Y) : button.Padding.Y * scaleY;
                const float buttonPadRight = isWorldSpace ? PixelsToUnits(button.Padding.Z) : button.Padding.Z * scaleX;
                const float buttonPadBottom = isWorldSpace ? PixelsToUnits(button.Padding.W) : button.Padding.W * scaleY;
                const Vector2D innerPos = {
                    element.ContentPosition.X + buttonPadLeft,
                    element.ContentPosition.Y + buttonPadTop * ySign
                };
                const Vector2D innerSize = {
                    std::max(0.0f, element.ContentSize.X - buttonPadLeft - buttonPadRight),
                    std::max(0.0f, element.ContentSize.Y - buttonPadTop - buttonPadBottom)
                };

                const std::string label = button.TextId ? ECS::StringTable::Resolve(button.TextId) : std::string();
                if (!label.empty()) {
                    // Label text is anchored at the inner content origin.
                    // Keep screen-space button label size in absolute pixels; do not scale with layout.
                    const float labelSize = isWorldSpace ? PixelsToUnits(button.FontSize) : button.FontSize;
                    const std::string buttonFontPath = button.FontPathId ? ECS::StringTable::Resolve(button.FontPathId) : std::string();
                    const std::string buttonFontLoadPath = ResolveProjectPathForLoad(buttonFontPath);
                    if (isWorldSpace) {
                        renderer->SubmitWorldGUIText(innerPos, label, buttonFontLoadPath, labelSize, button.TextColor);
                    } else {
                        renderer->SubmitGUIText(innerPos, label, buttonFontLoadPath, labelSize, button.TextColor);
                    }
                }

                if (button.IconPathId != 0) {
                    const std::string iconPath = ECS::StringTable::Resolve(button.IconPathId);
                    const std::string iconLoadPath = ResolveProjectPathForLoad(iconPath);
                    if (!iconLoadPath.empty()) {
                        auto iconTex = RM.Get<Texture>(iconLoadPath);
                        if (iconTex) {
                            const Vector2D iconSize = isWorldSpace
                                ? Vector2D{ PixelsToUnits(button.IconSize.X), PixelsToUnits(button.IconSize.Y) }
                                : Vector2D{ button.IconSize.X * scaleX, button.IconSize.Y * scaleY };
                            const float iconOffsetX = isWorldSpace ? PixelsToUnits(button.IconOffset.X) : button.IconOffset.X * scaleX;
                            const float iconOffsetY = isWorldSpace ? PixelsToUnits(button.IconOffset.Y) : button.IconOffset.Y * scaleY;
                            const Vector2D iconPos = {
                                innerPos.X + iconOffsetX,
                                innerPos.Y + iconOffsetY * ySign
                            };
                            // Icons are drawn as a tinted image over the button.
                            if (isWorldSpace) {
                                renderer->SubmitWorldGUIImage(iconPos, iconSize, iconTex->ID(),
                                    Vector4D{ 0.0f, 0.0f, 1.0f, 1.0f }, button.IconColor);
                            } else {
                                renderer->SubmitGUIImage(iconPos, iconSize, iconTex->ID(),
                                    Vector4D{ 0.0f, 0.0f, 1.0f, 1.0f }, button.IconColor);
                            }
                        }
                    }
                }
                break;
            }
            case RenderItem::Type::Slider: {
                auto& slider = world.Get<Components::GUISlider>(item.entity);
                // Use padding to keep the track away from element edges.
                const Vector2D padding = isWorldSpace
                    ? Vector2D{ PixelsToUnits(slider.Padding.X), PixelsToUnits(slider.Padding.Y) }
                    : Vector2D{ slider.Padding.X * scaleX, slider.Padding.Y * scaleY };
                const Vector2D paddingOpposite = isWorldSpace
                    ? Vector2D{ PixelsToUnits(slider.Padding.Z), PixelsToUnits(slider.Padding.W) }
                    : Vector2D{ slider.Padding.Z * scaleX, slider.Padding.W * scaleY };
                Vector2D trackPos = {
                    element.ContentPosition.X + padding.X,
                    element.ContentPosition.Y + padding.Y * ySign
                };
                Vector2D trackSize = {
                    std::max(0.0f, element.ContentSize.X - padding.X - paddingOpposite.X),
                    std::max(0.0f, element.ContentSize.Y - padding.Y - paddingOpposite.Y)
                };

                const float rotationRadians = slider.RotationDegrees * (3.14159265358979323846f / 180.0f);
                const float cosRot = std::cos(rotationRadians);
                const float sinRot = std::sin(rotationRadians);
                const Vector2D trackCenter = {
                    trackPos.X + trackSize.X * 0.5f,
                    trackPos.Y + trackSize.Y * 0.5f
                };

                auto rotatePointAroundTrackCenter = [&](const Vector2D& point) {
                    const float dx = point.X - trackCenter.X;
                    const float dy = point.Y - trackCenter.Y;
                    return Vector2D{
                        trackCenter.X + dx * cosRot - dy * sinRot,
                        trackCenter.Y + dx * sinRot + dy * cosRot
                    };
                };

                auto resolveSliderTextureId = [&](uint32_t runtimeId, uint32_t pathId) {
                    // Prefer path-backed texture when available so serialized slider assets reload correctly.
                    uint32_t textureId = runtimeId;
                    const std::string texturePath = ECS::StringTable::Resolve(pathId);
                    const std::string loadPath = ResolveProjectPathForLoad(texturePath);
                    if (!loadPath.empty()) {
                        auto texture = RM.Get<Texture>(loadPath);
                        if (texture) {
                            textureId = texture->ID();
                        }
                    }
                    return textureId;
                };

                const uint32_t trackTextureId = resolveSliderTextureId(slider.TrackTextureId, slider.TrackTexturePathId);
                const uint32_t fillTextureId = resolveSliderTextureId(slider.FillTextureId, slider.FillTexturePathId);
                const uint32_t knobTextureId = resolveSliderTextureId(slider.KnobTextureId, slider.KnobTexturePathId);
                // Slider textures intentionally use full UVs; per-part UV editing is not supported.
                const Vector4D fullUVRect{ 0.0f, 0.0f, 1.0f, 1.0f };

                const Color trackColor = ResolveStyleColor(style, slider.TrackColor, state);
                if (trackTextureId != 0u) {
                    // Texture path: draw image part. Fallback path below keeps previous color-panel behavior.
                    if (isWorldSpace) {
                        renderer->SubmitWorldGUIImage(trackPos, trackSize, trackTextureId, fullUVRect, trackColor,
                            slider.TrackTextureFilter, rotationRadians);
                    } else {
                        renderer->SubmitGUIImage(trackPos, trackSize, trackTextureId, fullUVRect, trackColor,
                            slider.TrackTextureFilter, rotationRadians);
                    }
                } else if (isWorldSpace) {
                    renderer->SubmitWorldGUIPanel(trackPos, trackSize, trackColor, slider.CornerRadius, rotationRadians);
                } else {
                    renderer->SubmitGUIPanel(trackPos, trackSize, trackColor, slider.CornerRadius, rotationRadians);
                }

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

                const Vector2D fillCenter = {
                    fillPos.X + fillSize.X * 0.5f,
                    fillPos.Y + fillSize.Y * 0.5f
                };
                const Vector2D rotatedFillCenter = rotatePointAroundTrackCenter(fillCenter);
                fillPos = {
                    rotatedFillCenter.X - fillSize.X * 0.5f,
                    rotatedFillCenter.Y - fillSize.Y * 0.5f
                };
                if (fillTextureId != 0u) {
                    if (isWorldSpace) {
                        renderer->SubmitWorldGUIImage(fillPos, fillSize, fillTextureId, fullUVRect, slider.FillColor,
                            slider.FillTextureFilter, rotationRadians);
                    } else {
                        renderer->SubmitGUIImage(fillPos, fillSize, fillTextureId, fullUVRect, slider.FillColor,
                            slider.FillTextureFilter, rotationRadians);
                    }
                } else if (isWorldSpace) {
                    renderer->SubmitWorldGUIPanel(fillPos, fillSize, slider.FillColor, slider.CornerRadius, rotationRadians);
                } else {
                    renderer->SubmitGUIPanel(fillPos, fillSize, slider.FillColor, slider.CornerRadius, rotationRadians);
                }

                // Knob is centered at the end of the fill for consistent dragging.
                Vector2D knobSize = isWorldSpace
                    ? Vector2D{ PixelsToUnits(slider.KnobSize.X), PixelsToUnits(slider.KnobSize.Y) }
                    : Vector2D{ slider.KnobSize.X * scaleX, slider.KnobSize.Y * scaleY };
                Vector2D knobPos = trackPos;
                if (slider.Horizontal) {
                    knobPos.X = trackPos.X + trackSize.X * t - knobSize.X * 0.5f;
                    knobPos.Y = trackPos.Y + (trackSize.Y - knobSize.Y) * 0.5f;
                } else {
                    knobPos.X = trackPos.X + (trackSize.X - knobSize.X) * 0.5f;
                    knobPos.Y = trackPos.Y + trackSize.Y * t - knobSize.Y * 0.5f;
                }

                const Vector2D knobCenter = {
                    knobPos.X + knobSize.X * 0.5f,
                    knobPos.Y + knobSize.Y * 0.5f
                };
                const Vector2D rotatedKnobCenter = rotatePointAroundTrackCenter(knobCenter);
                knobPos = {
                    rotatedKnobCenter.X - knobSize.X * 0.5f,
                    rotatedKnobCenter.Y - knobSize.Y * 0.5f
                };
                if (knobTextureId != 0u) {
                    if (isWorldSpace) {
                        renderer->SubmitWorldGUIImage(knobPos, knobSize, knobTextureId, fullUVRect, slider.KnobColor,
                            slider.KnobTextureFilter, rotationRadians);
                    } else {
                        renderer->SubmitGUIImage(knobPos, knobSize, knobTextureId, fullUVRect, slider.KnobColor,
                            slider.KnobTextureFilter, rotationRadians);
                    }
                } else if (isWorldSpace) {
                    renderer->SubmitWorldGUIPanel(knobPos, knobSize, slider.KnobColor, slider.CornerRadius, rotationRadians);
                } else {
                    renderer->SubmitGUIPanel(knobPos, knobSize, slider.KnobColor, slider.CornerRadius, rotationRadians);
                }
                break;
            }
            }
        }
    }

    // Tear down GUI render system state.
    void GUIRenderSystem::OnDestroy(World& world) {
        (void)world;
    }

    // Return metadata used for system registration.
    SystemMetadata GUIRenderSystem::GetMetadata() const {
        ComponentAccessBuilder builder("GUIRenderSystem");
        builder.SetExecutionOrder(-10);
        return builder
            .ReadComponent<Components::Active>()
            .ReadComponent<Components::Parent>()
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
