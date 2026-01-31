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

#include "ecs/ui/GUIHelpers.h"
#include "ecs/ui/GUILayout.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/StringTable.h"
#include "core/Logger.h"
#include <algorithm>
#include <cmath>

namespace ECS {
    namespace UI {

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
            auto& element = world.Add<Components::GUIElement>(button);
            element.Position = position;
            element.Size = size;
            element.ElementType = Components::GUIElementType::Button;

            // Add button component
            auto& guiBtn = world.Add<Components::GUIButton>(button);
            guiBtn.ActionID = actionID;
            guiBtn.Interactable = true;
            guiBtn.State = Components::ButtonState::Normal;

            // Add container for hierarchy
            auto& container = world.Add<Components::GUIContainer>(button);
            world.Add<Components::GUIChildList>(button);

            // Add panel for visual style
            auto& panel = world.Add<Components::GUIPanel>(button);
            panel.BackgroundColor = guiBtn.ColorNormal;
            panel.BorderThickness = 1.0f;

            // Add text label
            if (!label.empty()) {
                auto& text = world.Add<Components::GUIText>(button);
                text.Content = ECS::StringTable::Intern(label);
                text.FontColor = Color{1.0f, 1.0f, 1.0f, 1.0f};
                text.Alignment = Components::GUIText::TextAlignment::Center;
                text.FontSize = 16.0f;
                guiBtn.Label = ECS::StringTable::Intern(label);
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

            auto& element = world.Add<Components::GUIElement>(panel);
            element.Position = position;
            element.Size = size;
            element.ElementType = Components::GUIElementType::Panel;

            auto& panelComp = world.Add<Components::GUIPanel>(panel);
            panelComp.BackgroundColor = backgroundColor;

            auto& container = world.Add<Components::GUIContainer>(panel);
            container.Layout = Components::LayoutType::VerticalBox;
            world.Add<Components::GUIChildList>(panel);

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

            auto& element = world.Add<Components::GUIElement>(label);
            element.Position = position;
            element.Size = {200.0f, 30.0f};  // Default size
            element.ElementType = Components::GUIElementType::Text;

            auto& textComp = world.Add<Components::GUIText>(label);
            textComp.Content = text.empty() ? 0 : ECS::StringTable::Intern(text);
            textComp.FontSize = fontSize;
            textComp.FontColor = Color{1.0f, 1.0f, 1.0f, 1.0f};

            auto& container = world.Add<Components::GUIContainer>(label);
            world.Add<Components::GUIChildList>(label);

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

            auto& element = world.Add<Components::GUIElement>(inputField);
            element.Position = position;
            element.Size = size;
            element.ElementType = Components::GUIElementType::InputField;

            auto& input = world.Add<Components::GUIInputField>(inputField);
            input.Interactable = true;
            input.Placeholder = placeholder.empty() ? 0 : ECS::StringTable::Intern(placeholder);

            auto& panel = world.Add<Components::GUIPanel>(inputField);
            panel.BackgroundColor = Color{0.1f, 0.1f, 0.1f, 1.0f};
            panel.BorderThickness = 1.0f;

            auto& container = world.Add<Components::GUIContainer>(inputField);
            world.Add<Components::GUIChildList>(inputField);

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

            auto& element = world.Add<Components::GUIElement>(slider);
            element.Position = position;
            element.Size = {width, 30.0f};
            element.ElementType = Components::GUIElementType::Slider;

            auto& sliderComp = world.Add<Components::GUISlider>(slider);
            sliderComp.MinValue = minValue;
            sliderComp.MaxValue = maxValue;
            sliderComp.CurrentValue = initialValue;
            sliderComp.ActionID = actionID;
            sliderComp.Interactable = true;

            auto& panel = world.Add<Components::GUIPanel>(slider);
            panel.BackgroundColor = sliderComp.BackgroundColor;

            auto& container = world.Add<Components::GUIContainer>(slider);
            world.Add<Components::GUIChildList>(slider);

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

            auto& element = world.Add<Components::GUIElement>(checkbox);
            element.Position = position;
            element.Size = {30.0f, 30.0f};
            element.ElementType = Components::GUIElementType::Checkbox;

            auto& checkboxComp = world.Add<Components::GUICheckbox>(checkbox);
            checkboxComp.ActionID = actionID;
            checkboxComp.Interactable = true;
            if (!label.empty()) {
                checkboxComp.Label = ECS::StringTable::Intern(label);
            }

            auto& container = world.Add<Components::GUIContainer>(checkbox);
            world.Add<Components::GUIChildList>(checkbox);

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

            auto& element = world.Add<Components::GUIElement>(dropdown);
            element.Position = position;
            element.Size = {width, 40.0f};
            element.ElementType = Components::GUIElementType::Dropdown;

            auto& dropdownComp = world.Add<Components::GUIDropdown>(dropdown);
            dropdownComp.ActionID = actionID;
            dropdownComp.Interactable = true;
            dropdownComp.Options = options.empty() ? 0 : ECS::StringTable::Intern(options);

            // Count options (newline-separated)
            if (!options.empty()) {
                dropdownComp.OptionCount = 1;
                for (char c : options) {
                    if (c == '\n') {
                        dropdownComp.OptionCount++;
                    }
                }
            } else {
                dropdownComp.OptionCount = 0;
            }

            auto& panel = world.Add<Components::GUIPanel>(dropdown);
            panel.BackgroundColor = dropdownComp.BackgroundColor;

            auto& container = world.Add<Components::GUIContainer>(dropdown);
            world.Add<Components::GUIChildList>(dropdown);

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
            Components::LayoutType layoutType,
            const std::string& name) {
            
            Entity container = world.Create();

            auto& element = world.Add<Components::GUIElement>(container);
            element.Position = position;
            element.Size = size;
            element.ElementType = Components::GUIElementType::Container;

            auto& containerComp = world.Add<Components::GUIContainer>(container);
            containerComp.Layout = layoutType;
            world.Add<Components::GUIChildList>(container);

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

            auto& element = world.Add<Components::GUIElement>(separator);
            element.Position = position;
            element.Size = horizontal ? Vector2D{length, 1.0f} : Vector2D{1.0f, length};
            element.ElementType = Components::GUIElementType::Separator;

            auto& sepComp = world.Add<Components::GUISeparator>(separator);
            sepComp.Orient = horizontal ? Components::GUISeparator::Orientation::Horizontal : 
                                         Components::GUISeparator::Orientation::Vertical;

            auto& container = world.Add<Components::GUIContainer>(separator);
            world.Add<Components::GUIChildList>(separator);

            // Assign to UI layer
            world.Set<ECS::Components::Layer>(separator, ECS::Components::Layer{ 98 });

            // Assign name
            ECS::Components::Name nm{};
            std::string finalName = !name.empty() ? name : "Separator";
            nm.Value = ECS::StringTable::Intern(finalName);
            world.Set<ECS::Components::Name>(separator, nm);
            return separator;
        }

        void GUIFactory::AttachChild(World& world, Entity parent, Entity child) {
            if (parent.IsNull() || child.IsNull()) {
                return;
            }

            if (!world.Has<Components::GUIChildList>(parent)) {
                world.Add<Components::GUIChildList>(parent);
            }

            auto& childList = world.Get<Components::GUIChildList>(parent);
            for (uint16_t i = 0; i < childList.ChildCount; ++i) {
                if (childList.Children[i] == child) {
                    Components::Parent parentComp{};
                    parentComp.ParentEntity = parent;
                    world.Set<Components::Parent>(child, parentComp);
                    return;
                }
            }

            if (childList.ChildCount >= Components::GUIChildList::MaxChildren) {
                LOG_WARNING("GUIFactory::AttachChild: MaxChildren reached for parent " << parent.Index);
                return;
            }

            childList.Children[childList.ChildCount++] = child;

            Components::Parent parentComp{};
            parentComp.ParentEntity = parent;
            world.Set<Components::Parent>(child, parentComp);
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
            
            // Use 96px as the standard SDF size, matching RendererSystem
            const int StandardSDFSize = 96;
            std::string actualPath = fontPath.empty() ? "assets/fonts/arial.ttf" : fontPath;
            
            auto font = RM.GetFont(actualPath, StandardSDFSize);
            if (!font) {
                // Fallback estimation if font fails to load
                float width = text.length() * fontSize * 0.6f;
                float height = fontSize;
                return {width, height};
            }

            float scale = fontSize / static_cast<float>(StandardSDFSize);
            float width = 0.0f;
            float height = font->getLineHeight() * scale;

            for (char c : text) {
                const auto& glyph = font->getGlyph(c);
                width += glyph.advance * scale;
            }

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

        Color GUIStyle::GetButtonColor(Components::ButtonState state) {
            switch (state) {
                case Components::ButtonState::Normal:
                    return Color{0.3f, 0.3f, 0.3f, 1.0f};
                case Components::ButtonState::Hovered:
                    return Color{0.4f, 0.4f, 0.4f, 1.0f};
                case Components::ButtonState::Pressed:
                    return Color{0.2f, 0.2f, 0.2f, 1.0f};
                case Components::ButtonState::Disabled:
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

    } // namespace UI
} // namespace ECS
