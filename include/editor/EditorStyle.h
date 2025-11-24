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

namespace EditorStyle {
    // Danger / destructive action colors
    static const ImVec4 DangerText         = ImVec4(0.80f, 0.12f, 0.12f, 1.0f);
    static const ImVec4 DangerButton       = ImVec4(0.72f, 0.10f, 0.10f, 1.0f);
    static const ImVec4 DangerButtonHover  = ImVec4(0.85f, 0.18f, 0.18f, 1.0f);
    static const ImVec4 DangerButtonActive = ImVec4(0.60f, 0.08f, 0.08f, 1.0f);

    // Transparent (useful for passthru backgrounds)
    static const ImVec4 Transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Primary / Accent
    static const ImVec4 Accent             = ImVec4(0.20f, 0.60f, 0.86f, 1.0f);
    static const ImVec4 AccentHover        = ImVec4(0.25f, 0.68f, 0.95f, 1.0f);
    static const ImVec4 AccentActive       = ImVec4(0.14f, 0.50f, 0.78f, 1.0f);

    // Positive / success
    static const ImVec4 SuccessText        = ImVec4(0.10f, 0.72f, 0.30f, 1.0f);
    static const ImVec4 SuccessButton      = ImVec4(0.08f, 0.62f, 0.26f, 1.0f);
    static const ImVec4 SuccessButtonHover = ImVec4(0.12f, 0.78f, 0.36f, 1.0f);

    // Warning / caution
    static const ImVec4 WarningText        = ImVec4(0.95f, 0.70f, 0.10f, 1.0f);
    static const ImVec4 WarningButton      = ImVec4(0.90f, 0.60f, 0.05f, 1.0f);

    // Log level colors
    static const ImVec4 LogInfo            = ImVec4(0.70f, 0.70f, 0.70f, 1.0f);
    static const ImVec4 LogDebug           = ImVec4(0.40f, 0.80f, 1.00f, 1.0f);
    static const ImVec4 LogWarning         = WarningText;
    static const ImVec4 LogCritical        = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);

    // Neutral UI colors
    static const ImVec4 Text               = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
    static const ImVec4 TextDisabled       = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
    static const ImVec4 WindowBg           = ImVec4(0.11f, 0.12f, 0.13f, 1.0f);
    static const ImVec4 FrameBg            = ImVec4(0.16f, 0.17f, 0.18f, 1.0f);
    static const ImVec4 FrameBgHover       = ImVec4(0.22f, 0.24f, 0.26f, 1.0f);
    static const ImVec4 FrameBgActive      = ImVec4(0.09f, 0.10f, 0.11f, 1.0f);
    static const ImVec4 Border             = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
    static const ImVec4 Separator          = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
    
    // Tabs
    static const ImVec4 Tab                = ImVec4(0.14f, 0.16f, 0.18f, 1.0f);
    static const ImVec4 TabHover           = ImVec4(0.20f, 0.22f, 0.24f, 1.0f);
    static const ImVec4 TabActive          = ImVec4(0.10f, 0.12f, 0.14f, 1.0f);

    // Selection / highlights
    static const ImVec4 Selection          = ImVec4(0.25f, 0.50f, 0.90f, 0.35f);
    static const ImVec4 SelectionBorder    = ImVec4(0.25f, 0.50f, 0.90f, 1.0f);

    // Misc
    static const ImVec4 PopupBg            = ImVec4(0.09f, 0.10f, 0.11f, 0.95f);
    static const ImVec4 TooltipBg          = ImVec4(0.08f, 0.09f, 0.10f, 0.95f);
    static const ImVec4 Muted              = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);

    // Small helper: apply some of these to ImGui style (optional helper function)
    inline void ApplyToImGuiStyle(ImGuiStyle& style) {
        style.Colors[ImGuiCol_Text] = Text;
        style.Colors[ImGuiCol_TextDisabled] = TextDisabled;
        style.Colors[ImGuiCol_WindowBg] = WindowBg;
        style.Colors[ImGuiCol_FrameBg] = FrameBg;
        style.Colors[ImGuiCol_FrameBgHovered] = FrameBgHover;
        style.Colors[ImGuiCol_FrameBgActive] = FrameBgActive;
        style.Colors[ImGuiCol_Border] = Border;
        style.Colors[ImGuiCol_Separator] = Separator;
        style.Colors[ImGuiCol_Header] = Tab;
        style.Colors[ImGuiCol_HeaderHovered] = TabHover;
        style.Colors[ImGuiCol_HeaderActive] = TabActive;
        style.Colors[ImGuiCol_Tab] = Tab;
        style.Colors[ImGuiCol_TabHovered] = TabHover;
        style.Colors[ImGuiCol_TabActive] = TabActive;
        style.Colors[ImGuiCol_Button] = Accent;
        style.Colors[ImGuiCol_ButtonHovered] = AccentHover;
        style.Colors[ImGuiCol_ButtonActive] = AccentActive;
        style.Colors[ImGuiCol_PopupBg] = PopupBg;
    }

    // Scale a color by a scalar
    inline ImVec4 Scale(const ImVec4& c, float s) {
        return ImVec4(c.x * s, c.y * s, c.z * s, c.w * s);
    }
}

#endif
