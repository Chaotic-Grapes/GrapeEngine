/* Start Header *****************************************************************/
/*!
\file   UICommon.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Common UI helpers and a minimal layout registry for editor windows.
Provides canonical window IDs and APIs to register/apply persistent layouts.
Used by LevelEditor to keep window positions/sizes consistent across runs
and to initialize sensible defaults for panels.

Features:
- Canonical `WindowId` enum for consistent window identification
- Register/apply/clear per-window position and size presets
- Initialize default layouts for LevelEditor panels
*/
/* End Header *******************************************************************/

#include "services/UICommon.h"
#include <unordered_map>

namespace UICommon {
    namespace {
        struct Rect { float x, y, w, h; };
        static std::unordered_map<WindowId, Rect> layouts;
    }

    // Register a layout rectangle (pos/size) for a specific window ID.
    // Stored in-memory; can be cleared/reset via ClearLayouts.
    void RegisterLayout(WindowId id, float x, float y, float w, float h) {
        layouts[id] = Rect{ x, y, w, h };
    }

    // Apply a previously registered layout for a window.
    // Returns false if the window ID has no stored layout.
    bool ApplyLayout(WindowId id, ImGuiCond cond) {
        const auto it = layouts.find(id);
        if (it == layouts.end()) return false;

        const Rect& r = it->second;
        ImGui::SetNextWindowPos(ImVec2(r.x, r.y), cond);
        ImGui::SetNextWindowSize(ImVec2(r.w, r.h), cond);
        return true;
    }

    // Clear all stored layouts from the registry.
    // Use when resetting UI positions or switching profiles.
    void ClearLayouts() {
        layouts.clear();
    }

    // Initialize sensible default layouts for editor panels.
    // Called during EditorService initialization to seed positions/sizes.
    void InitializeDefaultLayouts() {
        layouts.clear();

        // LevelEditor defaults
        RegisterLayout(WindowId::EDITOR_PLAYBACK, 650.0f, 0.0f, 334.0f, 57.0f);
        RegisterLayout(WindowId::EDITOR_ASSET_BROWSER, 0.0f, 600.0f, 1000.0f, 300.0f);
        RegisterLayout(WindowId::EDITOR_PREFAB_EDITOR, 1150.0f, 1.0f, 450.0f, 900.0f);
    }
}
