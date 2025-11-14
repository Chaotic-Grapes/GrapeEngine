/* Start Header *****************************************************************
\file   EditorUIHelpers.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   2nd November 2025
\brief
Implements reusable, stateless ImGui helper functions for editing
JSON-backed properties in the editor. Uses a fixed content width for
predictable horizontal scrolling and consistent alignment.
***************************************************************************** */

#include "../editor/EditorUIHelpers.h"
#include <imgui.h>

namespace EditorUI {

    static float currentLabelOffset = 0.0f;
    static float valueStartOffset = 0.0f;

    static constexpr float LABEL_PADDING = 25.0f;
    static constexpr float FIELD_LABEL_GAP = 8.0f;
    static constexpr float FIELD_GAP = 20.0f;

    static std::string _displayLabel(const std::string& label) {
        size_t pos = label.find("##");
        return (pos != std::string::npos) ? label.substr(0, pos) : label;
    }

    // Ensure the incoming JSON is an object to safely use value()/operator[]
    static inline void _ensureObject(nlohmann::json& j) {
        if (!j.is_object()) {
            j = nlohmann::json::object();
        }
    }


    // -------------------------------------------------------------------------
    // Section Management
    // -------------------------------------------------------------------------
    // Begin a property section and compute aligned label/value columns.
    // Uses the widest label to set a stable content start X.
    void BeginPropertySection(const std::vector<std::string>& labels) {
        float maxWidth = 0.0f;
        for (const auto& label : labels)
            maxWidth = std::max(maxWidth, ImGui::CalcTextSize(_displayLabel(label).c_str()).x);

        float baseX = ImGui::GetCursorPosX();
        currentLabelOffset = baseX;
        valueStartOffset = baseX + maxWidth + LABEL_PADDING;
    }

    // End a property section and reset alignment tracking.
    // Emits dummy items to ensure horizontal scroll ranges are correct.
    void EndPropertySection() {
        // defines total horizontal scroll extent
        ImGui::Dummy(ImVec2(GetContentWidth(), 0.0f));
        ImGui::Dummy(ImVec2(0.0f, 4.0f)); // vertical spacing
        currentLabelOffset = 0.0f;
        valueStartOffset = 0.0f;
    }

    // -------------------------------------------------------------------------
    // Static value row (Sprite: TextureId: 2)
    // -------------------------------------------------------------------------
    void RenderStaticValueRow(const std::string& label, const std::string& value) {
        ImGui::Text("%s", _displayLabel(label).c_str());
        ImGui::SameLine();

        float axisLabelWidth = ImGui::CalcTextSize("W").x;
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        ImGui::Text("%s", value.c_str());
    }

    // -------------------------------------------------------------------------
    // Float
    // -------------------------------------------------------------------------
    void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
        nlohmann::json& data, const std::string& key, float dragSpeed)
    {
        _ensureObject(data);
        ImGui::Text("%s", _displayLabel(label).c_str());
        float value = data.value(key, 0.0f);

        float axisLabelWidth = ImGui::CalcTextSize("W").x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::DragFloat(("##" + label).c_str(), &value, dragSpeed, 0, 0, "%.2f"))
            data[key] = value;

        if (!fieldLabel.empty()) {
            ImGui::SameLine();
            ImGui::Text("%s", fieldLabel.c_str());
        }
    }

    // -------------------------------------------------------------------------
    // Vector2D
    // -------------------------------------------------------------------------
    void RenderVector2DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, float dragSpeed)
    {
        _ensureObject(data);
        ImGui::Text("%s", _displayLabel(label).c_str());
        float x = data.value(xKey, 0.0f);
        float y = data.value(yKey, 0.0f);

        const float fieldWidth = 90.0f;
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;

        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset);
        ImGui::Text("X");
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0, 0, "%.2f"))
            data[xKey] = x;

        float yStartX = valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP;
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX);
        ImGui::Text("Y");
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX + axisLabelWidth + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0, 0, "%.2f"))
            data[yKey] = y;
    }

    // -------------------------------------------------------------------------
    // Vector3D (X Y Z on one line)
    // -------------------------------------------------------------------------
    void RenderVector3DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey,
        const std::string& zKey, float dragSpeed)
    {
        _ensureObject(data);
        ImGui::Text("%s", _displayLabel(label).c_str());
        float x = data.value(xKey, 0.0f);
        float y = data.value(yKey, 0.0f);
        float z = data.value(zKey, 0.0f);

        const float fieldWidth = 90.0f;
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;

        const char* labels[3] = { "X", "Y", "Z" };
        float* vals[3] = { &x, &y, &z };
        const std::string keys[3] = { xKey, yKey, zKey };

        for (int i = 0; i < 3; ++i) {
            float startX = valueStartOffset + i * (axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP);
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX);
            ImGui::Text("%s", labels[i]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX + axisLabelWidth + FIELD_LABEL_GAP);
            ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::DragFloat(("##" + label + labels[i]).c_str(), vals[i], dragSpeed, 0, 0, "%.2f"))
                data[keys[i]] = *vals[i];
        }
    }

    // -------------------------------------------------------------------------
    // Quaternion (X Y Z W on one line)
    // -------------------------------------------------------------------------
    void RenderQuaternionRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey,
        const std::string& zKey, const std::string& wKey, float dragSpeed)
    {
        _ensureObject(data);
        ImGui::Text("%s", _displayLabel(label).c_str());

        float x = data.value(xKey, 0.0f);
        float y = data.value(yKey, 0.0f);
        float z = data.value(zKey, 0.0f);
        float w = data.value(wKey, 1.0f);

        const float fieldWidth = 90.0f;
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;

        const char* comps[4] = { "X", "Y", "Z", "W" };
        float* vals[4] = { &x, &y, &z, &w };
        const std::string keys[4] = { xKey, yKey, zKey, wKey };

        for (int i = 0; i < 4; ++i) {
            float startX = valueStartOffset + i * (axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP);
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX);
            ImGui::Text("%s", comps[i]);
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX + axisLabelWidth + FIELD_LABEL_GAP);
            ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::DragFloat(("##" + label + comps[i]).c_str(), vals[i], dragSpeed, 0, 0, "%.2f"))
                data[keys[i]] = *vals[i];
        }
    }

    // -------------------------------------------------------------------------
    // Color
    // -------------------------------------------------------------------------
    void RenderColorProperty(const std::string& label, nlohmann::json& colorData) {
        _ensureObject(colorData);
        ImGui::Text("%s", _displayLabel(label).c_str());

#if 0
        // WORK IN PROGRESS; DON'T DELETE ANYTHING HERE
        // OLD CODE: This was causing HDR values to be divided by 255 every frame
        // When you entered "2.0" in the color picker, it would:
        // 1. Store 2.0 to JSON
        // 2. Next frame: read 2.0, see it's > 1.0, divide by 255 => 0.00784
        // 3. This created an infinite loop of division, clamping HDR colors to tiny values
        // The lambda was designed for legacy 0-255 integer format, but we're using HDR floats (0.0 to inf)
        auto get = [&](const char* k) {
            float v = colorData.value(k, 1.0f);
            return (v > 1.0f) ? v / 255.0f : v;
            };
        float col[4] = { get("R"), get("G"), get("B"), get("A") };
#else
        // Not fixed yet, color stores beyond LDR but rendering is not right
        // NEW CODE: Read HDR float values directly without conversion
        // HDR colors are stored as floats in the range [0.0, INF), so no conversion needed
        float col[4] = {
            colorData.value("R", 1.0f),
            colorData.value("G", 1.0f),
            colorData.value("B", 1.0f),
            colorData.value("A", 1.0f)
        };
#endif

        const float axisLabelWidth = ImGui::CalcTextSize("W").x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::ColorEdit4(("##" + label).c_str(), col,
            ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoLabel |
            ImGuiColorEditFlags_PickerHueWheel |
            ImGuiColorEditFlags_HDR |    // <-- Enable HDR
            ImGuiColorEditFlags_Float))  // <-- Optional but recommended for HDR
        {
            colorData["R"] = col[0];
            colorData["G"] = col[1];
            colorData["B"] = col[2];
            colorData["A"] = col[3];
        }
    }

    // -------------------------------------------------------------------------
    // Text / Int
    // -------------------------------------------------------------------------
    void RenderTextProperty(const std::string& label, nlohmann::json& data, const std::string& key) {
        _ensureObject(data);
        std::string value = data.value(key, std::string());
        char buf[128];
        strncpy_s(buf, value.c_str(), sizeof(buf) - 1);

        float axisLabelWidth = ImGui::CalcTextSize("W").x;
        ImGui::Text("%s", _displayLabel(label).c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::InputText(("##" + key).c_str(), buf, sizeof(buf)))
            data[key] = std::string(buf);
    }

    void RenderIntProperty(const std::string& label, nlohmann::json& data, const std::string& key) {
        _ensureObject(data);
        int value = data.value(key, 0);
        float axisLabelWidth = ImGui::CalcTextSize("W").x;

        ImGui::Text("%s", _displayLabel(label).c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::DragInt(("##" + label).c_str(), &value))
            data[key] = value;
    }

    // -------------------------------------------------------------------------
    // Single Checkbox Property (writes directly to JSON)
    // -------------------------------------------------------------------------
    void RenderCheckboxProperty(const std::string& label, nlohmann::json& data, const std::string& key)
    {
        _ensureObject(data);

        // Label column
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Retrieve current value
        bool value = data.value(key, false);

        // Layout constants (same pattern as RenderCheckboxRow)
        float axisLabelWidth = ImGui::CalcTextSize("W").x;

        // Move to same line and align to field start
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);

        // Use a stable, unique ID derived from the provided label
        // Example: "Filled##ShapeCircle2D" -> id "##Filled##ShapeCircle2D"
        std::string id = "##" + label;
        if (ImGui::Checkbox(id.c_str(), &value))
            data[key] = value;
    }

    // -------------------------------------------------------------------------
    // Single Checkbox Property (returns if changed, DOES NOT write to JSON)
    // Use this when you need to handle the value yourself (e.g. bitflag manipulation)
    // -------------------------------------------------------------------------
    bool RenderCheckboxPropertyReturn(const std::string& label, bool& value)
    {
        // Label column
        ImGui::Text("%s", _displayLabel(label).c_str());

        float axisLabelWidth = ImGui::CalcTextSize("W").x;

        // Move to same line and align to field start
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);

        // Render checkbox with unique ID based on label
        std::string id = "##" + label;
        bool changed = ImGui::Checkbox(id.c_str(), &value);

        return changed;
    }

    // -------------------------------------------------------------------------
    // Checkbox Row
    // -------------------------------------------------------------------------
    void RenderCheckboxRow(const std::string& label, nlohmann::json& data,
        const std::string& key1, const std::string& label1,
        const std::string& key2, const std::string& label2)
    {
        _ensureObject(data);
        ImGui::Text("%s", label.c_str());
        bool value1 = data.value(key1, false);
        bool value2 = data.value(key2, false);

        float axisLabelWidth = ImGui::CalcTextSize("W").x;
        float fieldWidth = 90.0f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        // Stable, unique IDs per row entry
        std::string id1 = "##" + label + "##" + key1;
        if (ImGui::Checkbox(id1.c_str(), &value1))
            data[key1] = value1;
        ImGui::SameLine();
        ImGui::Text("%s", label1.c_str());

        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP);
        std::string id2 = "##" + label + "##" + key2;
        if (ImGui::Checkbox(id2.c_str(), &value2))
            data[key2] = value2;
        ImGui::SameLine();
        ImGui::Text("%s", label2.c_str());
    }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------
    float GetCurrentLabelOffset() { return currentLabelOffset; }
    float GetContentStartX() { return valueStartOffset; }

} // namespace EditorUI