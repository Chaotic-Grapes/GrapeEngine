/* Start Header *****************************************************************/
/*!
\file   UICommon.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Common UI helpers and a minimal layout registry for editor windows.
Defines canonical window IDs and APIs to register/apply persistent layouts.
Used by LevelEditor to keep window positions/sizes consistent across runs
and to initialize sensible defaults for panels.

Features:
- Canonical `WindowId` enum for consistent window identification
- Register/apply/clear per-window position and size presets
- Initialize default layouts for LevelEditor panels
*/
/* End Header *******************************************************************/

#ifndef EDITOR_UICOMMON_H
#define EDITOR_UICOMMON_H

#include <imgui.h>

namespace UICommon {

    // Canonical window identifiers to centralize layout management
    enum class WindowId {
        EDITOR_PLAYBACK,
        EDITOR_ASSET_BROWSER,
        EDITOR_PREFAB_EDITOR
    };

    /**
     * @brief Register or update a persisted layout rectangle for a window id.
     * @param id Window identifier.
     * @param x Window position x in pixels.
     * @param y Window position y in pixels.
     * @param w Window width in pixels.
     * @param h Window height in pixels.
     */
    void RegisterLayout(WindowId id, float x, float y, float w, float h);

    /**
     * @brief Apply a stored layout rectangle to the current ImGui window.
     * @param id Window identifier.
     * @param cond ImGui condition controlling when layout is applied.
     * @return True when a stored layout exists for the given window id.
     */
    bool ApplyLayout(WindowId id, ImGuiCond cond = ImGuiCond_Once);

    /**
     * @brief Remove all registered layout rectangles.
     */
    void ClearLayouts();

    /**
     * @brief Register default layout rectangles used by editor windows.
     */
    void InitializeDefaultLayouts();

}

#endif
