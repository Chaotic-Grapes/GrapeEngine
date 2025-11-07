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

// Static state for automatic label alignment within property sections
// This allows all properties in a section to align perfectly without manual offset calculations
static float currentLabelOffset = 0.0f;
static constexpr float LABEL_PADDING = 65.0f;

// Strip any '##' suffix used for unique widget IDs and return display label
// ImGui uses ## to create unique IDs while hiding the suffix from display
static std::string _displayLabel(const std::string& label) {
    size_t pos = label.find("##");
    if (pos != std::string::npos) {
        return label.substr(0, pos);
    }
    return label;
}

// Begin a property section: calculates max label width for alignment
// Pass in all labels that will be rendered in this section
// We calculate the widest label and use that for consistent spacing
void BeginPropertySection(const std::vector<std::string>& labels) {
    float maxWidth = 0.0f;
    for (const auto& label : labels) {
        std::string displayLabel = _displayLabel(label);
        float width = ImGui::CalcTextSize(displayLabel.c_str()).x;
        maxWidth = std::max(maxWidth, width);
    }
    // Store absolute X position where all controls should start
    currentLabelOffset = ImGui::GetCursorStartPos().x + maxWidth + LABEL_PADDING;
}

// End property section: resets alignment state
// Call this when done rendering a group of properties
void EndPropertySection() {
    currentLabelOffset = 0.0f;
}

// Render a row with draggable float fields for a 2D vector (X, Y)
void RenderVector2DRow(const std::string& label, nlohmann::json& data,
    const std::string& xKey, const std::string& yKey, float dragSpeed)
{
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());

    float x = 0.0f;
    float y = 0.0f;
    if (data.contains(xKey) && data[xKey].is_number()) x = data[xKey].get<float>();
    if (data.contains(yKey) && data[yKey].is_number()) y = data[yKey].get<float>();

    // Position "X" label just before the alignment point
    ImGui::SameLine();
    float xLabelWidth = ImGui::CalcTextSize("X").x;
    ImGui::SetCursorPosX(currentLabelOffset - xLabelWidth - 10.0f);
    ImGui::Text("X");

    // Position X input box at alignment point
    ImGui::SameLine();
    ImGui::SetCursorPosX(currentLabelOffset);
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[xKey] = x;
    }

    // Y label and field
    ImGui::SameLine();
    float yLabelWidth = ImGui::CalcTextSize("Y").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
    ImGui::Text("Y");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[yKey] = y;
    }
}

// Render a row with draggable float fields for a 3D vector (X, Y, Z)
void RenderVector3DRow(const std::string& label, nlohmann::json& data,
    const std::string& xKey, const std::string& yKey, const std::string& zKey, float dragSpeed)
{
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());

    float x = data.contains(xKey) && data[xKey].is_number() ? data[xKey].get<float>() : 0.0f;
    float y = data.contains(yKey) && data[yKey].is_number() ? data[yKey].get<float>() : 0.0f;
    float z = data.contains(zKey) && data[zKey].is_number() ? data[zKey].get<float>() : 0.0f;

    // X label and field
    ImGui::SameLine();
    float xLabelWidth = ImGui::CalcTextSize("X").x;
    ImGui::SetCursorPosX(currentLabelOffset - xLabelWidth - 10.0f);
    ImGui::Text("X");
    ImGui::SameLine();
    ImGui::SetCursorPosX(currentLabelOffset);
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[xKey] = x;
    }

    // Y label and field
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
    ImGui::Text("Y");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[yKey] = y;
    }

    // Z label and field
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
    ImGui::Text("Z");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat(("##" + label + "Z").c_str(), &z, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[zKey] = z;
    }
}

// Render a row with draggable float fields for a quaternion (X, Y, Z, W)
void RenderQuaternionRow(const std::string& label, nlohmann::json& data,
    const std::string& xKey, const std::string& yKey, const std::string& zKey, const std::string& wKey, float dragSpeed)
{
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());

    float x = data.contains(xKey) && data[xKey].is_number() ? data[xKey].get<float>() : 0.0f;
    float y = data.contains(yKey) && data[yKey].is_number() ? data[yKey].get<float>() : 0.0f;
    float z = data.contains(zKey) && data[zKey].is_number() ? data[zKey].get<float>() : 0.0f;
    float w = data.contains(wKey) && data[wKey].is_number() ? data[wKey].get<float>() : 1.0f;

    // X
    ImGui::SameLine();
    float xLabelWidth = ImGui::CalcTextSize("X").x;
    ImGui::SetCursorPosX(currentLabelOffset - xLabelWidth - 10.0f);
    ImGui::Text("X");
    ImGui::SameLine();
    ImGui::SetCursorPosX(currentLabelOffset);
    ImGui::SetNextItemWidth(70);
    if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[xKey] = x;
    }

    // Y
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
    ImGui::Text("Y");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[yKey] = y;
    }

    // Z
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
    ImGui::Text("Z");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    if (ImGui::DragFloat(("##" + label + "Z").c_str(), &z, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[zKey] = z;
    }

    // W
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
    ImGui::Text("W");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    if (ImGui::DragFloat(("##" + label + "W").c_str(), &w, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[wKey] = w;
    }
}

// Render a draggable float field with an optional unit label (e.g. "m", "kg")
void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
    nlohmann::json& data, const std::string& key, float dragSpeed)
{
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());

    float value = 0.0f;
    if (data.contains(key) && data[key].is_number()) value = data[key].get<float>();

    // Position field label just before the alignment point
    ImGui::SameLine();
    float fieldLabelWidth = ImGui::CalcTextSize(fieldLabel.c_str()).x;
    ImGui::SetCursorPosX(currentLabelOffset - fieldLabelWidth - 10.0f);
    ImGui::Text("%s", fieldLabel.c_str());

    // Position input box at alignment point
    ImGui::SameLine();
    ImGui::SetCursorPosX(currentLabelOffset);
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat(("##" + label).c_str(), &value, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[key] = value;
    }
}

// Render a text input property backed by a JSON string
void RenderTextProperty(const std::string& label, nlohmann::json& data,
    const std::string& key)
{
    std::string value = data.value(key, std::string());
    char buf[128];
    strncpy_s(buf, value.c_str(), sizeof(buf) - 1);

    ImGui::Text("%s", label.c_str());
    ImGui::SameLine();
    // Position at absolute offset for alignment
    ImGui::SetCursorPosX(currentLabelOffset);
    ImGui::SetNextItemWidth(100);
    // Text input field
    if (ImGui::InputText(("##" + key).c_str(), buf, sizeof(buf))) {
        data[key] = std::string(buf);
    }
}

// Render an integer property with a draggable input
void RenderIntProperty(const std::string& label, nlohmann::json& data,
    const std::string& key)
{
    int value = data.value(key, 0);
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());
    ImGui::SameLine();
    // Position at absolute offset for alignment
    ImGui::SetCursorPosX(currentLabelOffset);
    ImGui::SetNextItemWidth(100);
    // Integer drag field
    if (ImGui::DragInt(("##" + label).c_str(), &value)) {
        data[key] = value;
    }
}

// Render a color picker widget backed by RGBA values stored in JSON (0.0–1.0 floats)
// Backward-compatible: if existing JSON uses 0–255 ints, normalize on read and store floats.
void RenderColorProperty(const std::string& label, nlohmann::json& colorData)
{
    // Strip '##' suffix for display purposes (used only for ImGui unique IDs)
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());
    ImGui::SameLine();
    // Position at absolute offset for alignment
    ImGui::SetCursorPosX(currentLabelOffset);

    // Read RGBA from JSON as floats. If values look like 0–255 ints, normalize to 0–1.
    auto _getChannel = [&](const char* key) -> float {
        if (!colorData.contains(key) || !colorData[key].is_number()) return 1.0f;
        float v = colorData[key].get<float>();
        // If legacy integer representation is detected, normalize
        if (v > 1.0f) v /= 255.0f;
        // Clamp to [0,1] for safety
        if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
        return v;
    };

    float col[4] = {
        _getChannel("R"),
        _getChannel("G"),
        _getChannel("B"),
        _getChannel("A")
    };

    ImGui::SetNextItemWidth(180);
    // Render ImGui color picker
    if (ImGui::ColorEdit4(("##" + label).c_str(), col,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel))
    {
        // Store normalized floats directly in JSON (0.0–1.0)
        colorData["R"] = col[0];
        colorData["G"] = col[1];
        colorData["B"] = col[2];
        colorData["A"] = col[3];
    }
}

// Render a disabled (read-only) text label-value pair
void RenderReadOnlyText(const std::string& label, const std::string& value)
{
    ImGui::Text("%s", label.c_str());
    ImGui::SameLine();
    // Position at absolute offset for alignment
    ImGui::SetCursorPosX(currentLabelOffset);
    ImGui::TextDisabled("%s", value.c_str());  // Gray text = read-only
}

// Render a pair of checkboxes with labels, each bound to a JSON boolean field
void RenderCheckboxRow(const std::string& label, nlohmann::json& data,
    const std::string& key1, const std::string& label1, const std::string& key2,
    const std::string& label2)
{
    ImGui::Text("%s", label.c_str());
    ImGui::SameLine();

    bool value1 = data.value(key1, false);
    bool value2 = data.value(key2, false);

    // Position at absolute offset for alignment
    ImGui::SetCursorPosX(currentLabelOffset);
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

// Get the current alignment offset (for custom rendering that needs to align with other properties)
float GetCurrentLabelOffset() {
    return currentLabelOffset;
}

} // namespace EditorUI