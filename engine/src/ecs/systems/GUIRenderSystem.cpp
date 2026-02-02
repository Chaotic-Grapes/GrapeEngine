/* Start Header *****************************************************************/
/*!
\file   GUIRenderSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of GUI rendering system.
*/
/* End Header *******************************************************************/

#include "ecs/systems/GUIRenderSystem.h"
#include "ecs/systems/RendererSystem.h"
#include "ecs/ui/GUIContext.h"
#include "ecs/ui/GUIStyleRegistry.h"
#include "ecs/ui/GUIUtilities.h"
#include "ecs/ui/GUIHelpers.h"
#include "ecs/StringTable.h"
#include "math/Vector2D.h"
#include "Color.h"
#include "core/Logger.h"
#include <algorithm>
#include <unordered_set>

namespace {
    using ECS::Entity;
    using ECS::World;
    using ECS::Components::GUIButton;
    using ECS::Components::GUIContainer;
    using ECS::Components::GUIElement;
    using ECS::Components::GUIPanel;
    using ECS::Components::GUISeparator;
    using ECS::Components::GUISlider;
    using ECS::Components::GUICheckbox;
    using ECS::Components::GUIDropdown;
    using ECS::Components::GUIInputField;
    using ECS::Components::GUIText;
    using ECS::Components::GUIStyleRef;

    const ECS::UI::GUIStyle* ResolveStyle(World& world, Entity entity) {
        if (!world.Has<GUIStyleRef>(entity)) {
            return nullptr;
        }

        const auto& styleRef = world.Get<GUIStyleRef>(entity);
        if (styleRef.StyleId == 0) {
            return nullptr;
        }

        return ECS::UI::GUIStyleRegistry::GetStyle(styleRef.StyleId);
    }

    Vector2D CenteredPosition(Vector2D topLeft, Vector2D size) {
        return { topLeft.X + size.X * 0.5f, topLeft.Y + size.Y * 0.5f };
    }

    Vector2D ComputeAlignedTextPosition(const GUIElement& element, const GUIText& text,
                                        const std::string& content,
                                        const std::string& fontPath) {
        const Vector2D measured = ECS::UI::GUITextUtils::MeasureText(content, fontPath, text.FontSize);

        const float contentWidth = std::max(0.0f, element.Size.X - element.PaddingLeft - element.PaddingRight);
        const float contentHeight = std::max(0.0f, element.Size.Y - element.PaddingTop - element.PaddingBottom);

        float x = element.WorldPosition.X + element.PaddingLeft;
        switch (text.Alignment) {
            case ECS::Components::GUIText::TextAlignment::Center:
                x += (contentWidth - measured.X) * 0.5f;
                break;
            case ECS::Components::GUIText::TextAlignment::Right:
                x += contentWidth - measured.X;
                break;
            case ECS::Components::GUIText::TextAlignment::Justified:
            case ECS::Components::GUIText::TextAlignment::Left:
            default:
                break;
        }

        float y = element.WorldPosition.Y + element.PaddingTop;
        switch (element.VAlign) {
            case ECS::Components::VerticalAlignment::Middle:
                y += (contentHeight - measured.Y) * 0.5f;
                break;
            case ECS::Components::VerticalAlignment::Bottom:
                y += contentHeight - measured.Y;
                break;
            case ECS::Components::VerticalAlignment::Stretch:
            case ECS::Components::VerticalAlignment::Top:
            default:
                break;
        }

        return { x, y };
    }

    Vector2D ComputeAlignedTextPosition(const GUIElement& element,
                                        ECS::Components::GUIText::TextAlignment alignment,
                                        const Vector2D& measured) {
        const float contentWidth = std::max(0.0f, element.Size.X - element.PaddingLeft - element.PaddingRight);
        const float contentHeight = std::max(0.0f, element.Size.Y - element.PaddingTop - element.PaddingBottom);

        float x = element.WorldPosition.X + element.PaddingLeft;
        switch (alignment) {
            case ECS::Components::GUIText::TextAlignment::Center:
                x += (contentWidth - measured.X) * 0.5f;
                break;
            case ECS::Components::GUIText::TextAlignment::Right:
                x += contentWidth - measured.X;
                break;
            case ECS::Components::GUIText::TextAlignment::Justified:
            case ECS::Components::GUIText::TextAlignment::Left:
            default:
                break;
        }

        float y = element.WorldPosition.Y + element.PaddingTop + (contentHeight - measured.Y) * 0.5f;
        return { x, y };
    }

    std::vector<std::string> SplitOptions(const std::string& optionsText, uint32_t maxOptions) {
        std::vector<std::string> options;
        std::string current;
        for (char c : optionsText) {
            if (c == '\n') {
                options.push_back(current);
                current.clear();
                if (options.size() >= maxOptions) {
                    return options;
                }
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty() && options.size() < maxOptions) {
            options.push_back(current);
        }
        return options;
    }

    bool ResolveClipRect(World& world, Entity entity, Vector2D& outPos, Vector2D& outSize) {
        auto* parent = world.TryGet<ECS::Components::Parent>(entity);
        while (parent && !parent->ParentEntity.IsNull()) {
            Entity parentEntity = parent->ParentEntity;
            if (world.Has<ECS::Components::GUIScrollView>(parentEntity) &&
                world.Has<ECS::Components::GUIElement>(parentEntity)) {
                const auto& scroll = world.Get<ECS::Components::GUIScrollView>(parentEntity);
                if (scroll.ClipContent) {
                    const auto& parentElement = world.Get<ECS::Components::GUIElement>(parentEntity);
                    outPos = {
                        parentElement.WorldPosition.X + parentElement.PaddingLeft,
                        parentElement.WorldPosition.Y + parentElement.PaddingTop
                    };
                    outSize = {
                        std::max(0.0f, parentElement.Size.X - parentElement.PaddingLeft - parentElement.PaddingRight),
                        std::max(0.0f, parentElement.Size.Y - parentElement.PaddingTop - parentElement.PaddingBottom)
                    };
                    return true;
                }
            }

            parent = world.TryGet<ECS::Components::Parent>(parentEntity);
        }

        return false;
    }

    void LogMissingGUIElement(World& world) {
        static std::unordered_set<uint32_t> loggedEntities;

        auto logIfMissing = [&](Entity entity, const char* componentName) {
            if (world.Has<GUIElement>(entity)) {
                return;
            }
            if (!loggedEntities.insert(entity.Index).second) {
                return;
            }
            LOG_WARNING("GUIRenderSystem: entity " << entity.Index
                        << " has " << componentName << " without GUIElement");
        };

        world.Each<GUIPanel>([&](Entity entity, const GUIPanel&) {
            logIfMissing(entity, "GUIPanel");
        });
        world.Each<GUIButton>([&](Entity entity, const GUIButton&) {
            logIfMissing(entity, "GUIButton");
        });
        world.Each<GUIInputField>([&](Entity entity, const GUIInputField&) {
            logIfMissing(entity, "GUIInputField");
        });
        world.Each<GUIText>([&](Entity entity, const GUIText&) {
            logIfMissing(entity, "GUIText");
        });
        world.Each<GUISlider>([&](Entity entity, const GUISlider&) {
            logIfMissing(entity, "GUISlider");
        });
        world.Each<GUICheckbox>([&](Entity entity, const GUICheckbox&) {
            logIfMissing(entity, "GUICheckbox");
        });
        world.Each<GUIDropdown>([&](Entity entity, const GUIDropdown&) {
            logIfMissing(entity, "GUIDropdown");
        });
        world.Each<GUISeparator>([&](Entity entity, const GUISeparator&) {
            logIfMissing(entity, "GUISeparator");
        });
    }

    void RenderPanel(World& world, Entity entity, const GUIElement& element, const GUIPanel& panel,
                     bool clipEnabled, Vector2D clipPos, Vector2D clipSize) {
        auto& ctx = ECS::UI::GUIContext::Get();
        const ECS::UI::GUIStyle* style = ResolveStyle(world, entity);

        Color color = panel.BackgroundColor;
        if (style && style->HasPanelColor) {
            color = style->PanelColor;
        }

        ctx.RenderCommands.SubmitPanel(CenteredPosition(element.WorldPosition, element.Size),
                                       element.Size, color, panel.BorderRadius,
                                       clipEnabled, clipPos, clipSize);
    }

    void RenderButton(World& world, Entity entity, const GUIElement& element, const GUIButton& button,
                      bool clipEnabled, Vector2D clipPos, Vector2D clipSize) {
        auto& ctx = ECS::UI::GUIContext::Get();
        const ECS::UI::GUIStyle* style = ResolveStyle(world, entity);

        Color colorNormal = button.ColorNormal;
        Color colorHovered = button.ColorHovered;
        Color colorPressed = button.ColorPressed;
        Color colorDisabled = button.ColorDisabled;

        if (style && style->HasButtonColors) {
            colorNormal = style->ButtonNormal;
            colorHovered = style->ButtonHovered;
            colorPressed = style->ButtonPressed;
            colorDisabled = style->ButtonDisabled;
        }

        Color bgColor;
        switch (button.State) {
            case ECS::Components::ButtonState::Hovered:
                bgColor = colorHovered;
                break;
            case ECS::Components::ButtonState::Pressed:
                bgColor = colorPressed;
                break;
            case ECS::Components::ButtonState::Disabled:
                bgColor = colorDisabled;
                break;
            case ECS::Components::ButtonState::Normal:
            default:
                bgColor = colorNormal;
                break;
        }

        ctx.RenderCommands.SubmitPanel(CenteredPosition(element.WorldPosition, element.Size),
                                       element.Size, bgColor, 0.0f,
                                       clipEnabled, clipPos, clipSize);

        if (button.Label != 0) {
            Color textColor{1.0f, 1.0f, 1.0f, 1.0f};
            uint32_t fontPath = 0;
            Color shadowColor{0.0f, 0.0f, 0.0f, 0.0f};
            Vector2D shadowOffset{0.0f, 0.0f};
            ECS::Components::GUIText::TextAlignment alignment = ECS::Components::GUIText::TextAlignment::Center;
            float fontSize = 16.0f;

            if (button.UseLabelTextSettings) {
                textColor = button.LabelTextSettings.TextColor;
                fontPath = button.LabelTextSettings.FontPath;
                shadowColor = button.LabelTextSettings.ShadowColor;
                shadowOffset = button.LabelTextSettings.ShadowOffset;
                alignment = button.LabelTextSettings.Alignment;
                fontSize = button.LabelTextSettings.FontSize;
            }

            if (style) {
                if (style->HasTextColor) {
                    textColor = style->TextColor;
                }
                if (style->HasFontPath) {
                    if (fontPath == 0) {
                        fontPath = style->FontPath;
                    }
                }
                if (style->HasTextShadow) {
                    shadowColor = style->TextShadowColor;
                    shadowOffset = style->TextShadowOffset;
                }
                if (style->HasTextFontSize && !button.UseLabelTextSettings) {
                    fontSize = style->TextFontSize;
                }
            }

            const std::string& content = ctx.StringCache.Resolve(button.Label);
            const std::string& fontPathStr = ctx.StringCache.Resolve(fontPath);
            const Vector2D measured = ECS::UI::GUITextUtils::MeasureText(content, fontPathStr, fontSize);
            Vector2D textPos = ComputeAlignedTextPosition(element, alignment, measured);

            ctx.RenderCommands.SubmitText(fontPath, button.Label, textPos, textColor, fontSize,
                                          shadowColor, shadowOffset,
                                          clipEnabled, clipPos, clipSize);
        }
    }

    void RenderText(World& world, Entity entity, const GUIElement& element, const GUIText& text,
                    bool clipEnabled, Vector2D clipPos, Vector2D clipSize) {
        auto& ctx = ECS::UI::GUIContext::Get();
        const ECS::UI::GUIStyle* style = ResolveStyle(world, entity);

        Color fontColor = text.FontColor;
        uint32_t fontPath = text.FontPath;
        float fontSize = text.FontSize;
        Color shadowColor = text.ShadowColor;
        Vector2D shadowOffset = text.ShadowOffset;

        if (style) {
            if (style->HasTextColor) {
                fontColor = style->TextColor;
            }
            if (style->HasFontPath) {
                fontPath = style->FontPath;
            }
            if (style->HasTextFontSize) {
                fontSize = style->TextFontSize;
            }
            if (style->HasTextShadow) {
                shadowColor = style->TextShadowColor;
                shadowOffset = style->TextShadowOffset;
            }
        }

        const std::string& content = ctx.StringCache.Resolve(text.Content);
        const std::string& fontPathStr = ctx.StringCache.Resolve(fontPath);
        GUIText measureText = text;
        measureText.FontSize = fontSize;
        const Vector2D textPos = ComputeAlignedTextPosition(element, measureText, content, fontPathStr);

        ctx.RenderCommands.SubmitText(fontPath, text.Content, textPos,
                                      fontColor, fontSize,
                                      shadowColor, shadowOffset,
                                      clipEnabled, clipPos, clipSize);
    }

    void RenderSlider(World& world, Entity entity, const GUIElement& element, const GUISlider& slider,
                      bool clipEnabled, Vector2D clipPos, Vector2D clipSize) {
        auto& ctx = ECS::UI::GUIContext::Get();
        const ECS::UI::GUIStyle* style = ResolveStyle(world, entity);

        Color background = slider.BackgroundColor;
        Color handle = slider.HandleColor;

        if (style && style->HasSliderColors) {
            background = style->SliderBackground;
            handle = style->SliderHandle;
        }

        float normalizedValue = (slider.CurrentValue - slider.MinValue) /
                                (slider.MaxValue - slider.MinValue);
        normalizedValue = std::max(0.0f, std::min(1.0f, normalizedValue));

        ctx.RenderCommands.SubmitSlider(CenteredPosition(element.WorldPosition, element.Size),
                                        element.Size,
                                        normalizedValue, background, handle,
                                        clipEnabled, clipPos, clipSize);
    }

    void RenderCheckbox(World& world, Entity entity, const GUIElement& element, const GUICheckbox& checkbox,
                        bool clipEnabled, Vector2D clipPos, Vector2D clipSize) {
        auto& ctx = ECS::UI::GUIContext::Get();
        const ECS::UI::GUIStyle* style = ResolveStyle(world, entity);

        Color checked = checkbox.CheckedColor;
        Color unchecked = checkbox.UncheckedColor;
        Color border = checkbox.BorderColor;
        Color textColor{1.0f, 1.0f, 1.0f, 1.0f};
        uint32_t fontPath = 0;
        Color shadowColor{0.0f, 0.0f, 0.0f, 0.0f};
        Vector2D shadowOffset{0.0f, 0.0f};
        ECS::Components::GUIText::TextAlignment labelAlignment = ECS::Components::GUIText::TextAlignment::Left;
        float labelFontSize = checkbox.LabelFontSize;

        if (style && style->HasCheckboxColors) {
            checked = style->CheckboxChecked;
            unchecked = style->CheckboxUnchecked;
            border = style->CheckboxBorder;
        }
        if (checkbox.UseLabelTextSettings) {
            textColor = checkbox.LabelTextSettings.TextColor;
            fontPath = checkbox.LabelTextSettings.FontPath;
            shadowColor = checkbox.LabelTextSettings.ShadowColor;
            shadowOffset = checkbox.LabelTextSettings.ShadowOffset;
            labelAlignment = checkbox.LabelTextSettings.Alignment;
            labelFontSize = checkbox.LabelTextSettings.FontSize;
        } else {
            switch (checkbox.LabelAlignment) {
                case ECS::Components::HorizontalAlignment::Center:
                    labelAlignment = ECS::Components::GUIText::TextAlignment::Center;
                    break;
                case ECS::Components::HorizontalAlignment::Right:
                    labelAlignment = ECS::Components::GUIText::TextAlignment::Right;
                    break;
                case ECS::Components::HorizontalAlignment::Stretch:
                case ECS::Components::HorizontalAlignment::Left:
                default:
                    break;
            }
        }

        if (style) {
            if (style->HasTextColor) {
                textColor = style->TextColor;
            }
            if (style->HasFontPath) {
                fontPath = style->FontPath;
            }
            if (style->HasTextShadow) {
                shadowColor = style->TextShadowColor;
                shadowOffset = style->TextShadowOffset;
            }
            if (style->HasTextFontSize && !checkbox.UseLabelTextSettings) {
                labelFontSize = style->TextFontSize;
            }
        }

        if (!checkbox.Interactable) {
            checked.A *= 0.6f;
            unchecked.A *= 0.6f;
            textColor.A *= 0.6f;
        }

        Color boxColor = checkbox.IsChecked ? checked : unchecked;
        const float contentWidth = std::max(0.0f, element.Size.X - element.PaddingLeft - element.PaddingRight);
        const float contentHeight = std::max(0.0f, element.Size.Y - element.PaddingTop - element.PaddingBottom);
        float checkSize = checkbox.CheckSize > 0.0f ? checkbox.CheckSize : contentHeight;
        if (contentHeight > 0.0f) {
            checkSize = std::min(checkSize, contentHeight);
        }

        const Vector2D boxCenter{
            element.WorldPosition.X + element.PaddingLeft + checkSize * 0.5f,
            element.WorldPosition.Y + element.PaddingTop + contentHeight * 0.5f
        };

        ctx.RenderCommands.SubmitCheckbox(boxCenter,
                                          { checkSize, checkSize },
                                          checkbox.IsChecked, boxColor, checked, border,
                                          clipEnabled, clipPos, clipSize);

        if (checkbox.Label != 0) {
            constexpr float kLabelSpacing = 6.0f;
            const std::string& content = ctx.StringCache.Resolve(checkbox.Label);
            const std::string& fontPathStr = ctx.StringCache.Resolve(fontPath);
            const Vector2D measured = ECS::UI::GUITextUtils::MeasureText(content, fontPathStr, labelFontSize);

            const float labelStartX = element.WorldPosition.X + element.PaddingLeft + checkSize + kLabelSpacing;
            float labelWidth = element.WorldPosition.X + element.PaddingLeft + contentWidth - labelStartX;
            if (labelWidth < 0.0f) {
                labelWidth = 0.0f;
            }

            float textX = labelStartX;
            switch (labelAlignment) {
                case ECS::Components::GUIText::TextAlignment::Center:
                    textX = labelStartX + (labelWidth - measured.X) * 0.5f;
                    break;
                case ECS::Components::GUIText::TextAlignment::Right:
                    textX = labelStartX + (labelWidth - measured.X);
                    break;
                case ECS::Components::GUIText::TextAlignment::Justified:
                case ECS::Components::GUIText::TextAlignment::Left:
                default:
                    break;
            }

            Vector2D textPos{
                textX,
                element.WorldPosition.Y + element.PaddingTop + (contentHeight - measured.Y) * 0.5f
            };

            ctx.RenderCommands.SubmitText(fontPath, checkbox.Label, textPos,
                                          textColor, labelFontSize,
                                          shadowColor, shadowOffset,
                                          clipEnabled, clipPos, clipSize);
        }
    }

    void RenderDropdown(World& world, Entity entity, const GUIElement& element, const GUIDropdown& dropdown,
                        bool clipEnabled, Vector2D clipPos, Vector2D clipSize) {
        auto& ctx = ECS::UI::GUIContext::Get();
        const ECS::UI::GUIStyle* style = ResolveStyle(world, entity);

        Color background = dropdown.BackgroundColor;
        Color highlight = dropdown.HighlightColor;
        Color textColor{1.0f, 1.0f, 1.0f, 1.0f};
        uint32_t fontPath = 0;
        float fontSize = 16.0f;
        Color shadowColor{0.0f, 0.0f, 0.0f, 0.0f};
        Vector2D shadowOffset{0.0f, 0.0f};
        ECS::Components::GUIText::TextAlignment alignment = ECS::Components::GUIText::TextAlignment::Left;

        if (style && style->HasDropdownColors) {
            background = style->DropdownBackground;
            highlight = style->DropdownHighlight;
        }
        if (dropdown.UseOptionTextSettings) {
            textColor = dropdown.OptionTextSettings.TextColor;
            fontPath = dropdown.OptionTextSettings.FontPath;
            fontSize = dropdown.OptionTextSettings.FontSize;
            shadowColor = dropdown.OptionTextSettings.ShadowColor;
            shadowOffset = dropdown.OptionTextSettings.ShadowOffset;
            alignment = dropdown.OptionTextSettings.Alignment;
        }
        if (style) {
            if (style->HasTextColor) {
                textColor = style->TextColor;
            }
            if (style->HasFontPath && fontPath == 0) {
                fontPath = style->FontPath;
            }
            if (style->HasTextFontSize && !dropdown.UseOptionTextSettings) {
                fontSize = style->TextFontSize;
            }
            if (style->HasTextShadow) {
                shadowColor = style->TextShadowColor;
                shadowOffset = style->TextShadowOffset;
            }
        }

        ctx.RenderCommands.SubmitPanel(CenteredPosition(element.WorldPosition, element.Size),
                                       element.Size, background, 0.0f,
                                       clipEnabled, clipPos, clipSize);

            const std::string& optionTextRaw = ctx.StringCache.Resolve(dropdown.Options);
            const auto options = SplitOptions(optionTextRaw, dropdown.OptionCount);
            if (!options.empty() && dropdown.SelectedIndex < options.size()) {
                const std::string& selected = options[dropdown.SelectedIndex];
                const std::string& fontPathStr = ctx.StringCache.Resolve(fontPath);
                const Vector2D measured = ECS::UI::GUITextUtils::MeasureText(selected, fontPathStr, fontSize);
                Vector2D textPos = ComputeAlignedTextPosition(element, alignment, measured);
                ctx.RenderCommands.SubmitText(fontPath, ECS::StringTable::Intern(selected), textPos,
                                              textColor, fontSize, shadowColor, shadowOffset,
                                              clipEnabled, clipPos, clipSize);
            }

        if (dropdown.IsOpen && dropdown.OptionCount > 0) {
            for (uint32_t i = 0; i < dropdown.OptionCount && i < ECS::Components::GUIDropdown::MaxOptions; ++i) {
                Vector2D optionPos = element.WorldPosition;
                optionPos.Y += element.Size.Y * (i + 1);

                Color optionColor = (i == dropdown.SelectedIndex) ? highlight : background;
                ctx.RenderCommands.SubmitPanel(CenteredPosition(optionPos, element.Size),
                                               element.Size, optionColor, 0.0f,
                                               clipEnabled, clipPos, clipSize);

                if (i < options.size()) {
                    GUIElement optionElement = element;
                    optionElement.WorldPosition = optionPos;
                    const std::string& optionText = options[i];
                    const std::string& fontPathStr = ctx.StringCache.Resolve(fontPath);
                    const Vector2D measured = ECS::UI::GUITextUtils::MeasureText(optionText, fontPathStr, fontSize);
                    Vector2D textPos = ComputeAlignedTextPosition(optionElement, alignment, measured);
                    ctx.RenderCommands.SubmitText(fontPath, ECS::StringTable::Intern(optionText), textPos,
                                                  textColor, fontSize, shadowColor, shadowOffset,
                                                  clipEnabled, clipPos, clipSize);
                }
            }
        }
    }

    void RenderInputField(World& world, Entity entity, const GUIElement& element, const GUIInputField& input,
                          bool clipEnabled, Vector2D clipPos, Vector2D clipSize) {
        auto& ctx = ECS::UI::GUIContext::Get();
        const ECS::UI::GUIStyle* style = ResolveStyle(world, entity);
        Color background = input.BackgroundColor;
        Color textColor = input.TextColor;
        uint32_t fontPath = input.FontPath;
        float fontSize = input.FontSize;
        Color shadowColor{0.0f, 0.0f, 0.0f, 0.0f};
        Vector2D shadowOffset{0.0f, 0.0f};
        ECS::Components::GUIText::TextAlignment alignment = ECS::Components::GUIText::TextAlignment::Left;

        if (input.UseTextSettings) {
            textColor = input.TextSettings.TextColor;
            fontPath = input.TextSettings.FontPath;
            fontSize = input.TextSettings.FontSize;
            shadowColor = input.TextSettings.ShadowColor;
            shadowOffset = input.TextSettings.ShadowOffset;
            alignment = input.TextSettings.Alignment;
        }

        if (style) {
            if (style->HasInputColors) {
                background = style->InputBackground;
                textColor = style->InputText;
            }
            if (style->HasTextColor) {
                textColor = style->TextColor;
            }
            if (style->HasFontPath && fontPath == 0) {
                fontPath = style->FontPath;
            }
            if (style->HasTextFontSize && !input.UseTextSettings) {
                fontSize = style->TextFontSize;
            }
            if (style->HasTextShadow) {
                shadowColor = style->TextShadowColor;
                shadowOffset = style->TextShadowOffset;
            }
        }

        ctx.RenderCommands.SubmitPanel(CenteredPosition(element.WorldPosition, element.Size),
                                       element.Size, background, 0.0f,
                                       clipEnabled, clipPos, clipSize);

        uint32_t textId = input.Content != 0 ? input.Content : input.Placeholder;
        if (textId != 0) {
            if (input.Content == 0) {
                if (input.UsePlaceholderSettings) {
                    textColor = input.PlaceholderSettings.TextColor;
                    fontPath = input.PlaceholderSettings.FontPath;
                    fontSize = input.PlaceholderSettings.FontSize;
                    shadowColor = input.PlaceholderSettings.ShadowColor;
                    shadowOffset = input.PlaceholderSettings.ShadowOffset;
                    alignment = input.PlaceholderSettings.Alignment;
                } else {
                    textColor.A *= 0.6f;
                    if (style && style->HasInputColors) {
                        textColor = style->InputPlaceholder;
                    }
                }
            }

            const std::string& content = ctx.StringCache.Resolve(textId);
            const std::string& fontPathStr = ctx.StringCache.Resolve(fontPath);
            const Vector2D measured = ECS::UI::GUITextUtils::MeasureText(content, fontPathStr, fontSize);
            Vector2D textPos = ComputeAlignedTextPosition(element, alignment, measured);
            ctx.RenderCommands.SubmitText(fontPath, textId, textPos,
                                          textColor, fontSize,
                                          shadowColor, shadowOffset,
                                          clipEnabled, clipPos, clipSize);
        }
    }

    void RenderSeparator(World& world, Entity entity, const GUIElement& element, const GUISeparator& separator,
                         bool clipEnabled, Vector2D clipPos, Vector2D clipSize) {
        (void)world;
        (void)entity;
        auto& ctx = ECS::UI::GUIContext::Get();

        Vector2D startPos, endPos;
        if (separator.Orient == ECS::Components::GUISeparator::Orientation::Horizontal) {
            float centerY = element.WorldPosition.Y + element.Size.Y * 0.5f;
            startPos = { element.WorldPosition.X, centerY };
            endPos = { element.WorldPosition.X + element.Size.X, centerY };
        } else {
            float centerX = element.WorldPosition.X + element.Size.X * 0.5f;
            startPos = { centerX, element.WorldPosition.Y };
            endPos = { centerX, element.WorldPosition.Y + element.Size.Y };
        }

        ctx.RenderCommands.SubmitLine(startPos, endPos, separator.Color, separator.Thickness,
                                      clipEnabled, clipPos, clipSize);
    }

    void RenderElement(World& world, Entity entity) {
        if (!world.Has<GUIElement>(entity)) {
            return;
        }

        const auto& element = world.Get<GUIElement>(entity);
        if (!element.Active || !element.Visible) {
            return;
        }

        auto& ctx = ECS::UI::GUIContext::Get();
        auto visualElement = element;
        const auto visualIt = ctx.VisualStates.find(entity.Index);
        if (visualIt != ctx.VisualStates.end()) {
            const float scale = std::max(0.01f, visualIt->second.CurrentScale);
            const Vector2D scaledSize{ element.Size.X * scale, element.Size.Y * scale };
            const Vector2D offset{
                (element.Size.X - scaledSize.X) * 0.5f,
                (element.Size.Y - scaledSize.Y) * 0.5f
            };
            visualElement.Size = scaledSize;
            visualElement.WorldPosition = { element.WorldPosition.X + offset.X, element.WorldPosition.Y + offset.Y };
        }

        Vector2D clipPos{};
        Vector2D clipSize{};
        const bool clipEnabled = ResolveClipRect(world, entity, clipPos, clipSize);

        if (world.Has<GUIPanel>(entity)) {
            RenderPanel(world, entity, visualElement, world.Get<GUIPanel>(entity),
                        clipEnabled, clipPos, clipSize);
        }
        if (world.Has<GUIButton>(entity)) {
            RenderButton(world, entity, visualElement, world.Get<GUIButton>(entity),
                         clipEnabled, clipPos, clipSize);
        }
        if (world.Has<GUIInputField>(entity)) {
            RenderInputField(world, entity, visualElement, world.Get<GUIInputField>(entity),
                             clipEnabled, clipPos, clipSize);
        }
        if (world.Has<GUIText>(entity)) {
            RenderText(world, entity, visualElement, world.Get<GUIText>(entity),
                       clipEnabled, clipPos, clipSize);
        }
        if (world.Has<GUISlider>(entity)) {
            RenderSlider(world, entity, visualElement, world.Get<GUISlider>(entity),
                         clipEnabled, clipPos, clipSize);
        }
        if (world.Has<GUICheckbox>(entity)) {
            RenderCheckbox(world, entity, visualElement, world.Get<GUICheckbox>(entity),
                           clipEnabled, clipPos, clipSize);
        }
        if (world.Has<GUIDropdown>(entity)) {
            RenderDropdown(world, entity, visualElement, world.Get<GUIDropdown>(entity),
                           clipEnabled, clipPos, clipSize);
        }
        if (world.Has<GUISeparator>(entity)) {
            RenderSeparator(world, entity, visualElement, world.Get<GUISeparator>(entity),
                            clipEnabled, clipPos, clipSize);
        }

        if (visualIt != ctx.VisualStates.end() && visualIt->second.HasOverlay) {
            const auto& overlay = visualIt->second.OverlayColor;
            ctx.RenderCommands.SubmitPanel(
                CenteredPosition(visualElement.WorldPosition, visualElement.Size),
                visualElement.Size,
                overlay,
                0.0f,
                clipEnabled, clipPos, clipSize);
        }
    }

    void FlushRenderCommands(ECS::RendererSystem* rendererSystem) {
        if (!rendererSystem) {
            return;
        }

        auto& ctx = ECS::UI::GUIContext::Get();
        static const std::string kDefaultFont = "assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf";
        const float scale = (ctx.CanvasScale > 0.0f) ? ctx.CanvasScale : 1.0f;
        const Vector2D offset = ctx.CanvasOffset;

        for (const auto& cmd : ctx.RenderCommands.Commands()) {
            const bool clipEnabled = cmd.ClipEnabled;
            const Vector2D clipPos{
                cmd.ClipPosition.X * scale + offset.X,
                cmd.ClipPosition.Y * scale + offset.Y
            };
            const Vector2D clipSize{
                cmd.ClipSize.X * scale,
                cmd.ClipSize.Y * scale
            };

            switch (cmd.Type) {
                case ECS::UI::GUIRenderCommandType::Panel:
                    rendererSystem->SubmitGUIPanel(
                        { cmd.Position.X * scale + offset.X, cmd.Position.Y * scale + offset.Y },
                        { cmd.Size.X * scale, cmd.Size.Y * scale },
                        cmd.PrimaryColor,
                        cmd.ScalarA * scale,
                        clipEnabled, clipPos, clipSize);
                    break;
                case ECS::UI::GUIRenderCommandType::Text: {
                    const std::string& resolvedFont = ctx.StringCache.Resolve(cmd.FontId);
                    const std::string& fontPath = resolvedFont.empty() ? kDefaultFont : resolvedFont;
                    const std::string& content = ctx.StringCache.Resolve(cmd.TextId);
                    rendererSystem->SubmitGUIText(
                        fontPath,
                        content,
                        { cmd.Position.X * scale + offset.X, cmd.Position.Y * scale + offset.Y },
                        cmd.PrimaryColor,
                        cmd.ScalarA * scale,
                        cmd.SecondaryColor,
                        { cmd.Offset.X * scale, cmd.Offset.Y * scale },
                        clipEnabled, clipPos, clipSize);
                    break;
                }
                case ECS::UI::GUIRenderCommandType::Slider:
                    rendererSystem->SubmitGUISlider(
                        { cmd.Position.X * scale + offset.X, cmd.Position.Y * scale + offset.Y },
                        { cmd.Size.X * scale, cmd.Size.Y * scale },
                        cmd.ScalarA,
                        cmd.PrimaryColor,
                        cmd.SecondaryColor,
                        Color(200U, 200U, 200U, 255U),
                        4.0f,
                        clipEnabled, clipPos, clipSize);
                    break;
                case ECS::UI::GUIRenderCommandType::Checkbox:
                    rendererSystem->SubmitGUICheckbox(
                        { cmd.Position.X * scale + offset.X, cmd.Position.Y * scale + offset.Y },
                        { cmd.Size.X * scale, cmd.Size.Y * scale },
                        cmd.Flag,
                        cmd.PrimaryColor,
                        cmd.SecondaryColor,
                        cmd.TertiaryColor,
                        clipEnabled, clipPos, clipSize);
                    break;
                case ECS::UI::GUIRenderCommandType::Line:
                    rendererSystem->SubmitGUILine(
                        { cmd.Position.X * scale + offset.X, cmd.Position.Y * scale + offset.Y },
                        { cmd.End.X * scale + offset.X, cmd.End.Y * scale + offset.Y },
                        cmd.PrimaryColor,
                        cmd.ScalarA * scale,
                        clipEnabled, clipPos, clipSize);
                    break;
            }
        }
    }

    void SubmitRectOutline(ECS::UI::GUIRenderCommandBuffer& buffer, Vector2D pos, Vector2D size,
                           const Color& color, float thickness) {
        Vector2D topLeft = pos;
        Vector2D topRight{ pos.X + size.X, pos.Y };
        Vector2D bottomLeft{ pos.X, pos.Y + size.Y };
        Vector2D bottomRight{ pos.X + size.X, pos.Y + size.Y };

        buffer.SubmitLine(topLeft, topRight, color, thickness);
        buffer.SubmitLine(topRight, bottomRight, color, thickness);
        buffer.SubmitLine(bottomRight, bottomLeft, color, thickness);
        buffer.SubmitLine(bottomLeft, topLeft, color, thickness);
    }

    void RenderDebugOverlay(World& world, const ECS::Components::GUICanvas& canvasSettings) {
        auto& ctx = ECS::UI::GUIContext::Get();
        const Color boundsColor = canvasSettings.DebugBoundsColor;
        const Color paddingColor = canvasSettings.DebugPaddingColor;
        const Color anchorColor = canvasSettings.DebugAnchorColor;
        constexpr float kLineThickness = 1.0f;
        constexpr float kAnchorSize = 6.0f;

        world.Each<GUIElement>([&](Entity entity, const GUIElement& element) {
            if (!element.Active || !element.Visible) {
                return;
            }

            SubmitRectOutline(ctx.RenderCommands, element.WorldPosition, element.Size,
                              boundsColor, kLineThickness);

            Vector2D innerPos{
                element.WorldPosition.X + element.PaddingLeft,
                element.WorldPosition.Y + element.PaddingTop
            };
            Vector2D innerSize{
                std::max(0.0f, element.Size.X - element.PaddingLeft - element.PaddingRight),
                std::max(0.0f, element.Size.Y - element.PaddingTop - element.PaddingBottom)
            };
            if (innerSize.X > 0.0f && innerSize.Y > 0.0f) {
                SubmitRectOutline(ctx.RenderCommands, innerPos, innerSize,
                                  paddingColor, kLineThickness);
            }

            if (const auto* parent = world.TryGet<ECS::Components::Parent>(entity)) {
                if (!parent->ParentEntity.IsNull() && world.Has<GUIElement>(parent->ParentEntity)) {
                    const auto& parentElement = world.Get<GUIElement>(parent->ParentEntity);
                    Vector2D anchorPos{
                        parentElement.WorldPosition.X + element.AnchorMin.X * parentElement.Size.X,
                        parentElement.WorldPosition.Y + element.AnchorMin.Y * parentElement.Size.Y
                    };
                    Vector2D left{ anchorPos.X - kAnchorSize, anchorPos.Y };
                    Vector2D right{ anchorPos.X + kAnchorSize, anchorPos.Y };
                    Vector2D top{ anchorPos.X, anchorPos.Y - kAnchorSize };
                    Vector2D bottom{ anchorPos.X, anchorPos.Y + kAnchorSize };
                    ctx.RenderCommands.SubmitLine(left, right, anchorColor, kLineThickness);
                    ctx.RenderCommands.SubmitLine(top, bottom, anchorColor, kLineThickness);
                }
            }
        });
    }
}

namespace ECS {

    void GUIRenderSystem::OnCreate(World& world) {
        (void)world;
        LOG_INFO("GUIRenderSystem initialized");
    }

    void GUIRenderSystem::OnUpdate(World& world) {
        RendererSystem* rendererSystem = RendererSystem::GetInstance();
        if (!rendererSystem) {
            LOG_WARNING("GUIRenderSystem::OnUpdate: RendererSystem not available");
            return;
        }

        LogMissingGUIElement(world);

        auto elements = UI::GetSortedGUIElements(world);
        for (Entity entity : elements) {
            if (world.IsAlive(entity)) {
                RenderElement(world, entity);
            }
        }

        auto& ctx = UI::GUIContext::Get();
        for (Entity modal : ctx.Modals) {
            if (world.IsAlive(modal)) {
                RenderElement(world, modal);
            }
        }

        if (ctx.Tooltip.Visible) {
            // TODO: render tooltip via renderer
        }

        bool debugDraw = false;
        ECS::Components::GUICanvas canvasSettings{};
        world.Each<ECS::Components::GUICanvas>([&](Entity, const ECS::Components::GUICanvas& canvas) {
            if (!debugDraw && canvas.DebugDraw) {
                canvasSettings = canvas;
                debugDraw = true;
            }
        });

        if (debugDraw) {
            RenderDebugOverlay(world, canvasSettings);
        }

        FlushRenderCommands(rendererSystem);
    }

    void GUIRenderSystem::OnDestroy(World& world) {
        (void)world;
        LOG_INFO("GUIRenderSystem destroyed");
    }

    SystemMetadata GUIRenderSystem::GetMetadata() const {
        return ComponentAccessBuilder("GUIRenderSystem")
            .ReadComponent<Components::GUIElement>()
            .SetExecutionOrder(2)
            .SetGroup(SystemGroup::PreRender)
            .SetRunMode(SystemRunMode::Always)
            .SetEnabled(true)
            .Build();
    }

} // namespace ECS
