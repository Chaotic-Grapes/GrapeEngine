/* Start Header *****************************************************************/
/*!
\file   ComponentWidgets.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025

\brief
Implements the stateless ImGui helper widgets declared in ComponentWidgets.h.
These functions provide consistent label alignment and spacing for editing
JSON-backed properties in the inspector. All helpers operate on JSON objects
so both entities and prefab files use the same UI drawing path.
*/
/* End Header *******************************************************************/

#include "ComponentWidgets.h"
#include "EditorStyle.h"
#include <imgui.h>
#include <algorithm>
#include <unordered_map>

namespace EditorUI {

    // Internal variables used to align fields consistently
    // They are not exposed in the header because users shouldn't modify them

    static float valueStartOffset = 0.0f;                  // Where editable fields should start horizontally
    static constexpr float FIELD_LABEL_GAP = 6.0f;         // Gap between axis label (X/Y/Z) and its drag field
    static constexpr float FIELD_GAP = 20.0f;              // Gap between consecutive fields (e.g. X to Y)
    static constexpr float UNIFIED_FIELD_START_X = 160.0f; // Where all fields start relative to section label
    static std::string s_propertyFilterLower;              // Lowercase filter for property row matching
    static std::unordered_map<const nlohmann::json*, const nlohmann::json*> s_defaultScopeMap; // Maps data to defaults
    static ImFont* s_symbolsFont = nullptr;                // Symbols font for icon-only buttons

    static const char* kResetIcon = "\xEF\x91\xBF";         // Reset icon (material: restart_alt)

    // Strips "##" suffixes so visible labels don't show internal IDs
    static std::string _displayLabel(const std::string& label) {
        size_t pos = label.find("##");
        return (pos != std::string::npos) ? label.substr(0, pos) : label;
    }

    // Ensures JSON is an object before trying to assign properties
    static inline void _ensureObject(nlohmann::json& j) {
        if (!j.is_object()) j = nlohmann::json::object();
    }

    // Lowercase helper for filter matching
    static std::string _toLower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    // Check whether the current filter allows a row to render
    static bool _filterAllowsLabel(const std::string& label) {
        if (s_propertyFilterLower.empty()) {
            return true;
        }
        std::string labelLower = _toLower(_displayLabel(label));
        return labelLower.find(s_propertyFilterLower) != std::string::npos;
    }

    // Recursively map data nodes to their default nodes for fast lookup
    static void _registerDefaultsRecursive(nlohmann::json& data, const nlohmann::json& defaults) {
        s_defaultScopeMap[&data] = &defaults;

        if (data.is_object() && defaults.is_object()) {
            for (auto it = data.begin(); it != data.end(); ++it) {
                const std::string& key = it.key();
                if (!defaults.contains(key)) {
                    continue;
                }
                nlohmann::json& child = it.value();
                const nlohmann::json& childDefaults = defaults.at(key);
                if (child.is_object() || child.is_array()) {
                    _registerDefaultsRecursive(child, childDefaults);
                }
            }
        } else if (data.is_array() && defaults.is_array()) {
            const size_t count = std::min(data.size(), defaults.size());
            for (size_t i = 0; i < count; ++i) {
                nlohmann::json& child = data[i];
                const nlohmann::json& childDefaults = defaults[i];
                if (child.is_object() || child.is_array()) {
                    _registerDefaultsRecursive(child, childDefaults);
                }
            }
        }
    }

    // Finds the default JSON node that corresponds to this data node
    static const nlohmann::json* _findDefaultsFor(const nlohmann::json& data) {
        auto it = s_defaultScopeMap.find(&data);
        if (it != s_defaultScopeMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Renders a right-aligned reset button for a property row
    static bool _renderResetButton(const std::string& id) {
        const float buttonSize = ImGui::GetFrameHeight();
        const float rightEdge = ImGui::GetWindowContentRegionMax().x;
        const float buttonX = rightEdge - buttonSize - ImGui::GetStyle().FramePadding.x;

        // Position the button correctly
        ImGui::SameLine();
        const float lineHeight = ImGui::GetTextLineHeight();
        const float frameHeight = ImGui::GetFrameHeight();
        const float y = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(y - (frameHeight - lineHeight) * 0.5f);
        ImGui::SetCursorPosX(buttonX);
        ImGui::PushID(("Reset_" + id).c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

        if (s_symbolsFont) ImGui::PushFont(s_symbolsFont);
        const bool clicked = ImGui::SmallButton(kResetIcon);
        if (s_symbolsFont) ImGui::PopFont();
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset to default");
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        ImGui::PopID();
        return clicked;
    }

    // -------------------------------------------------------------------------
    // Section Management
    // -------------------------------------------------------------------------

    // Begins a new property section
    // We compute where the editable fields should start so that all rows inside this section align correctly
    void BeginPropertySection(const std::vector<std::string>&) {
        // Cursor X = current starting point + fixed offset for all fields
        valueStartOffset = ImGui::GetCursorPosX() + UNIFIED_FIELD_START_X;
    }

    // Ends a property section
    // Inserts small spacing to ensure blocks of properties don't collapse into each other visually
    void EndPropertySection() {
        // Dummy creates invisible spacing (width = available content width)
        ImGui::Dummy(ImVec2(GetContentWidth(), 0.0f));
        // Additional vertical spacing between sections
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        valueStartOffset = 0.0f;
    }

    // Sets a case-insensitive filter used to hide non-matching property rows
    void SetPropertyFilter(const std::string& filter) {
        s_propertyFilterLower = _toLower(filter);
    }

    // Clears the active property filter so all rows render
    void ClearPropertyFilter() {
        s_propertyFilterLower.clear();
    }

    // Returns true if the current property filter allows this label to render
    bool PropertyFilterAllows(const std::string& label) {
        return _filterAllowsLabel(label);
    }

    // Sets the symbols font used for icon-only buttons in property rows
    void SetSymbolsFont(ImFont* symbolsFont) {
        s_symbolsFont = symbolsFont;
    }

    // Registers default values for the active component UI render
    void RegisterDefaultDataScope(nlohmann::json& data, const nlohmann::json& defaults) {
        s_defaultScopeMap.clear();
        _registerDefaultsRecursive(data, defaults);
    }

    // Clears the default scope map to avoid stale references
    void ClearDefaultDataScope() {
        s_defaultScopeMap.clear();
    }

    // -------------------------------------------------------------------------
    // Basic Value Rendering
    // -------------------------------------------------------------------------

    // Renders a read-only row showing a static text value
    // Used for things like file paths or IDs that the user should see but cannot edit directly
    void RenderStaticValueRow(const std::string& label, const std::string& value, bool grayed) {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        ImGui::Text("%s", _displayLabel(label).c_str());
        // Move to aligned field column ("W" is the widest character so it's used as reference)
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        if (grayed) {
            ImGui::TextDisabled("%s", value.c_str());
        }
        else {
            ImGui::Text("%s", value.c_str());
        }
    }

    // Renders a float property using a drag widget
    // The value is loaded from JSON and written back only if the user edits it
    // fieldLabel is an optional unit label like "kg" or "deg"
    void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
        nlohmann::json& data, const std::string& key, float dragSpeed, float min, float max)
    {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        // Ensure we are editing a JSON object (key value pairs)
        _ensureObject(data);
        ImGui::Text("%s", _displayLabel(label).c_str());
        float value = data.value(key, 0.0f);

        // Pretty much the same as static value
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Set the width for the drag widget so all floats look consistent
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::DragFloat(("##" + label).c_str(), &value, dragSpeed, min, max, "%.2f"))
            data[key] = value;

        // If a unit label is provided, draw it next to the field
        if (!fieldLabel.empty()) {
            ImGui::SameLine();
            ImGui::Text("%s", fieldLabel.c_str());
        }

        // Add a reset button when defaults exist and the value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {
            if (defaults->contains(key) && (*defaults)[key].is_number()) {
                const float defaultValue = (*defaults)[key].get<float>();
                if (value != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Vector Editing
    // -------------------------------------------------------------------------

    // Renders a 2D vector as X and Y fields on one row
    // Each component has a small axis label and a numeric field
    void RenderVector2DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, float dragSpeed)
    {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        _ensureObject(data);
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Load X and Y values from JSON
        float x = data.value(xKey, 0.0f);
        float y = data.value(yKey, 0.0f);

        const float fieldWidth = 90.0f;
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;

        // Just styling stuff
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset);
        ImGui::Text("X");

        // Move right by label width + gap and draw the X field
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0, 0, "%.2f"))
            data[xKey] = x;

        // Compute X position where Y should start (after X block)
        float yStartX = valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP;
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX);
        ImGui::Text("Y");

        // Move to the right for the Y field and draw it
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX + axisLabelWidth + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0, 0, "%.2f"))
            data[yKey] = y;

        // Add a reset button when defaults exist and any component differs
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {
            if (defaults->contains(xKey) && defaults->contains(yKey)) {
                const float defaultX = (*defaults)[xKey].get<float>();
                const float defaultY = (*defaults)[yKey].get<float>();
                if ((x != defaultX || y != defaultY) && _renderResetButton(label)) {
                    data[xKey] = defaultX;
                    data[yKey] = defaultY;
                }
            }
        }
    }

    // Renders a 3D vector as X, Y and Z fields on one row
    // Layout is the same idea as 2D but we add a third block
    void RenderVector3DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey,
        const std::string& zKey, float dragSpeed)
    {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        _ensureObject(data);
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Load all three components from JSON
        float vals[3] = { data.value(xKey, 0.0f), data.value(yKey, 0.0f), data.value(zKey, 0.0f) };
        const float fieldWidth = 90.0f;
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;

        // Labels for the three components
        const char* labels[3] = { "X", "Y", "Z" };

        // Matching keys for JSON updates
        const std::string keys[3] = { xKey, yKey, zKey };

        // Draw each component in sequence
        for (int i = 0; i < 3; i++) {
            // Compute horizontal start for this component
            float startX = valueStartOffset + i * (axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP);
            
            // Draw axis label at this position
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX);
            ImGui::Text("%s", labels[i]);

            // Draw the numeric field for this component
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX + axisLabelWidth + FIELD_LABEL_GAP);
            ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::DragFloat(("##" + label + labels[i]).c_str(), &vals[i], dragSpeed, 0, 0, "%.2f"))
                data[keys[i]] = vals[i];
        }

        // Add a reset button when defaults exist and any component differs
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {
            if (defaults->contains(xKey) && defaults->contains(yKey) && defaults->contains(zKey)) {
                const float defaultX = (*defaults)[xKey].get<float>();
                const float defaultY = (*defaults)[yKey].get<float>();
                const float defaultZ = (*defaults)[zKey].get<float>();
                if ((vals[0] != defaultX || vals[1] != defaultY || vals[2] != defaultZ) && _renderResetButton(label)) {
                    data[xKey] = defaultX;
                    data[yKey] = defaultY;
                    data[zKey] = defaultZ;
                }
            }
        }
    }

    // Renders a quaternion as X, Y, Z and W fields on one row
    // Exact same layout logic as the 3D vector, but with four components
    // The default value for W is 1 since that is the identity rotation
    void RenderQuaternionRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey,
        const std::string& zKey, const std::string& wKey, float dragSpeed)
    {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        _ensureObject(data);
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Load four components, default W to 1
        float vals[4] = { data.value(xKey, 0.0f), data.value(yKey, 0.0f),
                          data.value(zKey, 0.0f), data.value(wKey, 1.0f) };

        // Logic's the same as 3D
        const float fieldWidth = 90.0f;
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;
        const char* comps[4] = { "X", "Y", "Z", "W" };
        const std::string keys[4] = { xKey, yKey, zKey, wKey };

        // Draw all four components in a row
        for (int i = 0; i < 4; i++) {
            // Compute horizontal start for this component
            float startX = valueStartOffset + i * (axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP);

            // Draw axis label at this position
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX);
            ImGui::Text("%s", comps[i]);

            // Draw the numeric field for this component
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX + axisLabelWidth + FIELD_LABEL_GAP);
            ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::DragFloat(("##" + label + comps[i]).c_str(), &vals[i], dragSpeed, 0, 0, "%.2f"))
                data[keys[i]] = vals[i];
        }

        // Add a reset button when defaults exist and any component differs
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {
            if (defaults->contains(xKey) && defaults->contains(yKey) && defaults->contains(zKey) && defaults->contains(wKey)) {
                const float defaultX = (*defaults)[xKey].get<float>();
                const float defaultY = (*defaults)[yKey].get<float>();
                const float defaultZ = (*defaults)[zKey].get<float>();
                const float defaultW = (*defaults)[wKey].get<float>();
                if ((vals[0] != defaultX || vals[1] != defaultY || vals[2] != defaultZ || vals[3] != defaultW) &&
                    _renderResetButton(label)) {
                    data[xKey] = defaultX;
                    data[yKey] = defaultY;
                    data[zKey] = defaultZ;
                    data[wKey] = defaultW;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Property Editing
    // -------------------------------------------------------------------------

    // Renders a color picker tied to JSON RGBA values
    // The values are stored as HDR floats so bright colors are supported
    void RenderColorProperty(const std::string& label, nlohmann::json& colorData) {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
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

        // Move into the aligned field column
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);

        // Use a fixed width color picker
        ImGui::SetNextItemWidth(220.0f);

        // Draw color edit widget with HDR and no text inputs
        // NoInputs: hides numeric R/G/B/A inputs (cleaner UI)
        // AlphaBar: adds vertical bar for transparency
        // NoLabel: prevents ImGui from drawing a label (we draw ours)
        // PickerHueWheel: circular color wheel (more intuitive)
        // HDR: enables brightness values above 1.0
        // Float: treats color values as float (not 0�255)
        if (ImGui::ColorEdit4(("##" + label).c_str(), col,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel |
            ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float)) 
        {
            // Write new values back to JSON
            colorData["R"] = col[0];
            colorData["G"] = col[1];
            colorData["B"] = col[2];
            colorData["A"] = col[3];
        }

        // Add a reset button when defaults exist and any channel differs
        if (const nlohmann::json* defaults = _findDefaultsFor(colorData)) {
            if (defaults->contains("R") && defaults->contains("G") && defaults->contains("B") && defaults->contains("A")) {
                const float defaultR = (*defaults)["R"].get<float>();
                const float defaultG = (*defaults)["G"].get<float>();
                const float defaultB = (*defaults)["B"].get<float>();
                const float defaultA = (*defaults)["A"].get<float>();
                if ((col[0] != defaultR || col[1] != defaultG || col[2] != defaultB || col[3] != defaultA) &&
                    _renderResetButton(label)) {
                    colorData["R"] = defaultR;
                    colorData["G"] = defaultG;
                    colorData["B"] = defaultB;
                    colorData["A"] = defaultA;
                }
            }
        }
    }

    void RenderColorRow(const std::string& label, nlohmann::json& colorData) {
        RenderColorProperty(label, colorData);
    }

    // Renders a text field for editing a JSON string key
    // Reads the string, copies it into a local buffer, then writes back if the InputText reports a change
    void RenderTextProperty(const std::string& label, nlohmann::json& data, const std::string& key) {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        _ensureObject(data);

        // Get current string or empty if not set
        std::string value = data.value(key, std::string());

        // Local buffer used by ImGui input
        char buf[128];
        strncpy_s(buf, value.c_str(), sizeof(buf) - 1);

        // Usual stuff
        ImGui::Text("%s", _displayLabel(label).c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Set field width and draw text box
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::InputText(("##" + key).c_str(), buf, sizeof(buf)))
            // Only update JSON if value changed
            data[key] = std::string(buf);

        // Add a reset button when defaults exist and the value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {
            if (defaults->contains(key) && (*defaults)[key].is_string()) {
                const std::string defaultValue = (*defaults)[key].get<std::string>();
                if (data.value(key, std::string()) != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // Renders an integer property using a drag control
    // Reads the integer from JSON, lets the user drag to adjust it and writes the new value back when changed
    void RenderIntProperty(const std::string& label, nlohmann::json& data, const std::string& key) {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        _ensureObject(data);

        // Load integer value
        int value = data.value(key, 0);

        // Same same
        ImGui::Text("%s", _displayLabel(label).c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::DragInt(("##" + label).c_str(), &value))
            data[key] = value;

        // Add a reset button when defaults exist and the value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {
            if (defaults->contains(key) && (*defaults)[key].is_number_integer()) {
                const int defaultValue = (*defaults)[key].get<int>();
                if (value != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // Renders a dropdown for bitmask editing with labeled entries
    void RenderBitmaskDropdown(const std::string& label, nlohmann::json& data,
        const std::string& key, const std::vector<std::string>& bitNames,
        uint32_t defaultMask)
    {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        // Ensure JSON object
        _ensureObject(data);
        uint32_t mask = data.value(key, defaultMask); // Load current mask

        // Determine how many bits we have labels for (max 32)
        const size_t rawCount = bitNames.size();
        const int bitCount = static_cast<int>(std::min<size_t>(32, rawCount));
        const uint32_t allMask = (bitCount <= 0) ? 0u
            : (bitCount >= 32 ? 0xFFFFFFFFu : ((1u << bitCount) - 1u)); // Mask with all bits set

        // Count how many bits are currently selected
        int selectedCount = 0;
        for (int i = 0; i < bitCount; ++i) {
            if (mask & (1u << i)) {
                ++selectedCount;
            }
        }

        // Create summary string for the current selection
        std::string summary;
        if (bitCount <= 0) {
            summary = std::to_string(mask);
        }
        else if (selectedCount == 0) {
            summary = "None";
        }
        else if (mask == allMask) {
            summary = "All";
        }
        else {
            summary = std::to_string(selectedCount) + " of " + std::to_string(bitCount); // e.g. "1 of 3"
        }

        // Usual label rendering
        ImGui::Text("%s", _displayLabel(label).c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);
        ImGui::SetNextItemWidth(220.0f);

        // Dropdown combo box for bitmask
        // This part is a bit more complex since we need to handle multiple checkboxes inside the combo
        bool changed = false;
        if (ImGui::BeginCombo(("##" + label).c_str(), summary.c_str())) {
            if (bitCount > 0) {
                // "All" and "None" buttons for quick selection
                if (ImGui::SmallButton("All")) {
                    mask = allMask;
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("None")) {
                    mask = 0u;
                    changed = true;
                }
                ImGui::Separator(); // Two buttons above, checkboxes below

                // Render each bit as a checkbox with its label
                for (int i = 0; i < bitCount; ++i) {
                    bool isSet = (mask & (1u << i)) != 0; // Check if this bit is set
                    std::string entryLabel = bitNames[i].empty() // Fallback if no label provided
                        ? ("Bit " + std::to_string(i))
                        : bitNames[i];

                    // Draw checkbox and update mask if changed
                    if (ImGui::Checkbox((entryLabel + "##" + label + std::to_string(i)).c_str(), &isSet)) {
                        if (isSet) {
                            mask |= (1u << i); // Set the bit
                        }
                        else {
                            mask &= ~(1u << i); // Clear the bit
                        }
                        changed = true;
                    }
                }
            }
            ImGui::EndCombo();
        }

        // Write back to JSON if anything changed
        if (changed || !data.contains(key) || data[key] != mask) {
            data[key] = mask;
        }

        // Add a reset button when defaults exist and the value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {
            if (defaults->contains(key) && (*defaults)[key].is_number()) {
                const uint32_t defaultValue = (*defaults)[key].get<uint32_t>();
                if (mask != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // Renders a single checkbox tied directly to a JSON boolean key
    void RenderCheckboxProperty(const std::string& label, nlohmann::json& data, const std::string& key) {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        _ensureObject(data);
        ImGui::Text("%s", _displayLabel(label).c_str());
        bool value = data.value(key, false);

        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Draw checkbox and write back if toggled
        if (ImGui::Checkbox(("##" + label).c_str(), &value))
            data[key] = value;

        // Add a reset button when defaults exist and the value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {
            if (defaults->contains(key) && (*defaults)[key].is_boolean()) {
                const bool defaultValue = (*defaults)[key].get<bool>();
                if (value != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // Renders a checkbox based on a bool reference and returns true if it changed
    // This is useful when JSON is not written directly here and the caller wants to handle the change manually
    bool RenderCheckboxPropertyReturn(const std::string& label, bool& value) {
        if (!_filterAllowsLabel(label)) {
            return false; // Skip rows that do not match the current filter
        }
        ImGui::Text("%s", _displayLabel(label).c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Draw checkbox and return ImGui change result
        return ImGui::Checkbox(("##" + label).c_str(), &value);
    }

    // Renders two checkboxes on one row, both stored in JSON
    // The row label appears on the left and both checkboxes with their individual labels are rendered on the right
    void RenderCheckboxRow(const std::string& label, nlohmann::json& data, const std::string& key1, 
        const std::string& label1, const std::string& key2, const std::string& label2)
    {
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        _ensureObject(data);
        ImGui::Text("%s", label.c_str());

        // Load both boolean values
        bool value1 = data.value(key1, false);
        bool value2 = data.value(key2, false);

        const float axisLabelWidth = ImGui::CalcTextSize("W").x;
        const float fieldWidth = 90.0f;

        // First checkbox block
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        if (ImGui::Checkbox(("##" + label + "##" + key1).c_str(), &value1)) {
            data[key1] = value1;
        }

        // Label for first checkbox
        ImGui::SameLine();
        ImGui::Text("%s", label1.c_str());

        // Second checkbox block
        ImGui::SameLine();
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP);
        if (ImGui::Checkbox(("##" + label + "##" + key2).c_str(), &value2)) {
            data[key2] = value2;
        }

        // Label for second checkbox
        ImGui::SameLine();
        ImGui::Text("%s", label2.c_str());

        // Add a reset button when defaults exist and any value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {
            if (defaults->contains(key1) && defaults->contains(key2)) {
                const bool default1 = (*defaults)[key1].get<bool>();
                const bool default2 = (*defaults)[key2].get<bool>();
                if ((value1 != default1 || value2 != default2) && _renderResetButton(label)) {
                    data[key1] = default1;
                    data[key2] = default2;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Layout Helpers
    // -------------------------------------------------------------------------

    float GetContentStartX() { return valueStartOffset; }

    float GetContentWidth() {
        return std::max(0.0f, ImGui::GetContentRegionAvail().x);
    }

} 
