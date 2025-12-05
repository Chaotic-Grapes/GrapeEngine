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

    // Centralized registry APIs
    void RegisterLayout(WindowId id, float x, float y, float w, float h);
    bool ApplyLayout(WindowId id, ImGuiCond cond = ImGuiCond_Once);
    void ClearLayouts();
    void InitializeDefaultLayouts();

}

#endif
