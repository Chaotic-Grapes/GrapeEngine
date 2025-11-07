/* Start Header *****************************************************************/
/*!
\file   EditorUIHelpers.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   2nd November 2025
\brief
Implements reusable, stateless ImGui helper functions for editing
JSON-backed properties in the editor.
*/
/* End Header *******************************************************************/

#include "../editor/EditorUIHelpers.h"
#include <imgui.h>

namespace EditorUI {

    static float currentLabelOffset = 0.0f;
    // Keep label-to-content padding modest; vector rows add axis column on top
    static constexpr float LABEL_PADDING = 12.0f;
    static constexpr float CONTENT_LABEL_PAD = 12.0f;

    // Unified content start: end of label column + axis label column + pad
    static float _axisColWidth() {
        return ImGui::CalcTextSize("W").x;
    }
    static float _contentStartX() {
        return currentLabelOffset + _axisColWidth() + CONTENT_LABEL_PAD;
    }

    static std::string _displayLabel(const std::string& label) {
        size_t pos = label.find("##");
        if (pos != std::string::npos) {
            return label.substr(0, pos);
        }
        return label;
    }

    void BeginPropertySection(const std::vector<std::string>& labels) {
        float maxWidth = 0.0f;
        for (const auto& label : labels) {
            std::string displayLabel = _displayLabel(label);
            float width = ImGui::CalcTextSize(displayLabel.c_str()).x;
            maxWidth = std::max(maxWidth, width);
        }
        currentLabelOffset = ImGui::GetCursorStartPos().x + maxWidth + LABEL_PADDING;
    }

    void EndPropertySection() {
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4.0f));
        currentLabelOffset = 0.0f;
    }

    void RenderVector2DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, float dragSpeed)
    {
        std::string displayLabel = _displayLabel(label);
        ImGui::Text("%s", displayLabel.c_str());

        float x = 0.0f;
        float y = 0.0f;
        if (data.contains(xKey) && data[xKey].is_number()) x = data[xKey].get<float>();
        if (data.contains(yKey) && data[yKey].is_number()) y = data[yKey].get<float>();

        // Layout constants - block-based positioning to avoid overlap
        const float fieldWidth = 100.0f;
        const float gap = 12.0f;        // space between adjacent fields
        const float labelPad = 12.0f;   // label-to-field padding
        const float rowStartX = currentLabelOffset;

        // Use a fixed axis label column width (match widest axis label 'W')
        float axisLabelWidth = _axisColWidth();

        // Compute blocks
        const float block1X = rowStartX;
        const float block1Width = axisLabelWidth + labelPad + fieldWidth;
        const float block2X = block1X + block1Width + gap;

        // Block 1: X
        ImGui::SameLine();
        ImGui::SetCursorPosX(block1X);
        ImGui::Text("X");
        ImGui::SameLine();
        ImGui::SetCursorPosX(block1X + axisLabelWidth + labelPad);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[xKey] = x;
        }

        // Block 2: Y
        ImGui::SameLine();
        ImGui::SetCursorPosX(block2X);
        ImGui::Text("Y");
        ImGui::SameLine();
        ImGui::SetCursorPosX(block2X + axisLabelWidth + labelPad);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[yKey] = y;
        }

        ImGui::Dummy(ImVec2(0, 4));
    }

    void RenderVector3DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, const std::string& zKey, float dragSpeed)
    {
        std::string displayLabel = _displayLabel(label);
        ImGui::Text("%s", displayLabel.c_str());

        float x = data.contains(xKey) && data[xKey].is_number() ? data[xKey].get<float>() : 0.0f;
        float y = data.contains(yKey) && data[yKey].is_number() ? data[yKey].get<float>() : 0.0f;
        float z = data.contains(zKey) && data[zKey].is_number() ? data[zKey].get<float>() : 0.0f;

        // Layout constants - block-based positioning
        const float fieldWidth = 100.0f;
        const float gap = 24.0f;        // space between adjacent fields
        const float labelPad = 12.0f;   // label-to-field padding
        const float rowStartX = currentLabelOffset;

        // Use a fixed axis label column width (match widest axis label 'W')
        float axisLabelWidth = _axisColWidth();
        const float blockX1 = rowStartX;
        const float blockX1Width = axisLabelWidth + labelPad + fieldWidth;
        const float blockX2 = blockX1 + blockX1Width + gap;

        // Block X
        ImGui::SameLine();
        ImGui::SetCursorPosX(blockX1);
        ImGui::Text("X");
        ImGui::SameLine();
        ImGui::SetCursorPosX(blockX1 + axisLabelWidth + labelPad);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[xKey] = x;
        }

        // Block Y
        ImGui::SameLine();
        ImGui::SetCursorPosX(blockX2);
        ImGui::Text("Y");
        ImGui::SameLine();
        ImGui::SetCursorPosX(blockX2 + axisLabelWidth + labelPad);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[yKey] = y;
        }

        // Second line: Z (single block)
        const float blockZ = rowStartX;
        ImGui::SetCursorPosX(blockZ);
        ImGui::Text("Z");
        ImGui::SameLine();
        ImGui::SetCursorPosX(blockZ + axisLabelWidth + labelPad);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "Z").c_str(), &z, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[zKey] = z;
        }
        ImGui::Dummy(ImVec2(0, 4));
    }

    void RenderQuaternionRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, const std::string& zKey, const std::string& wKey, float dragSpeed)
    {
        std::string displayLabel = _displayLabel(label);
        ImGui::Text("%s", displayLabel.c_str());

        float x = data.contains(xKey) && data[xKey].is_number() ? data[xKey].get<float>() : 0.0f;
        float y = data.contains(yKey) && data[yKey].is_number() ? data[yKey].get<float>() : 0.0f;
        float z = data.contains(zKey) && data[zKey].is_number() ? data[zKey].get<float>() : 0.0f;
        float w = data.contains(wKey) && data[wKey].is_number() ? data[wKey].get<float>() : 1.0f;

        // Layout constants - block-based positioning
        const float fieldWidth = 100.0f;
        const float gap = 24.0f;        // space between adjacent fields
        const float labelPad = 12.0f;   // label-to-field padding
        const float rowStartX = currentLabelOffset;

        // Use a fixed axis label column width (match widest axis label 'W')
        float axisLabelWidth = ImGui::CalcTextSize("W").x;
        const float block1X = rowStartX;
        const float block1Width = axisLabelWidth + labelPad + fieldWidth;
        const float block2X = block1X + block1Width + gap;

        // Block X
        ImGui::SameLine();
        ImGui::SetCursorPosX(block1X);
        ImGui::Text("X");
        ImGui::SameLine();
        ImGui::SetCursorPosX(block1X + axisLabelWidth + labelPad);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[xKey] = x;
        }

        // Block Y
        ImGui::SameLine();
        ImGui::SetCursorPosX(block2X);
        ImGui::Text("Y  ");
        ImGui::SameLine();
        ImGui::SetCursorPosX(block2X + axisLabelWidth + labelPad);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[yKey] = y;
        }

        // Second line: Z and W
        const float blockZ = rowStartX;
        const float blockZWidth = axisLabelWidth + labelPad + fieldWidth;
        const float blockW = blockZ + blockZWidth + gap;

        // Block Z
        ImGui::SetCursorPosX(blockZ);
        ImGui::Text("Z");
        ImGui::SameLine();
        ImGui::SetCursorPosX(blockZ + axisLabelWidth + labelPad);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "Z").c_str(), &z, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[zKey] = z;
        }

        // Block W
        ImGui::SameLine();
        ImGui::SetCursorPosX(blockW);
        ImGui::Text("W");
        ImGui::SameLine();
        ImGui::SetCursorPosX(blockW + axisLabelWidth + labelPad);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "W").c_str(), &w, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[wKey] = w;
        }

        ImGui::Dummy(ImVec2(0, 4));
    }

    void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
        nlohmann::json& data, const std::string& key, float dragSpeed)
    {
        std::string displayLabel = _displayLabel(label);
        ImGui::Text("%s", displayLabel.c_str());

        float value = 0.0f;
        if (data.contains(key) && data[key].is_number()) value = data[key].get<float>();

        ImGui::SameLine();
        float fieldLabelWidth = ImGui::CalcTextSize(fieldLabel.c_str()).x;
        float contentStartX = _contentStartX();
        ImGui::SetCursorPosX(contentStartX - fieldLabelWidth - 10.0f);
        ImGui::Text("%s", fieldLabel.c_str());

        ImGui::SameLine();
        ImGui::SetCursorPosX(contentStartX);
        ImGui::SetNextItemWidth(100);
        if (ImGui::DragFloat(("##" + label).c_str(), &value, dragSpeed, 0.0f, 0.0f, "%.2f")) {
            data[key] = value;
        }
    }

    void RenderTextProperty(const std::string& label, nlohmann::json& data,
        const std::string& key)
    {
        std::string value = data.value(key, std::string());
        char buf[128];
        strncpy_s(buf, value.c_str(), sizeof(buf) - 1);

        ImGui::Text("%s", label.c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(_contentStartX());
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputText(("##" + key).c_str(), buf, sizeof(buf))) {
            data[key] = std::string(buf);
        }
    }

    void RenderIntProperty(const std::string& label, nlohmann::json& data,
        const std::string& key)
    {
        int value = data.value(key, 0);
        std::string displayLabel = _displayLabel(label);
        ImGui::Text("%s", displayLabel.c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(_contentStartX());
        ImGui::SetNextItemWidth(100);
        if (ImGui::DragInt(("##" + label).c_str(), &value)) {
            data[key] = value;
        }
    }

    void RenderColorProperty(const std::string& label, nlohmann::json& colorData)
    {
        std::string displayLabel = _displayLabel(label);
        ImGui::Text("%s", displayLabel.c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(_contentStartX());

        auto _getChannel = [&](const char* key) -> float {
            if (!colorData.contains(key) || !colorData[key].is_number()) return 1.0f;
            float v = colorData[key].get<float>();
            if (v > 1.0f) v /= 255.0f;
            if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
            return v;
            };

        float col[4] = {
            _getChannel("R"),
            _getChannel("G"),
            _getChannel("B"),
            _getChannel("A")
        };

        // SetNextItemWidth controls the width of the ColorEdit widget
        ImGui::SetNextItemWidth(180);
        // ColorEdit4 flags:
        // - NoInputs: hide RGBA numeric inputs
        // - AlphaBar: show a separate alpha slider
        // - NoLabel: we render the label separately
        // - PickerHueWheel: use hue wheel style picker
        if (ImGui::ColorEdit4(("##" + label).c_str(), col,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel))
        {
            colorData["R"] = col[0];
            colorData["G"] = col[1];
            colorData["B"] = col[2];
            colorData["A"] = col[3];
        }
    }

    void RenderReadOnlyText(const std::string& label, const std::string& value)
    {
        ImGui::Text("%s", label.c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(_contentStartX());
        ImGui::TextDisabled("%s", value.c_str());
    }

    void RenderCheckboxRow(const std::string& label, nlohmann::json& data,
        const std::string& key1, const std::string& label1, const std::string& key2,
        const std::string& label2)
    {
        ImGui::Text("%s", label.c_str());
        ImGui::SameLine();

        bool value1 = data.value(key1, false);
        bool value2 = data.value(key2, false);

        ImGui::SetCursorPosX(_contentStartX());
        ImGui::SetNextItemWidth(20);
        if (ImGui::Checkbox(("##" + key1).c_str(), &value1)) data[key1] = value1;
        ImGui::SameLine();
        ImGui::Text("%s", label1.c_str());

        ImGui::SameLine();
        ImGui::SetNextItemWidth(20);
        if (ImGui::Checkbox(("##" + key2).c_str(), &value2)) data[key2] = value2;
        ImGui::SameLine();
        ImGui::Text("%s", label2.c_str());
    }

    float GetCurrentLabelOffset() {
        return currentLabelOffset;
    }

    float GetContentStartX() {
        return _contentStartX();
    }

} // namespace EditorUI