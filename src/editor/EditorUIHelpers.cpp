/* Start Header *****************************************************************/
/*!
\file   EditorUIHelpers.cpp
\author Foo Rui Qin
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

    float x = data[xKey];
    float y = data[yKey];

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

// Render a draggable float field with an optional unit label (e.g. "m", "kg")
void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
    nlohmann::json& data, const std::string& key, float dragSpeed)
{
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());

    float value = data[key];

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
    std::string value = data[key];
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
    int value = data[key];
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

// Render a color picker widget backed by RGBA values stored in JSON (0–255)
void RenderColorProperty(const std::string& label, nlohmann::json& colorData)
{
    // Strip '##' suffix for display purposes (used only for ImGui unique IDs)
    std::string displayLabel = _displayLabel(label);
    ImGui::Text("%s", displayLabel.c_str());
    ImGui::SameLine();
    // Position at absolute offset for alignment
    ImGui::SetCursorPosX(currentLabelOffset);

    // Convert JSON RGBA (0–255 range) to ImGui color (0–1 range)
    // ImGui's ColorEdit expects normalized floats between 0 and 1
    float col[4] = {
        colorData["R"].get<float>() / 255.0f,
        colorData["G"].get<float>() / 255.0f,
        colorData["B"].get<float>() / 255.0f,
        colorData["A"].get<float>() / 255.0f
    };

    ImGui::SetNextItemWidth(180);
    // Render ImGui color picker
    // ImGuiColorEditFlags_NoInputs: hides the numeric RGBA input boxes
    // ImGuiColorEditFlags_AlphaBar: shows an alpha (transparency) bar
    // ImGuiColorEditFlags_NoLabel: removes inline label (we already draw our own)
    // ImGuiColorEditFlags_PickerHueWheel: uses a hue wheel instead of a vertical hue bar
    if (ImGui::ColorEdit4(("##" + label).c_str(), col,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel))
    {
        // Convert back from ImGui's 0–1 range to 0–255 before storing in JSON
        colorData["R"] = col[0] * 255.0f;
        colorData["G"] = col[1] * 255.0f;
        colorData["B"] = col[2] * 255.0f;
        colorData["A"] = col[3] * 255.0f;
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

    bool value1 = data[key1];
    bool value2 = data[key2];

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