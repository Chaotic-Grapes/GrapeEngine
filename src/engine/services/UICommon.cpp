#include "services/UICommon.h"
#include <unordered_map>

namespace UICommon {
namespace {
    struct Rect { float x, y, w, h; };
    static std::unordered_map<WindowId, Rect> layouts;
}

void RegisterLayout(WindowId id, float x, float y, float w, float h) {
    layouts[id] = Rect{ x, y, w, h };
}

bool ApplyLayout(WindowId id, ImGuiCond cond) {
    const auto it = layouts.find(id);
    if (it == layouts.end()) return false;

    const Rect& r = it->second;
    ImGui::SetNextWindowPos(ImVec2(r.x, r.y), cond);
    ImGui::SetNextWindowSize(ImVec2(r.w, r.h), cond);
    return true;
}

void ClearLayouts() {
    layouts.clear();
}

void InitializeDefaultLayouts() {
    layouts.clear();
    // DebugUI defaults 
    RegisterLayout(WindowId::DEBUG_ENGINE, 10.0f, 10.0f, 350.0f, 200.0f);
    RegisterLayout(WindowId::DEBUG_PERF, 370.0f, 10.0f, 350.0f, 400.0f);
    RegisterLayout(WindowId::DEBUG_INPUT, 10.0f, 222.0f, 350.0f, 402.0f);
    RegisterLayout(WindowId::DEBUG_EDITOR, 730.0f, 10.0f, 350.0f, 400.0f);
    RegisterLayout(WindowId::DEBUG_AUDIO, 370.0f, 422.0f, 350.0f, 202.0f);

    // LevelEditor defaults
    RegisterLayout(WindowId::EDITOR_PLAYBACK, 650.0f, 0.0f, 334.0f, 57.0f);
    RegisterLayout(WindowId::EDITOR_ASSET_BROWSER, 0.0f, 600.0f, 1000.0f, 300.0f);
    RegisterLayout(WindowId::EDITOR_PREFAB_EDITOR, 350.0f, 200.0f, 470.0f, 300.0f);
}
} // namespace UICommon