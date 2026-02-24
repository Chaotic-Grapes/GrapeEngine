/* Start Header *****************************************************************/
/*!
\file   EditorStyle.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   22nd November 2025

\brief
Declares shared editor color styles for consistent UI theming.
These colors are used across multiple editor panels for actions like dangerous
operations (deletions, exits) to maintain a uniform look and feel.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_STYLE_H
#define EDITOR_STYLE_H

#include <imgui.h>
#include <string>

namespace EditorStyle {
    extern float FontScale;                               // Global font scaling factor (defined in .cpp)
    static constexpr size_t MAX_OBJECT_NAME_LENGTH = 128;  // Max length for game object names

    // Transparent (useful for passthru backgrounds)
    static const ImVec4 Transparent         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Primary / Accent
    static const ImVec4 Accent              = ImVec4(0.32f, 0.66f, 0.94f, 1.0f);
    static const ImVec4 AccentHover         = ImVec4(0.40f, 0.74f, 1.0f, 1.0f);
    static const ImVec4 AccentActive        = ImVec4(0.26f, 0.58f, 0.86f, 1.0f);
    // Button token aliases for semantic styling.
    static const ImVec4 PrimaryButton       = Accent;
    static const ImVec4 PrimaryButtonHover  = AccentHover;
    static const ImVec4 PrimaryButtonActive = AccentActive;

    // Secondary / neutral
    // Lower-emphasis button palette.
    static const ImVec4 SecondaryButton       = ImVec4(0.20f, 0.24f, 0.30f, 1.0f);
    static const ImVec4 SecondaryButtonHover  = ImVec4(0.26f, 0.30f, 0.36f, 1.0f);
    static const ImVec4 SecondaryButtonActive = ImVec4(0.18f, 0.22f, 0.28f, 1.0f);

    // Danger / destructive action colors
    static const ImVec4 DangerText          = ImVec4(0.90f, 0.26f, 0.22f, 1.0f);
    static const ImVec4 DangerButton        = ImVec4(0.80f, 0.20f, 0.18f, 1.0f);
    static const ImVec4 DangerButtonHover   = ImVec4(0.92f, 0.32f, 0.28f, 1.0f);
    static const ImVec4 DangerButtonActive  = ImVec4(0.70f, 0.16f, 0.14f, 1.0f);

    // Positive / success
    static const ImVec4 SuccessText         = ImVec4(0.28f, 0.84f, 0.54f, 1.0f);
    static const ImVec4 SuccessButton       = ImVec4(0.20f, 0.72f, 0.46f, 1.0f);
    static const ImVec4 SuccessButtonHover  = ImVec4(0.32f, 0.88f, 0.60f, 1.0f);
    static const ImVec4 SuccessButtonActive = ImVec4(0.18f, 0.62f, 0.40f, 1.0f);

    // Warning / caution
    static const ImVec4 WarningText         = ImVec4(0.96f, 0.74f, 0.20f, 1.0f);
    static const ImVec4 WarningButton       = ImVec4(0.92f, 0.64f, 0.14f, 1.0f);

    // Log level colors
    static const ImVec4 LogInfo             = ImVec4(0.70f, 0.74f, 0.78f, 1.0f);
    static const ImVec4 LogDebug            = ImVec4(0.30f, 0.78f, 0.98f, 1.0f);
    static const ImVec4 LogWarning          = WarningText;
    static const ImVec4 LogCritical         = ImVec4(1.0f, 0.44f, 0.26f, 1.0f);

    // Neutral UI colors
    static const ImVec4 Text                = ImVec4(0.92f, 0.94f, 0.97f, 1.0f);
    static const ImVec4 TextDisabled        = ImVec4(0.48f, 0.52f, 0.58f, 1.0f);
    static const ImVec4 WindowBg            = ImVec4(0.06f, 0.07f, 0.09f, 1.0f);
    static const ImVec4 FrameBg             = ImVec4(0.12f, 0.13f, 0.16f, 1.0f);
    static const ImVec4 FrameBgHover        = ImVec4(0.16f, 0.18f, 0.22f, 1.0f);
    static const ImVec4 FrameBgActive       = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
    static const ImVec4 Border              = ImVec4(0.24f, 0.27f, 0.32f, 1.0f);
    static const ImVec4 Separator           = ImVec4(0.18f, 0.20f, 0.24f, 1.0f);

    // Tabs
    static const ImVec4 Tab                 = ImVec4(0.10f, 0.12f, 0.15f, 1.0f);
    static const ImVec4 TabHover            = ImVec4(0.17f, 0.20f, 0.25f, 1.0f);
    static const ImVec4 TabActive           = ImVec4(0.13f, 0.15f, 0.19f, 1.0f);

    // Selection / highlights
    static const ImVec4 Selection           = ImVec4(0.18f, 0.78f, 0.66f, 0.30f);
    static const ImVec4 SelectionBorder     = ImVec4(0.18f, 0.78f, 0.66f, 1.0f);

    // Misc
    static const ImVec4 PopupBg             = ImVec4(0.08f, 0.09f, 0.12f, 0.98f);
    static const ImVec4 TooltipBg           = ImVec4(0.07f, 0.08f, 0.11f, 0.98f);
    static const ImVec4 Muted               = ImVec4(0.62f, 0.66f, 0.72f, 1.0f);

    // Scale a color by a scalar
    inline ImVec4 Scale(const ImVec4& c, float s) {
        return ImVec4(
            c.x * s,
            c.y * s,
            c.z * s,
            c.w * s
        );
    }


    // Small helper: apply some of these to ImGui style (optional helper function)
    inline void ApplyToImGuiStyle(ImGuiStyle& style) {
        style.Colors[ImGuiCol_Text] = Text;
        style.Colors[ImGuiCol_TextDisabled] = TextDisabled;
        style.Colors[ImGuiCol_WindowBg] = WindowBg;
        style.Colors[ImGuiCol_ChildBg] = WindowBg;
        style.Colors[ImGuiCol_PopupBg] = PopupBg;
        style.Colors[ImGuiCol_Border] = Border;
        style.Colors[ImGuiCol_Separator] = Separator;
        style.Colors[ImGuiCol_FrameBg] = FrameBg;
        style.Colors[ImGuiCol_FrameBgHovered] = FrameBgHover;
        style.Colors[ImGuiCol_FrameBgActive] = FrameBgActive;
        style.Colors[ImGuiCol_Tab] = Tab;
        style.Colors[ImGuiCol_TabHovered] = TabHover;
        style.Colors[ImGuiCol_TabActive] = TabActive;
        style.Colors[ImGuiCol_Button] = Accent;
        style.Colors[ImGuiCol_ButtonHovered] = AccentHover;
        style.Colors[ImGuiCol_ButtonActive] = AccentActive;
        style.Colors[ImGuiCol_Header] = FrameBg;
        style.Colors[ImGuiCol_HeaderHovered] = FrameBgHover;
        style.Colors[ImGuiCol_HeaderActive] = FrameBgActive;
    }

    // Full theme pass for a modern editor look (spacing, rounding, colors).
    inline void ApplyModernTheme(ImGuiStyle& style) {
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.CellPadding = ImVec2(8.0f, 6.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 14.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 10.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 0.0f;
        style.TabBarBorderSize = 0.0f;
        style.WindowRounding = 8.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 6.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 6.0f;
        style.TabRounding = 6.0f;

        style.Colors[ImGuiCol_Text] = Text;
        style.Colors[ImGuiCol_TextDisabled] = TextDisabled;
        style.Colors[ImGuiCol_WindowBg] = WindowBg;
        style.Colors[ImGuiCol_ChildBg] = WindowBg;
        style.Colors[ImGuiCol_PopupBg] = PopupBg;
        style.Colors[ImGuiCol_Border] = Border;
        style.Colors[ImGuiCol_BorderShadow] = Transparent;
        style.Colors[ImGuiCol_FrameBg] = FrameBg;
        style.Colors[ImGuiCol_FrameBgHovered] = FrameBgHover;
        style.Colors[ImGuiCol_FrameBgActive] = FrameBgActive;
        style.Colors[ImGuiCol_TitleBg] = Scale(WindowBg, 0.9f);
        style.Colors[ImGuiCol_TitleBgActive] = Scale(WindowBg, 1.1f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = Scale(WindowBg, 0.8f);
        style.Colors[ImGuiCol_MenuBarBg] = Scale(WindowBg, 1.05f);
        style.Colors[ImGuiCol_ScrollbarBg] = Scale(WindowBg, 0.9f);
        style.Colors[ImGuiCol_ScrollbarGrab] = Scale(FrameBg, 1.2f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = Scale(FrameBgHover, 1.2f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = Scale(FrameBgActive, 1.2f);
        style.Colors[ImGuiCol_CheckMark] = Accent;
        style.Colors[ImGuiCol_SliderGrab] = Accent;
        style.Colors[ImGuiCol_SliderGrabActive] = AccentActive;
        style.Colors[ImGuiCol_Button] = Accent;
        style.Colors[ImGuiCol_ButtonHovered] = AccentHover;
        style.Colors[ImGuiCol_ButtonActive] = AccentActive;
        style.Colors[ImGuiCol_Header] = FrameBg;
        style.Colors[ImGuiCol_HeaderHovered] = FrameBgHover;
        style.Colors[ImGuiCol_HeaderActive] = FrameBgActive;
        style.Colors[ImGuiCol_Separator] = Separator;
        style.Colors[ImGuiCol_SeparatorHovered] = Scale(Separator, 1.2f);
        style.Colors[ImGuiCol_SeparatorActive] = Scale(Separator, 1.4f);
        style.Colors[ImGuiCol_ResizeGrip] = Scale(Accent, 0.35f);
        style.Colors[ImGuiCol_ResizeGripHovered] = Scale(Accent, 0.6f);
        style.Colors[ImGuiCol_ResizeGripActive] = Accent;
        style.Colors[ImGuiCol_Tab] = Tab;
        style.Colors[ImGuiCol_TabHovered] = TabHover;
        style.Colors[ImGuiCol_TabActive] = TabActive;
        style.Colors[ImGuiCol_TabUnfocused] = Tab;
        style.Colors[ImGuiCol_TabUnfocusedActive] = TabActive;
        style.Colors[ImGuiCol_DockingPreview] = SelectionBorder;
        style.Colors[ImGuiCol_DockingEmptyBg] = WindowBg;
        style.Colors[ImGuiCol_PlotLines] = Accent;
        style.Colors[ImGuiCol_PlotLinesHovered] = AccentHover;
        style.Colors[ImGuiCol_PlotHistogram] = Accent;
        style.Colors[ImGuiCol_PlotHistogramHovered] = AccentHover;
        style.Colors[ImGuiCol_TableHeaderBg] = FrameBg;
        style.Colors[ImGuiCol_TableBorderStrong] = Border;
        style.Colors[ImGuiCol_TableBorderLight] = Scale(Border, 0.8f);
        style.Colors[ImGuiCol_TableRowBg] = Transparent;
        style.Colors[ImGuiCol_TableRowBgAlt] = Scale(FrameBg, 0.8f);
        style.Colors[ImGuiCol_TextSelectedBg] = Selection;
        style.Colors[ImGuiCol_DragDropTarget] = Accent;
        style.Colors[ImGuiCol_NavHighlight] = SelectionBorder;
        style.Colors[ImGuiCol_NavWindowingHighlight] = Scale(SelectionBorder, 0.5f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.45f);
    }
}

#endif
