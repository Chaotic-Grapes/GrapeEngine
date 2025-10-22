#ifndef UICOMMON_H
#define UICOMMON_H

#include <imgui.h>

namespace UICommon {

// Canonical window identifiers to centralize layout management
enum class WindowId {
    DEBUG_ENGINE,
    DEBUG_PERF,
    DEBUG_AUDIO,
    DEBUG_INPUT,
    DEBUG_EDITOR,
    EDITOR_PLAYBACK
};

// Centralized registry APIs
void RegisterLayout(WindowId id, float x, float y, float w, float h);
bool ApplyLayout(WindowId id, ImGuiCond cond = ImGuiCond_Once);
void ClearLayouts();
void InitializeDefaultLayouts();

} // namespace UICommon

#endif