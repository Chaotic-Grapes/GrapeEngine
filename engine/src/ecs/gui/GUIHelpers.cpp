/* Start Header *****************************************************************/
/*!
\file   GUIHelpers.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of GUI helper utilities including factory functions, conversion
utilities, and style management.

This implementation provides:
- Factory functions for creating common GUI elements
- Utility functions for hit testing and conversions
- Text measurement and manipulation utilities
- Predefined color palettes and style helpers
- Easing and animation functions

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/gui/GUIHelpers.h"
#include "ecs/gui/GUILayout.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/StringTable.h"
#include "core/Logger.h"
#include <cmath>

namespace ECS {
    namespace GUI {

        // ====================================================================
        // GUIFactory Implementation
        // ====================================================================

        Entity GUIFactory::CreateButton(
            World& world,
            Vector2D position,
            Vector2D size,
            const std::string& label,
            uint32_t actionID,
            const std::string& name) {
            
            Entity button = world.Create();

            // Add base element
            auto& element = world.Add<GUIElement>(button);
            element.Position = position;
            element.Size = size;
            element.ElementType = GUIElementType::Button;

            // Add button component
            auto& guiBtn = world.Add<GUIButton>(button);
            guiBtn.ActionID = actionID;
            guiBtn.Interactable = true;
            guiBtn.State = ButtonState::Normal;

            // Add container for hierarchy
            auto& container = world.Add<GUIContainer>(button);

            // Add panel for visual style
            auto& panel = world.Add<GUIPanel>(button);
            panel.BackgroundColor = guiBtn.ColorNormal;
            panel.BorderThickness = 1.0f;

            // Add text label
            if (!label.empty()) {
                auto& text = world.Add<GUIText>(button);
                strncpy_s(text.Content, label.c_str(), std::min(label.length(), text.MaxTextLength - 1));
                text.Content[text.MaxTextLength - 1] = '\0';
                text.FontColor = Color{1.0f, 1.0f, 1.0f, 1.0f};
                text.Alignment = GUIText::TextAlignment::Center;
                text.FontSize = 16.0f;
                strncpy_s(guiBtn.Label, label.c_str(), sizeof(guiBtn.Label) - 1);
                guiBtn.Label[sizeof(guiBtn.Label) - 1] = '\0';
            }

            // Assign name component
            ECS::Components::Name nm{};
            std::string finalName = !name.empty() ? name : label.empty() ? "Entity" : label;
            nm.Value = ECS::StringTable::Intern(finalName);
            world.Set<ECS::Components::Name>(button, nm);

            return button;
        }

        Entity GUIFactory::CreatePanel(
            World& world,
            Vector2D position,
            Vector2D size,
            Color backgroundColor,
            const std::string& name) {

            Entity panel = world.Create();

            auto& element = world.Add<GUIElement>(panel);
            element.Position = position;
            element.Size = size;
            element.ElementType = GUIElementType::Panel;

            auto& panelComp = world.Add<GUIPanel>(panel);
            panelComp.BackgroundColor = backgroundColor;

            auto& container = world.Add<GUIContainer>(panel);
            container.Layout = LayoutType::VerticalBox;

            // Assign to UI layer
            world.Set<ECS::Components::Layer>(panel, ECS::Components::Layer{ 98 });

            // Assign name
            ECS::Components::Name nm{};
            std::string finalName = !name.empty() ? name : "Panel";
            nm.Value = ECS::StringTable::Intern(finalName);
            world.Set<ECS::Components::Name>(panel, nm);

            return panel;
        }

        Entity GUIFactory::CreateLabel(
            World& world,
            Vector2D position,
            const std::string& text,
            float fontSize,
            const std::string& name) {
            
            Entity label = world.Create();

            auto& element = world.Add<GUIElement>(label);
            element.Position = position;
            element.Size = {200.0f, 30.0f};  // Default size
            element.ElementType = GUIElementType::Text;

            auto& textComp = world.Add<GUIText>(label);
            strncpy_s(textComp.Content, text.c_str(), std::min(text.length(), textComp.MaxTextLength - 1));
            textComp.Content[textComp.MaxTextLength - 1] = '\0';
            textComp.FontSize = fontSize;
            textComp.FontColor = Color{1.0f, 1.0f, 1.0f, 1.0f};

            auto& container = world.Add<GUIContainer>(label);

            // Assign to UI layer
            world.Set<ECS::Components::Layer>(label, ECS::Components::Layer{ 98 });

            // Assign name (use provided name or text)
            ECS::Components::Name nm{};
            std::string finalName = !name.empty() ? name : text.empty() ? "Label" : text;
            nm.Value = ECS::StringTable::Intern(finalName);
            world.Set<ECS::Components::Name>(label, nm);
            return label;
        }

        Entity GUIFactory::CreateInputField(
            World& world,
            Vector2D position,
            Vector2D size,
            const std::string& placeholder,
            const std::string& name) {
            
            Entity inputField = world.Create();

            auto& element = world.Add<GUIElement>(inputField);
            element.Position = position;
            element.Size = size;
            element.ElementType = GUIElementType::InputField;

            auto& input = world.Add<GUIInputField>(inputField);
            input.Interactable = true;
            strncpy_s(input.Placeholder, placeholder.c_str(), 
                     std::min(placeholder.length(), (size_t)255));
            input.Placeholder[255] = '\0';

            auto& panel = world.Add<GUIPanel>(inputField);
            panel.BackgroundColor = Color{0.1f, 0.1f, 0.1f, 1.0f};
            panel.BorderThickness = 1.0f;

            auto& container = world.Add<GUIContainer>(inputField);

                        // Assign to UI layer
                            world.Set<ECS::Components::Layer>(inputField, ECS::Components::Layer{ 98 });

                            // Assign name
                            ECS::Components::Name nm{};
                            std::string finalName = !name.empty() ? name : "InputField";
                            nm.Value = ECS::StringTable::Intern(finalName);
                            world.Set<ECS::Components::Name>(inputField, nm);
                            return inputField;
        }

        Entity GUIFactory::CreateSlider(
            World& world,
            Vector2D position,
            float width,
            float minValue,
            float maxValue,
            float initialValue,
            uint32_t actionID,
            const std::string& name) {
            
            Entity slider = world.Create();

            auto& element = world.Add<GUIElement>(slider);
            element.Position = position;
            element.Size = {width, 30.0f};
            element.ElementType = GUIElementType::Slider;

            auto& sliderComp = world.Add<GUISlider>(slider);
            sliderComp.MinValue = minValue;
            sliderComp.MaxValue = maxValue;
            sliderComp.CurrentValue = initialValue;
            sliderComp.ActionID = actionID;
            sliderComp.Interactable = true;

            auto& panel = world.Add<GUIPanel>(slider);
            panel.BackgroundColor = sliderComp.BackgroundColor;

            auto& container = world.Add<GUIContainer>(slider);

                        // Assign to UI layer
                            world.Set<ECS::Components::Layer>(slider, ECS::Components::Layer{ 98 });

                            // Assign name
                            ECS::Components::Name nm{};
                            std::string finalName = !name.empty() ? name : "Slider";
                            nm.Value = ECS::StringTable::Intern(finalName);
                            world.Set<ECS::Components::Name>(slider, nm);
                            return slider;
        }

        Entity GUIFactory::CreateCheckbox(
            World& world,
            Vector2D position,
            const std::string& label,
            uint32_t actionID,
            const std::string& name) {
            
            Entity checkbox = world.Create();

            auto& element = world.Add<GUIElement>(checkbox);
            element.Position = position;
            element.Size = {30.0f, 30.0f};
            element.ElementType = GUIElementType::Checkbox;

            auto& checkboxComp = world.Add<GUICheckbox>(checkbox);
            checkboxComp.ActionID = actionID;
            checkboxComp.Interactable = true;
            if (!label.empty()) {
                strncpy_s(checkboxComp.Label, label.c_str(), sizeof(checkboxComp.Label) - 1);
                checkboxComp.Label[sizeof(checkboxComp.Label) - 1] = '\0';
            }

            auto& container = world.Add<GUIContainer>(checkbox);

                // Assign to UI layer
                world.Set<ECS::Components::Layer>(checkbox, ECS::Components::Layer{ 98 });

                // Assign name
                ECS::Components::Name nm{};
                std::string finalName = !name.empty() ? name : label.empty() ? "Checkbox" : label;
                nm.Value = ECS::StringTable::Intern(finalName);
                world.Set<ECS::Components::Name>(checkbox, nm);
                return checkbox;
        }

        Entity GUIFactory::CreateDropdown(
            World& world,
            Vector2D position,
            float width,
            const std::string& options,
            uint32_t actionID,
            const std::string& name) {
            
            Entity dropdown = world.Create();

            auto& element = world.Add<GUIElement>(dropdown);
            element.Position = position;
            element.Size = {width, 40.0f};
            element.ElementType = GUIElementType::Dropdown;

            auto& dropdownComp = world.Add<GUIDropdown>(dropdown);
            dropdownComp.ActionID = actionID;
            dropdownComp.Interactable = true;
            strncpy_s(dropdownComp.Options, options.c_str(), sizeof(dropdownComp.Options) - 1);
            dropdownComp.Options[sizeof(dropdownComp.Options) - 1] = '\0';

            // Count options (newline-separated)
            dropdownComp.OptionCount = 1;
            for (char c : options) {
                if (c == '\n') {
                    dropdownComp.OptionCount++;
                }
            }

            auto& panel = world.Add<GUIPanel>(dropdown);
            panel.BackgroundColor = dropdownComp.BackgroundColor;

            auto& container = world.Add<GUIContainer>(dropdown);

                // Assign to UI layer
                world.Set<ECS::Components::Layer>(dropdown, ECS::Components::Layer{ 98 });

                // Assign name
                ECS::Components::Name nm{};
                std::string finalName = !name.empty() ? name : "Dropdown";
                nm.Value = ECS::StringTable::Intern(finalName);
                world.Set<ECS::Components::Name>(dropdown, nm);
                return dropdown;
        }

        Entity GUIFactory::CreateContainer(
            World& world,
            Vector2D position,
            Vector2D size,
            LayoutType layoutType,
            const std::string& name) {
            
            Entity container = world.Create();

            auto& element = world.Add<GUIElement>(container);
            element.Position = position;
            element.Size = size;
            element.ElementType = GUIElementType::Container;

            auto& containerComp = world.Add<GUIContainer>(container);
            containerComp.Layout = layoutType;

              // Assign to UI layer
              world.Set<ECS::Components::Layer>(container, ECS::Components::Layer{ 98 });

              // Assign name
              ECS::Components::Name nm{};
              std::string finalName = !name.empty() ? name : "Container";
              nm.Value = ECS::StringTable::Intern(finalName);
              world.Set<ECS::Components::Name>(container, nm);
              return container;
        }

        Entity GUIFactory::CreateSeparator(
            World& world,
            Vector2D position,
            float length,
            bool horizontal,
            const std::string& name) {
            
            Entity separator = world.Create();

            auto& element = world.Add<GUIElement>(separator);
            element.Position = position;
            element.Size = horizontal ? Vector2D{length, 1.0f} : Vector2D{1.0f, length};
            element.ElementType = GUIElementType::Separator;

            auto& sepComp = world.Add<GUISeparator>(separator);
            sepComp.Orient = horizontal ? GUISeparator::Orientation::Horizontal : 
                                         GUISeparator::Orientation::Vertical;

            auto& container = world.Add<GUIContainer>(separator);

              // Assign to UI layer
              world.Set<ECS::Components::Layer>(separator, ECS::Components::Layer{ 98 });

              // Assign name
              ECS::Components::Name nm{};
              std::string finalName = !name.empty() ? name : "Separator";
              nm.Value = ECS::StringTable::Intern(finalName);
              world.Set<ECS::Components::Name>(separator, nm);
              return separator;
        }

        // ====================================================================
        // GUIUtils Implementation
        // ====================================================================

        bool GUIUtils::PointInRect(
            Vector2D point,
            Vector2D rectPos,
            Vector2D rectSize) {
            
            return point.X >= rectPos.X && point.X <= rectPos.X + rectSize.X &&
                   point.Y >= rectPos.Y && point.Y <= rectPos.Y + rectSize.Y;
        }

        bool GUIUtils::PointInCircle(
            Vector2D point,
            Vector2D circlePos,
            float radius) {
            
            float dx = point.X - circlePos.X;
            float dy = point.Y - circlePos.Y;
            float distSquared = dx * dx + dy * dy;
            return distSquared <= radius * radius;
        }

        std::pair<Vector2D, Vector2D> GUIUtils::GetRectIntersection(
            Vector2D rect1Pos, Vector2D rect1Size,
            Vector2D rect2Pos, Vector2D rect2Size) {
            
            float left = std::max(rect1Pos.X, rect2Pos.X);
            float right = std::min(rect1Pos.X + rect1Size.X, rect2Pos.X + rect2Size.X);
            float top = std::max(rect1Pos.Y, rect2Pos.Y);
            float bottom = std::min(rect1Pos.Y + rect1Size.Y, rect2Pos.Y + rect2Size.Y);

            if (left < right && top < bottom) {
                return {{left, top}, {right - left, bottom - top}};
            }

            return {{0, 0}, {0, 0}};
        }

        Color GUIUtils::LerpColor(const Color& colorA, const Color& colorB, float t) {
            t = std::max(0.0f, std::min(1.0f, t));
            return Color{
                colorA.R + (colorB.R - colorA.R) * t,
                colorA.G + (colorB.G - colorA.G) * t,
                colorA.B + (colorB.B - colorA.B) * t,
                colorA.A + (colorB.A - colorA.A) * t
            };
        }

        float GUIUtils::Ease(float t, EaseType type) {
            t = std::max(0.0f, std::min(1.0f, t));

            switch (type) {
                case EaseType::Linear:
                    return t;

                case EaseType::EaseInQuad:
                    return t * t;

                case EaseType::EaseOutQuad:
                    return 1.0f - (1.0f - t) * (1.0f - t);

                case EaseType::EaseInOutQuad:
                    if (t < 0.5f) {
                        return 2.0f * t * t;
                    } else {
                        return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
                    }

                case EaseType::EaseInCubic:
                    return t * t * t;

                case EaseType::EaseOutCubic:
                    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);

                case EaseType::EaseInOutCubic:
                    if (t < 0.5f) {
                        return 4.0f * t * t * t;
                    } else {
                        return 1.0f - 4.0f * (1.0f - t) * (1.0f - t) * (1.0f - t);
                    }

                default:
                    return t;
            }
        }

        Vector2D GUIUtils::ScreenToCanvas(Vector2D screenPos, Vector2D canvasSize) {
            // Assuming screen coordinates match canvas coordinates directly
            // Adjust if your rendering uses different coordinate systems
            return screenPos;
        }

        Vector2D GUIUtils::CanvasToScreen(Vector2D canvasPos, Vector2D canvasSize) {
            // Assuming canvas coordinates match screen coordinates directly
            return canvasPos;
        }

        // ====================================================================
        // GUITextUtils Implementation
        // ====================================================================

        Vector2D GUITextUtils::MeasureText(
            const std::string& text,
            const std::string& fontPath,
            float fontSize) {
            
            // TODO: Implement actual text measurement using font system
            // For now, return estimated size
            float width = text.length() * fontSize * 0.6f;
            float height = fontSize;
            return {width, height};
        }

        std::string GUITextUtils::TruncateText(
            const std::string& text,
            const std::string& fontPath,
            float fontSize,
            float maxWidth,
            const std::string& suffix) {
            
            // TODO: Implement proper text truncation with font measurement
            if (text.length() * fontSize * 0.6f <= maxWidth) {
                return text;
            }

            size_t maxChars = static_cast<size_t>(maxWidth / (fontSize * 0.6f)) - suffix.length();
            return text.substr(0, maxChars) + suffix;
        }

        std::vector<std::string> GUITextUtils::WrapText(
            const std::string& text,
            const std::string& fontPath,
            float fontSize,
            float maxWidth) {
            
            std::vector<std::string> lines;
            
            // TODO: Implement proper text wrapping with word boundaries
            // For now, simple character-based wrapping
            size_t charsPerLine = static_cast<size_t>(maxWidth / (fontSize * 0.6f));
            
            for (size_t i = 0; i < text.length(); i += charsPerLine) {
                lines.push_back(text.substr(i, charsPerLine));
            }

            if (lines.empty()) {
                lines.push_back("");
            }

            return lines;
        }

        std::pair<Vector2D, float> GUITextUtils::GetTextSizeWithBestFit(
            const std::string& text,
            const std::string& fontPath,
            float minFontSize,
            float maxFontSize,
            float targetWidth,
            float targetHeight) {
            
            // Binary search for best-fit font size
            float bestFit = minFontSize;
            Vector2D bestSize = MeasureText(text, fontPath, minFontSize);

            for (float fontSize = minFontSize; fontSize <= maxFontSize; fontSize += 1.0f) {
                Vector2D size = MeasureText(text, fontPath, fontSize);
                if (size.X <= targetWidth && size.Y <= targetHeight) {
                    bestFit = fontSize;
                    bestSize = size;
                }
            }

            return {bestSize, bestFit};
        }

        // ====================================================================
        // GUIStyle Implementation
        // ====================================================================

        const Color GUIStyle::PrimaryColor = Color{0.2f, 0.7f, 1.0f, 1.0f};
        const Color GUIStyle::SecondaryColor = Color{1.0f, 0.6f, 0.2f, 1.0f};
        const Color GUIStyle::BackgroundColor = Color{0.15f, 0.15f, 0.15f, 1.0f};
        const Color GUIStyle::TextColor = Color{1.0f, 1.0f, 1.0f, 1.0f};
        const Color GUIStyle::BorderColor = Color{0.5f, 0.5f, 0.5f, 1.0f};
        const Color GUIStyle::HighlightColor = Color{0.2f, 0.5f, 1.0f, 1.0f};
        const Color GUIStyle::DisabledColor = Color{0.3f, 0.3f, 0.3f, 0.5f};
        const Color GUIStyle::ErrorColor = Color{1.0f, 0.2f, 0.2f, 1.0f};
        const Color GUIStyle::SuccessColor = Color{0.2f, 1.0f, 0.2f, 1.0f};

        Color GUIStyle::GetButtonColor(ButtonState state) {
            switch (state) {
                case ButtonState::Normal:
                    return Color{0.3f, 0.3f, 0.3f, 1.0f};
                case ButtonState::Hovered:
                    return Color{0.4f, 0.4f, 0.4f, 1.0f};
                case ButtonState::Pressed:
                    return Color{0.2f, 0.2f, 0.2f, 1.0f};
                case ButtonState::Disabled:
                    return Color{0.15f, 0.15f, 0.15f, 0.5f};
                default:
                    return Color{0.3f, 0.3f, 0.3f, 1.0f};
            }
        }

        Color GUIStyle::GetTransitionColor(
            const Color& startColor,
            const Color& endColor,
            float elapsedTime,
            float totalTime) {
            
            float t = (totalTime > 0.0f) ? (elapsedTime / totalTime) : 1.0f;
            return GUIUtils::LerpColor(startColor, endColor, t);
        }

    } // namespace GUI
} // namespace ECS
