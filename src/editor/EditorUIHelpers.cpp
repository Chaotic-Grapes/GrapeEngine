/* Start Header *****************************************************************/
/*!
\file   EditorUIHelpers.cpp
\author Foo Rui Qin (refactor)
\date   2nd November 2025
\brief
Implements reusable, stateless ImGui helper functions for editing
JSON-backed properties in the editor.
*/
/* End Header *******************************************************************/

#include "../editor/EditorUIHelpers.h"
#include <imgui.h>

namespace EditorUI {

// Strip any '##' suffix used for unique widget IDs and return display label
static std::string _displayLabel(const std::string& label) {
    size_t pos = label.find("##");
    if (pos != std::string::npos) {
        return label.substr(0, pos);
    }
    return label;
}

void RenderVector2DRow(const std::string& label, nlohmann::json& data,
    const std::string& xKey, const std::string& yKey, float dragSpeed, float labelOffset)
{
    // Strip '##' suffix (used to give unique IDs) for nicer display text
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());

    float x = data[xKey];
    float y = data[yKey];

    // X field
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::Text("X");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    // Hold to drag, double click to type
    if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[xKey] = x;
    }

    // Y field
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    ImGui::Text("Y");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[yKey] = y;
    }
}

void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
    nlohmann::json& data, const std::string& key, float dragSpeed, float labelOffset)
{
    // Strip '##' suffix for display label consistency
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());

    float value = data[key];
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    // Field label (e.g., "kg", "m", "deg") to provide unit context
    ImGui::Text("%s", fieldLabel.c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat(("##" + label).c_str(), &value, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[key] = value;
    }
}

void RenderTextProperty(const std::string& label, nlohmann::json& data,
    const std::string& key, float labelOffset)
{
    std::string value = data[key];
    char buf[128];
    strncpy_s(buf, value.c_str(), sizeof(buf) - 1);

    ImGui::Text("%s", label.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::SetNextItemWidth(100);
    // Text input field
    if (ImGui::InputText(("##" + key).c_str(), buf, sizeof(buf))) {
        data[key] = std::string(buf);
    }
}

void RenderIntProperty(const std::string& label, nlohmann::json& data,
    const std::string& key, float labelOffset)
{
    int value = data[key];
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::SetNextItemWidth(100);
    // Integer drag field
    if (ImGui::DragInt(("##" + label).c_str(), &value)) {
        data[key] = value;
    }
}

void RenderColorProperty(const std::string& label, nlohmann::json& colorData,
    float labelOffset)
{
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);

    // Convert JSON RGBA (0-255) to ImGui color (0-1)
    // Normalize it first
    float col[4] = {
        colorData["R"].get<float>() / 255.0f,
        colorData["G"].get<float>() / 255.0f,
        colorData["B"].get<float>() / 255.0f,
        colorData["A"].get<float>() / 255.0f
    };

    ImGui::SetNextItemWidth(180);
    // IMGUI HAS BUILT-IN COLOR EDITORS AND PICKERS
    // ColorEdit4 = RGBA editor with sliders + color square
    // NoInputs: hide RGBA numeric fields until interacting with picker
    // AlphaBar: show alpha channel (Unity-like)
    // NoLabel: remove inline label next to the color widget (we render our own)
    // PickerHueWheel: use wheel rather than vertical hue bar
    if (ImGui::ColorEdit4(("##" + label).c_str(), col,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel)) {
        colorData["R"] = col[0] * 255.0f;
        colorData["G"] = col[1] * 255.0f;
        colorData["B"] = col[2] * 255.0f;
        colorData["A"] = col[3] * 255.0f;
    }
}

void RenderReadOnlyText(const std::string& label, const std::string& value,
    float labelOffset)
{
    ImGui::Text("%s", label.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::TextDisabled("%s", value.c_str());  // Gray text = read-only
}

void RenderCheckboxRow(const std::string& label, nlohmann::json& data,
    const std::string& key1, const std::string& label1, const std::string& key2,
    const std::string& label2, float labelOffset)
{
    ImGui::Text("%s", label.c_str());
    ImGui::SameLine();

    bool value1 = data[key1];
    bool value2 = data[key2];

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::SetNextItemWidth(20);
    if (ImGui::Checkbox(("##" + key1).c_str(), &value1)) data[key1] = value1;
    ImGui::SameLine();
    ImGui::Text("%s", label1.c_str());

    ImGui::SameLine();
    ImGui::SetNextItemWidth(20);
    if (ImGui::Checkbox(("##" + key2).c_str(), &value2)) data[key2] = value2;
    ImGui::SameLine();
    ImGui::Text("%s", label2.c_str());  // Label next to second checkbox
}

} // namespace EditorUI