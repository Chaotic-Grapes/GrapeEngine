/* Start Header *****************************************************************/
/*!
\file   ComponentWidgets.cpp
\author Foo Rui Qin (90%)
        Samantha Leong (10%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   12th March 2026
\brief
Implements the stateless ImGui helper widgets declared in ComponentWidgets.h.
These helpers enforce consistent inspector alignment, spacing, filtering, reset behavior
and undo wiring for JSON-backed component properties used by entities and prefab assets.
*/
/* End Header *******************************************************************/

#include "ComponentWidgets.h"
#include "EditorStyle.h"
#include "EditorIcons.h"
#include <imgui.h>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include "UndoSystem.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include "ecs/ComponentRegistry.h"
#include "serialization/EntitySerializer.h"

namespace EditorUI {

    // Internal variables used to align fields consistently
    static float valueStartOffset = 0.0f;                   // Where editable fields should start horizontally
    static constexpr float FIELD_LABEL_GAP = 6.0f;          // Gap between axis label (X/Y/Z) and its drag field
    static constexpr float FIELD_GAP = 20.0f;               // Gap between consecutive fields (e.g. X to Y)
    static constexpr float UNIFIED_FIELD_START_X = 160.0f;  // Where all fields start relative to section label
    static std::string s_propertyFilterLower;               // Lowercase filter for property row matching
    static std::unordered_map<const nlohmann::json*, const nlohmann::json*> s_defaultScopeMap; // Maps data to defaults
    static ImFont* s_symbolsFont = nullptr;                 // Symbols font for icon-only buttons
    static const std::unordered_set<EntityId>* s_selectedEntities = nullptr; // For multi-select support

    static const char* kResetIcon = EditorIcons::Reset;

    // Forward declarations for helpers used before their definitions
    static const nlohmann::json* _findDefaultsFor(const nlohmann::json& data);
    static bool _renderResetButton(const std::string& id);

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
        } 
        else if (data.is_array() && defaults.is_array()) {
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

    // Render a float property row with optional units, undo support, multi select propagation, and reset behavior
    void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
        nlohmann::json& data, const std::string& key, float dragSpeed, float min, float max,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& propertyPath,
        const std::function<void(void*, uint32_t, uint32_t, const std::string&, const nlohmann::json&)>& applyFn)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return;
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());
        float value = data.value(key, 0.0f);


        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);


        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(90.0f);
        
        const std::string itemId = "##" + label;
        if (ImGui::DragFloat(itemId.c_str(), &value, dragSpeed, min, max, "%.2f")) {
            data[key] = value;
        }

        // Wire this input through the undo system when editing live component data
        if (undoSystemPtr && worldPtr && componentTypeId != 0 && !propertyPath.empty()) {
            auto* undo = reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr);
            auto* world = reinterpret_cast<ECS::World*>(worldPtr);
            
            bool isMultiSelect = s_selectedEntities && s_selectedEntities->size() > 1 && s_selectedEntities->count(entityId);


            // Capture the old value at drag start so undo can restore the previous state
            if (ImGui::IsItemActivated()) {
                if (isMultiSelect) {
                    undo->BeginBatchPropertyEdit(*s_selectedEntities, componentTypeId, propertyPath);
                }
                else {
                    nlohmann::json oldVal = data.contains(key) ? data[key] : nlohmann::json(0.0f);
                    undo->BeginPropertyEdit(entityId, componentTypeId, propertyPath, oldVal);
                }
            }

            // Broadcast live drag updates to other selected entities during interaction
            if (ImGui::IsItemActive() && isMultiSelect) {

                // Real-time update for all selected entities during drag
                nlohmann::json newVal = value;

                // Apply the same edit to every secondary selected entity id
                for (EntityId id : *s_selectedEntities) {
                    if (id == entityId) continue; // Skip primary, already updated via data[key]

                    // Resolve stored entity id into a live ECS entity handle before applying changes
                    ECS::Entity other = world->Resolve(id);

                    // Avoid applying edits to entities that were deleted during interaction
                    if (world->IsAlive(other)) {
                        applyFn(worldPtr, id, componentTypeId, propertyPath, newVal);
                    }
                }
            }

            // Commit the final edited value and close the undo transaction
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                nlohmann::json newVal = data.contains(key) ? data[key] : nlohmann::json(value);
                if (isMultiSelect) {
                    undo->EndBatchPropertyEdit(*s_selectedEntities, componentTypeId, propertyPath, newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
                else {
                    undo->EndPropertyEdit(entityId, componentTypeId, propertyPath, newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
            }
        }

        if (!fieldLabel.empty()) {

            // Keep the next widget on the same row as the property label
            ImGui::SameLine();
            ImGui::Text("%s", fieldLabel.c_str());
        }

        // Look up scoped defaults so this row can expose reset behavior
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key) && (*defaults)[key].is_number()) {
                const float defaultValue = (*defaults)[key].get<float>();

                // Apply default value when the reset button is pressed
                if (value != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // Render a two component vector row with aligned X and Y controls and per axis undo wiring
    void RenderVector2DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, float dragSpeed,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& basePath,
        const std::function<void(void*, uint32_t, uint32_t, const std::string&, const nlohmann::json&)>& applyFn)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return;
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        float x = data.value(xKey, 0.0f);
        float y = data.value(yKey, 0.0f);


        // Use a shared numeric field width so vector component inputs align cleanly
        const float fieldWidth = 90.0f;

        // Use reference glyph width to keep axis label and field spacing consistent
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;

        // Place X field at the shared value column and then offset by axis label width
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset);
        ImGui::Text("X");

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0, 0, "%.2f")) {
            data[xKey] = x;
        }

        // Wire this input through the undo system when editing live component data
        if (undoSystemPtr && worldPtr && componentTypeId != 0 && !basePath.empty()) {
            auto* undo = reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr);
            auto* world = reinterpret_cast<ECS::World*>(worldPtr);
            bool isMultiSelect = s_selectedEntities && s_selectedEntities->size() > 1 && s_selectedEntities->count(entityId);


            // Capture the old value at drag start so undo can restore the previous state
            if (ImGui::IsItemActivated()) {
                if (isMultiSelect) {
                    undo->BeginBatchPropertyEdit(*s_selectedEntities, componentTypeId, basePath + ".X");
                }
                else {
                    nlohmann::json oldVal = data.contains(xKey) ? data[xKey] : nlohmann::json(0.0f);
                    undo->BeginPropertyEdit(entityId, componentTypeId, basePath + ".X", oldVal);
                }
            }


            // Broadcast live drag updates to other selected entities during interaction
            if (ImGui::IsItemActive() && isMultiSelect) {
                nlohmann::json newVal = x;

                // Apply the same edit to every secondary selected entity id
                for (EntityId id : *s_selectedEntities) {

                    // Skip the primary entity because it is already updated through local JSON state
                    if (id == entityId) continue;

                    // Resolve stored entity id into a live ECS entity handle before applying changes
                    ECS::Entity other = world->Resolve(id);

                    // Avoid applying edits to entities that were deleted during interaction
                    if (world->IsAlive(other)) applyFn(worldPtr, id, componentTypeId, basePath + ".X", newVal);
                }
            }


            // Commit the final edited value and close the undo transaction
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                nlohmann::json newVal = data.contains(xKey) ? data[xKey] : nlohmann::json(x);
                if (isMultiSelect) {
                    undo->EndBatchPropertyEdit(*s_selectedEntities, componentTypeId, basePath + ".X", newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
                else {
                    undo->EndPropertyEdit(entityId, componentTypeId, basePath + ".X", newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
            }
        }

        // Compute Y field origin by advancing past the full X axis block and configured inter field gap
        float yStartX = valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP;

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX);
        ImGui::Text("Y");

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX + axisLabelWidth + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0, 0, "%.2f")) {
            data[yKey] = y;
        }

        // Wire this input through the undo system when editing live component data
        if (undoSystemPtr && worldPtr && componentTypeId != 0 && !basePath.empty()) {
            auto* undo = reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr);
            auto* world = reinterpret_cast<ECS::World*>(worldPtr);
            bool isMultiSelect = s_selectedEntities && s_selectedEntities->size() > 1 && s_selectedEntities->count(entityId);


            // Capture the old value at drag start so undo can restore the previous state
            if (ImGui::IsItemActivated()) {
                if (isMultiSelect) {
                    undo->BeginBatchPropertyEdit(*s_selectedEntities, componentTypeId, basePath + ".Y");
                }
                else {
                    nlohmann::json oldVal = data.contains(yKey) ? data[yKey] : nlohmann::json(0.0f);
                    undo->BeginPropertyEdit(entityId, componentTypeId, basePath + ".Y", oldVal);
                }
            }


            // Broadcast live drag updates to other selected entities during interaction
            if (ImGui::IsItemActive() && isMultiSelect) {
                nlohmann::json newVal = y;

                // Apply the same edit to every secondary selected entity id
                for (EntityId id : *s_selectedEntities) {

                    // Skip the primary entity because it is already updated through local JSON state
                    if (id == entityId) continue;

                    // Resolve stored entity id into a live ECS entity handle before applying changes
                    ECS::Entity other = world->Resolve(id);

                    // Avoid applying edits to entities that were deleted during interaction
                    if (world->IsAlive(other)) applyFn(worldPtr, id, componentTypeId, basePath + ".Y", newVal);
                }
            }


            // Commit the final edited value and close the undo transaction
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                nlohmann::json newVal = data.contains(yKey) ? data[yKey] : nlohmann::json(y);
                if (isMultiSelect) {
                    undo->EndBatchPropertyEdit(*s_selectedEntities, componentTypeId, basePath + ".Y", newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
                else {
                    undo->EndPropertyEdit(entityId, componentTypeId, basePath + ".Y", newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
            }
        }


        // Look up scoped defaults so this row can expose reset behavior
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(xKey) && defaults->contains(yKey)) {
                const float defaultX = (*defaults)[xKey].get<float>();
                const float defaultY = (*defaults)[yKey].get<float>();

                // Apply default value when the reset button is pressed
                if ((x != defaultX || y != defaultY) && _renderResetButton(label)) {
                    data[xKey] = defaultX;
                    data[yKey] = defaultY;
                }
            }
        }
    }

    // Render a three component vector row with aligned X Y Z controls and per axis undo wiring
    void RenderVector3DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, const std::string& zKey, float dragSpeed,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& basePath,
        const std::function<void(void*, uint32_t, uint32_t, const std::string&, const nlohmann::json&)>& applyFn)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return;
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        float vals[3] = { data.value(xKey, 0.0f), data.value(yKey, 0.0f), data.value(zKey, 0.0f) };

        // Use a shared numeric field width so vector component inputs align cleanly
        const float fieldWidth = 90.0f;

        // Use reference glyph width to keep axis label and field spacing consistent
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;
        const char* labels[3] = { "X", "Y", "Z" };
        const std::string keys[3] = { xKey, yKey, zKey };
        const std::string axisSuffix[3] = { ".X", ".Y", ".Z" };

        for (int i = 0; i < 3; i++) {
            // Compute per axis start by repeating one axis block width for each index
            float startX = valueStartOffset + i * (axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP);

            // Keep the next widget on the same row as the property label
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX);
            ImGui::Text("%s", labels[i]);


            // Keep the next widget on the same row as the property label
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX + axisLabelWidth + FIELD_LABEL_GAP);

            // Use a fixed item width so rows stay visually aligned across components
            ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::DragFloat(("##" + label + labels[i]).c_str(), &vals[i], dragSpeed, 0, 0, "%.2f"))
                data[keys[i]] = vals[i];


            // Wire this input through the undo system when editing live component data
            if (undoSystemPtr && worldPtr && componentTypeId != 0 && !basePath.empty()) {
                auto* undo = reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr);
                auto* world = reinterpret_cast<ECS::World*>(worldPtr);
                bool isMultiSelect = s_selectedEntities && s_selectedEntities->size() > 1 && s_selectedEntities->count(entityId);


                // Capture the old value at drag start so undo can restore the previous state
                if (ImGui::IsItemActivated()) {
                    if (isMultiSelect) {
                        undo->BeginBatchPropertyEdit(*s_selectedEntities, componentTypeId, basePath + axisSuffix[i]);
                    }
                    else {
                        nlohmann::json oldVal = data.contains(keys[i]) ? data[keys[i]] : nlohmann::json(0.0f);
                        undo->BeginPropertyEdit(entityId, componentTypeId, basePath + axisSuffix[i], oldVal);
                    }
                }


                // Broadcast live drag updates to other selected entities during interaction
                if (ImGui::IsItemActive() && isMultiSelect) {
                    nlohmann::json newVal = vals[i];

                    // Apply the same edit to every secondary selected entity id
                    for (EntityId id : *s_selectedEntities) {

                        // Skip the primary entity because it is already updated through local JSON state
                        if (id == entityId) continue;

                        // Resolve stored entity id into a live ECS entity handle before applying changes
                        ECS::Entity other = world->Resolve(id);

                        // Avoid applying edits to entities that were deleted during interaction
                        if (world->IsAlive(other)) applyFn(worldPtr, id, componentTypeId, basePath + axisSuffix[i], newVal);
                    }
                }


                // Commit the final edited value and close the undo transaction
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    nlohmann::json newVal = data.contains(keys[i]) ? data[keys[i]] : nlohmann::json(vals[i]);
                    if (isMultiSelect) {
                        undo->EndBatchPropertyEdit(*s_selectedEntities, componentTypeId, basePath + axisSuffix[i], newVal,
                            [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                                applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                            });
                    }
                    else {
                        undo->EndPropertyEdit(entityId, componentTypeId, basePath + axisSuffix[i], newVal,
                            [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                                applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                            });
                    }
                }
            }
        }


        // Look up scoped defaults so this row can expose reset behavior
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(xKey) && defaults->contains(yKey) && defaults->contains(zKey)) {
                const float defaultX = (*defaults)[xKey].get<float>();
                const float defaultY = (*defaults)[yKey].get<float>();
                const float defaultZ = (*defaults)[zKey].get<float>();

                // Apply default value when the reset button is pressed
                if ((vals[0] != defaultX || vals[1] != defaultY || vals[2] != defaultZ) && _renderResetButton(label)) {
                    data[xKey] = defaultX;
                    data[yKey] = defaultY;
                    data[zKey] = defaultZ;
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
        ImGui::SetCursorPosY(ImGui::GetCursorPosY());
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

    // -------------------------------------------------------------------
    // Section Management
    // -------------------------------------------------------------------

    // Begins a new property section
    // We compute where the editable fields should start so that all rows inside this section align correctly
    void BeginPropertySection(const std::vector<std::string>&) {

        // Cursor X = current starting point + fixed offset for all fields
        valueStartOffset = ImGui::GetCursorPosX() + UNIFIED_FIELD_START_X;
    }

    // Ends a property section
    // Inserts small spacing to ensure blocks of properties don't collapse into each other visually
    void EndPropertySection(bool addSpacing) {
        if (addSpacing) {

            // Dummy creates invisible spacing (width = available content width)
            ImGui::Dummy(ImVec2(GetContentWidth(), 0.0f));

            // Additional vertical spacing between sections
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }
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

    // Store selected entity ids so interactive drags can broadcast edits to multi selection
    void SetSelectedEntities(const std::unordered_set<EntityId>* entities) {
        s_selectedEntities = entities;
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

    // -------------------------------------------------------------------
    // Basic Value Rendering
    // -------------------------------------------------------------------

    // Renders a read-only row showing a static text value
    // Used for things like file paths or IDs that the user should see but cannot edit directly
    void RenderStaticValueRow(const std::string& label, const std::string& value, bool grayed) {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Move to aligned field column ("W" is the widest character so it's used as reference)
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
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

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Ensure we are editing a JSON object (key value pairs)
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());
        float value = data.value(key, 0.0f);

        // Pretty much the same as static value
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Set the width for the drag widget so all floats look consistent
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::DragFloat(("##" + label).c_str(), &value, dragSpeed, min, max, "%.2f"))
            data[key] = value;

        // If a unit label is provided, draw it next to the field
        if (!fieldLabel.empty()) {

            // Keep the next widget on the same row as the property label
            ImGui::SameLine();
            ImGui::Text("%s", fieldLabel.c_str());
        }

        // Add a reset button when defaults exist and the value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key) && (*defaults)[key].is_number()) {
                const float defaultValue = (*defaults)[key].get<float>();

                // Apply default value when the reset button is pressed
                if (value != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // -------------------------------------------------------------------
    // Vector Editing
    // -------------------------------------------------------------------

    // Renders a 2D vector as X and Y fields on one row
    // Each component has a small axis label and a numeric field
    void RenderVector2DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, float dragSpeed)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Load X and Y values from JSON
        float x = data.value(xKey, 0.0f);
        float y = data.value(yKey, 0.0f);


        // Use a shared numeric field width so vector component inputs align cleanly
        const float fieldWidth = 90.0f;

        // Use reference glyph width to keep axis label and field spacing consistent
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;

        // Just styling stuff
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset);
        ImGui::Text("X");

        // Move right by label width + gap and draw the X field
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0, 0, "%.2f"))
            data[xKey] = x;

        // Compute X position where Y should start (after X block)
        float yStartX = valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP;

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX);
        ImGui::Text("Y");

        // Move to the right for the Y field and draw it
        ImGui::SameLine();
        ImGui::SetCursorPosX(yStartX + axisLabelWidth + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0, 0, "%.2f"))
            data[yKey] = y;

        // Add a reset button when defaults exist and any component differs
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(xKey) && defaults->contains(yKey)) {
                const float defaultX = (*defaults)[xKey].get<float>();
                const float defaultY = (*defaults)[yKey].get<float>();

                // Apply default value when the reset button is pressed
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

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Load all three components from JSON
        float vals[3] = { data.value(xKey, 0.0f), data.value(yKey, 0.0f), data.value(zKey, 0.0f) };

        // Use a shared numeric field width so vector component inputs align cleanly
        const float fieldWidth = 90.0f;

        // Use reference glyph width to keep axis label and field spacing consistent
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

            // Use a fixed item width so rows stay visually aligned across components
            ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::DragFloat(("##" + label + labels[i]).c_str(), &vals[i], dragSpeed, 0, 0, "%.2f"))
                data[keys[i]] = vals[i];
        }

        // Add a reset button when defaults exist and any component differs
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(xKey) && defaults->contains(yKey) && defaults->contains(zKey)) {
                const float defaultX = (*defaults)[xKey].get<float>();
                const float defaultY = (*defaults)[yKey].get<float>();
                const float defaultZ = (*defaults)[zKey].get<float>();

                // Apply default value when the reset button is pressed
                if ((vals[0] != defaultX || vals[1] != defaultY || vals[2] != defaultZ) && _renderResetButton(label)) {
                    data[xKey] = defaultX;
                    data[yKey] = defaultY;
                    data[zKey] = defaultZ;
                }
            }
        }
    }

    // Renders a 4D vector as X, Y, Z and W fields on one row
    void RenderVector4DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey,
        const std::string& zKey, const std::string& wKey, float dragSpeed)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        float vals[4] = {
            data.value(xKey, 0.0f),
            data.value(yKey, 0.0f),
            data.value(zKey, 0.0f),
            data.value(wKey, 0.0f)
        };

        // Use a shared numeric field width so vector component inputs align cleanly
        const float fieldWidth = 90.0f;

        // Use reference glyph width to keep axis label and field spacing consistent
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;
        const char* labels[4] = { "X", "Y", "Z", "W" };
        const std::string keys[4] = { xKey, yKey, zKey, wKey };

        for (int i = 0; i < 4; i++) {

            // Compute per axis start position by repeating one axis block per index
            float startX = valueStartOffset + i * (axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP);

            // Keep the next widget on the same row as the property label
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX);
            ImGui::Text("%s", labels[i]);

            // Keep the next widget on the same row as the property label
            ImGui::SameLine();
            ImGui::SetCursorPosX(startX + axisLabelWidth + FIELD_LABEL_GAP);

            // Use a fixed item width so rows stay visually aligned across components
            ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::DragFloat(("##" + label + labels[i]).c_str(), &vals[i], dragSpeed, 0, 0, "%.2f")) {
                data[keys[i]] = vals[i];
            }
        }


        // Look up scoped defaults so this row can expose reset behavior
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(xKey) && defaults->contains(yKey)
                && defaults->contains(zKey) && defaults->contains(wKey)) {
                const float defaultX = (*defaults)[xKey].get<float>();
                const float defaultY = (*defaults)[yKey].get<float>();
                const float defaultZ = (*defaults)[zKey].get<float>();
                const float defaultW = (*defaults)[wKey].get<float>();
                if ((vals[0] != defaultX || vals[1] != defaultY
                    || vals[2] != defaultZ || vals[3] != defaultW) && _renderResetButton(label)) {
                    data[xKey] = defaultX;
                    data[yKey] = defaultY;
                    data[zKey] = defaultZ;
                    data[wKey] = defaultW;
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

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Load four components, default W to 1
        float vals[4] = { data.value(xKey, 0.0f), data.value(yKey, 0.0f),
                          data.value(zKey, 0.0f), data.value(wKey, 1.0f) };

        // Logic's the same as 3D
        const float fieldWidth = 90.0f;

        // Use reference glyph width to keep axis label and field spacing consistent
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

            // Use a fixed item width so rows stay visually aligned across components
            ImGui::SetNextItemWidth(fieldWidth);
            if (ImGui::DragFloat(("##" + label + comps[i]).c_str(), &vals[i], dragSpeed, 0, 0, "%.2f"))
                data[keys[i]] = vals[i];
        }

        // Add a reset button when defaults exist and any component differs
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
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

    // -------------------------------------------------------------------
    // Property Editing
    // -------------------------------------------------------------------

    // RenderColorProperty plain overload
    // Renders a color picker tied to JSON RGBA values
    // The values are stored as HDR floats so bright colors are supported
    void RenderColorProperty(const std::string& label, nlohmann::json& colorData) {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }
        _ensureObject(colorData);

        // Render the left side property label using the visible label text
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

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);

        // Use a fixed width color picker
        ImGui::SetNextItemWidth(220.0f);

        // Draw color edit widget with HDR and no text inputs
        // NoInputs: hides numeric R/G/B/A inputs (cleaner UI)
        // AlphaBar: adds vertical bar for transparency
        // NoLabel: prevents ImGui from drawing a label (we draw ours)
        // PickerHueWheel: circular color wheel (more intuitive)
        // HDR: enables brightness values above 1.0
        // Float: treats color values as float and avoids legacy 0 to 255 integer interpretation
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

            // Only show reset when the default scope contains the required key set
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

    // RenderColorProperty undo overload
    // Renders the same RGBA editor as the plain version while integrating undo tracking
    // Captures the initial color once, applies live updates while dragging and commits one undo record after interaction ends
    void RenderColorProperty(const std::string& label, nlohmann::json& colorData,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& basePath,
        const std::function<void(void*, uint32_t, uint32_t, const std::string&, const nlohmann::json&)>& applyFn)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return;
        }
        _ensureObject(colorData);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        float col[4] = {
            colorData.value("R", 1.0f),
            colorData.value("G", 1.0f),
            colorData.value("B", 1.0f),
            colorData.value("A", 1.0f)
        };


        // Use reference glyph width to keep axis label and field spacing consistent
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(220.0f);

        // Write color channel values only when the color editor reports a real change
        if (ImGui::ColorEdit4(("##" + label).c_str(), col,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel |
            ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float))
        {
            colorData["R"] = col[0];
            colorData["G"] = col[1];
            colorData["B"] = col[2];
            colorData["A"] = col[3];
        }


        // Wire this input through the undo system when editing live component data
        if (undoSystemPtr && worldPtr && componentTypeId != 0 && !basePath.empty()) {
            auto* undo = reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr);
            auto* world = reinterpret_cast<ECS::World*>(worldPtr);
            bool isMultiSelect = s_selectedEntities && s_selectedEntities->size() > 1 && s_selectedEntities->count(entityId);


            // Capture the old value at drag start so undo can restore the previous state
            if (ImGui::IsItemActivated()) {
                if (isMultiSelect) {
                    undo->BeginBatchPropertyEdit(*s_selectedEntities, componentTypeId, basePath);
                }
                else {
                    nlohmann::json oldVal = colorData;
                    undo->BeginPropertyEdit(entityId, componentTypeId, basePath, oldVal);
                }
            }


            // Broadcast live drag updates to other selected entities during interaction
            if (ImGui::IsItemActive() && isMultiSelect) {
                nlohmann::json newVal = { {"R", col[0]}, {"G", col[1]}, {"B", col[2]}, {"A", col[3]} };

                // Apply the same edit to every secondary selected entity id
                for (EntityId id : *s_selectedEntities) {

                    // Skip the primary entity because it is already updated through local JSON state
                    if (id == entityId) continue;

                    // Resolve stored entity id into a live ECS entity handle before applying changes
                    ECS::Entity other = world->Resolve(id);

                    // Avoid applying edits to entities that were deleted during interaction
                    if (world->IsAlive(other)) applyFn(worldPtr, id, componentTypeId, basePath, newVal);
                }
            }


            // Commit the final edited value and close the undo transaction
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                nlohmann::json newVal = { {"R", col[0]}, {"G", col[1]}, {"B", col[2]}, {"A", col[3]} };
                if (isMultiSelect) {
                    undo->EndBatchPropertyEdit(*s_selectedEntities, componentTypeId, basePath, newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
                else {
                    undo->EndPropertyEdit(entityId, componentTypeId, basePath, newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
            }
        }


        // Look up scoped defaults so this row can expose reset behavior
        if (const nlohmann::json* defaults = _findDefaultsFor(colorData)) {

            // Only show reset when the default scope contains the required key set
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

    // Keep a compatibility wrapper for color row rendering entry points
    void RenderColorRow(const std::string& label, nlohmann::json& colorData) {
        RenderColorProperty(label, colorData);
    }

    // RenderTextProperty plain overload
    // Renders a text field for editing a JSON string key
    // Reads the string, copies it into a local buffer, then writes back if the InputText reports a change
    void RenderTextProperty(const std::string& label, nlohmann::json& data, const std::string& key) {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Get current string or empty if not set
        std::string value = data.value(key, std::string());

        // Local buffer used by ImGui input
        char buf[128];
        strncpy_s(buf, value.c_str(), sizeof(buf) - 1);

        // Usual stuff
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Set field width and draw text box
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::InputText(("##" + key).c_str(), buf, sizeof(buf)))

            // Only update JSON if value changed
            data[key] = std::string(buf);

        // Add a reset button when defaults exist and the value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key) && (*defaults)[key].is_string()) {
                const std::string defaultValue = (*defaults)[key].get<std::string>();

                // Apply default value when the reset button is pressed
                if (data.value(key, std::string()) != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // RenderTextProperty undo overload
    // Renders a text input for a JSON string and wraps the full edit session in one undoable change
    // Stores the original text before typing starts and submits a single undo command when editing is finalized
    void RenderTextProperty(const std::string& label, nlohmann::json& data, const std::string& key,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& propertyPath,
        const std::function<void(void*, uint32_t, uint32_t, const std::string&, const nlohmann::json&)>& applyFn)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return;
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);
        std::string value = data.value(key, std::string());
        char buf[128];
        strncpy_s(buf, value.c_str(), sizeof(buf) - 1);


        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::InputText(("##" + key).c_str(), buf, sizeof(buf))) {
            data[key] = std::string(buf);
        }


        // Wire this input through the undo system when editing live component data
        if (undoSystemPtr && worldPtr && componentTypeId != 0 && !propertyPath.empty()) {
            auto* undo = reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr);
            bool isMultiSelect = s_selectedEntities && s_selectedEntities->size() > 1 && s_selectedEntities->count(entityId);


            // Capture the old value at drag start so undo can restore the previous state
            if (ImGui::IsItemActivated()) {
                if (isMultiSelect) {
                    undo->BeginBatchPropertyEdit(*s_selectedEntities, componentTypeId, propertyPath);
                }
                else {
                    nlohmann::json oldVal = data.contains(key) ? data[key] : nlohmann::json(std::string());
                    undo->BeginPropertyEdit(entityId, componentTypeId, propertyPath, oldVal);
                }
            }


            // Commit the final edited value and close the undo transaction
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                nlohmann::json newVal = data.contains(key) ? data[key] : nlohmann::json(std::string(buf));
                if (isMultiSelect) {
                    undo->EndBatchPropertyEdit(*s_selectedEntities, componentTypeId, propertyPath, newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
                else {
                    undo->EndPropertyEdit(entityId, componentTypeId, propertyPath, newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
            }
        }


        // Look up scoped defaults so this row can expose reset behavior
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key) && (*defaults)[key].is_string()) {
                const std::string defaultValue = (*defaults)[key].get<std::string>();

                // Apply default value when the reset button is pressed
                if (data.value(key, std::string()) != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // RenderIntProperty plain overload
    // Renders an integer property using a drag control
    // Reads the integer from JSON, lets the user drag to adjust it and writes the new value back when changed
    void RenderIntProperty(const std::string& label, nlohmann::json& data, const std::string& key) {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Load integer value
        int value = data.value(key, 0);

        // Same same
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::DragInt(("##" + label).c_str(), &value))
            data[key] = value;

        // Add a reset button when defaults exist and the value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key) && (*defaults)[key].is_number_integer()) {
                const int defaultValue = (*defaults)[key].get<int>();

                // Apply default value when the reset button is pressed
                if (value != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // RenderIntProperty undo overload
    // Renders an integer drag field and records the interaction as one undo operation
    // This avoids generating one undo entry per frame while the control is being dragged
    void RenderIntProperty(const std::string& label, nlohmann::json& data, const std::string& key,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& propertyPath,
        const std::function<void(void*, uint32_t, uint32_t, const std::string&, const nlohmann::json&)>& applyFn)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return;
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);
        int value = data.value(key, 0);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::DragInt(("##" + label).c_str(), &value))
            data[key] = value;


        // Wire this input through the undo system when editing live component data
        if (undoSystemPtr && worldPtr && componentTypeId != 0 && !propertyPath.empty()) {
            auto* undo = reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr);
            auto* world = reinterpret_cast<ECS::World*>(worldPtr);
            bool isMultiSelect = s_selectedEntities && s_selectedEntities->size() > 1 && s_selectedEntities->count(entityId);


            // Capture the old value at drag start so undo can restore the previous state
            if (ImGui::IsItemActivated()) {
                if (isMultiSelect) {
                    undo->BeginBatchPropertyEdit(*s_selectedEntities, componentTypeId, propertyPath);
                }
                else {
                    nlohmann::json oldVal = data.contains(key) ? data[key] : nlohmann::json(0);
                    undo->BeginPropertyEdit(entityId, componentTypeId, propertyPath, oldVal);
                }
            }


            // Broadcast live drag updates to other selected entities during interaction
            if (ImGui::IsItemActive() && isMultiSelect) {
                nlohmann::json newVal = value;

                // Apply the same edit to every secondary selected entity id
                for (EntityId id : *s_selectedEntities) {

                    // Skip the primary entity because it is already updated through local JSON state
                    if (id == entityId) continue;

                    // Resolve stored entity id into a live ECS entity handle before applying changes
                    ECS::Entity other = world->Resolve(id);

                    // Avoid applying edits to entities that were deleted during interaction
                    if (world->IsAlive(other)) applyFn(worldPtr, id, componentTypeId, propertyPath, newVal);
                }
            }


            // Commit the final edited value and close the undo transaction
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                nlohmann::json newVal = data.contains(key) ? data[key] : nlohmann::json(value);
                if (isMultiSelect) {
                    undo->EndBatchPropertyEdit(*s_selectedEntities, componentTypeId, propertyPath, newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
                else {
                    undo->EndPropertyEdit(entityId, componentTypeId, propertyPath, newVal,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid, const std::string& path, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, path, v);
                        });
                }
            }
        }


        // Look up scoped defaults so this row can expose reset behavior
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key) && (*defaults)[key].is_number_integer()) {
                const int defaultValue = (*defaults)[key].get<int>();

                // Apply default value when the reset button is pressed
                if (value != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // RenderBitmaskDropdown plain overload
    // Renders a dropdown for bitmask editing with labeled entries
    void RenderBitmaskDropdown(const std::string& label, nlohmann::json& data,
        const std::string& key, const std::vector<std::string>& bitNames,
        uint32_t defaultMask)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Ensure JSON object
        _ensureObject(data);
        uint32_t mask = data.value(key, defaultMask); // Load current mask

        // Determine how many bits we have labels for (max 32)
        const size_t rawCount = bitNames.size();
        const int bitCount = static_cast<int>(std::min<size_t>(32, rawCount));

        // Build an all selected mask for quick comparisons while handling edge cases for 0 and 32 bits
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

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(220.0f);

        // Dropdown combo box for bitmask
        // This part is a bit more complex since we need to handle multiple checkboxes inside the combo
        bool changed = false;

        // Open combo content and handle nested option widgets only while expanded
        if (ImGui::BeginCombo(("##" + label).c_str(), summary.c_str())) {
            if (bitCount > 0) {

                // "All" and "None" buttons for quick selection
                if (ImGui::SmallButton("All")) {
                    mask = allMask;
                    changed = true;
                }

                // Keep the next widget on the same row as the property label
                ImGui::SameLine();

                // Clear every selected bit with one action
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

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key) && (*defaults)[key].is_number()) {
                const uint32_t defaultValue = (*defaults)[key].get<uint32_t>();

                // Apply default value when the reset button is pressed
                if (mask != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // RenderBitmaskDropdown undo overload
    // Renders a labeled bitmask dropdown and treats one popup session as a single undoable edit
    // Captures the pre-edit mask when interaction starts and commits the final mask when the popup closes
    void RenderBitmaskDropdown(const std::string& label, nlohmann::json& data,
        const std::string& key, const std::vector<std::string>& bitNames,
        uint32_t defaultMask,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& propertyPath,
        const std::function<void(void*, uint32_t, uint32_t, const std::string&, const nlohmann::json&)>& applyFn)
    {
        (void)worldPtr;

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return;
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);
        uint32_t mask = data.value(key, defaultMask);

        const size_t rawCount = bitNames.size();
        const int bitCount = static_cast<int>(std::min<size_t>(32, rawCount));

        // Build a full selected bitmask for quick all none and summary comparisons
        const uint32_t allMask = (bitCount <= 0) ? 0u
            : (bitCount >= 32 ? 0xFFFFFFFFu : ((1u << bitCount) - 1u));


        // Count selected bits to generate user friendly summary text for the dropdown
        int selectedCount = 0;
        for (int i = 0; i < bitCount; ++i) {
            if (mask & (1u << i)) ++selectedCount;
        }


        // Prepare compact selection summary text shown on the collapsed combo control
        std::string summary;
        if (bitCount <= 0) summary = std::to_string(mask);
        else if (selectedCount == 0) summary = "None";
        else if (mask == allMask) summary = "All";
        else summary = std::to_string(selectedCount) + " of " + std::to_string(bitCount);


        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Use a fixed item width so rows stay visually aligned across components
        ImGui::SetNextItemWidth(220.0f);

        bool changed = false;

        // Open combo content and handle nested option widgets only while expanded
        if (ImGui::BeginCombo(("##" + label).c_str(), summary.c_str())) {
            if (bitCount > 0) {

                // Select every available bit with one action
                if (ImGui::SmallButton("All")) {
                    uint32_t oldMask = mask;
                    mask = allMask;
                    changed = true;
                    if (undoSystemPtr) {
                        reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr)->RecordPropertyChange(
                            entityId, componentTypeId, propertyPath,
                            oldMask, mask,
                            [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid,
                                      const std::string& p, const nlohmann::json& v) {
                                applyFn(reinterpret_cast<void*>(w), e.Index, cid, p, v);
                            }
                        );
                    }
                }

                // Keep the next widget on the same row as the property label
                ImGui::SameLine();

                // Clear every selected bit with one action
                if (ImGui::SmallButton("None")) {
                    uint32_t oldMask = mask;
                    mask = 0u;
                    changed = true;
                    if (undoSystemPtr) {
                        reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr)->RecordPropertyChange(
                            entityId, componentTypeId, propertyPath,
                            oldMask, mask,
                            [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid,
                                      const std::string& p, const nlohmann::json& v) {
                                applyFn(reinterpret_cast<void*>(w), e.Index, cid, p, v);
                            }
                        );
                    }
                }
                ImGui::Separator();

                for (int i = 0; i < bitCount; ++i) {
                    bool isSet = (mask & (1u << i)) != 0;
                    std::string entryLabel = bitNames[i].empty() ? ("Bit " + std::to_string(i)) : bitNames[i];
                    if (ImGui::Checkbox((entryLabel + "##" + label + std::to_string(i)).c_str(), &isSet)) {
                        uint32_t oldMask = mask;
                        if (isSet) mask |= (1u << i);
                        else mask &= ~(1u << i);
                        changed = true;
                        if (undoSystemPtr) {
                            reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr)->RecordPropertyChange(
                                entityId, componentTypeId, propertyPath,
                                oldMask, mask,
                                [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid,
                                          const std::string& p, const nlohmann::json& v) {
                                    applyFn(reinterpret_cast<void*>(w), e.Index, cid, p, v);
                                }
                            );
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }

        if (changed || !data.contains(key) || data[key] != mask) {
            data[key] = mask;
        }
    }

    // RenderCheckboxProperty plain overload
    // Renders a single checkbox tied directly to a JSON boolean key
    void RenderCheckboxProperty(const std::string& label, nlohmann::json& data, const std::string& key) {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());
        bool value = data.value(key, false);


        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Draw checkbox and write back if toggled
        if (ImGui::Checkbox(("##" + label).c_str(), &value))
            data[key] = value;

        // Add a reset button when defaults exist and the value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key) && (*defaults)[key].is_boolean()) {
                const bool defaultValue = (*defaults)[key].get<bool>();

                // Apply default value when the reset button is pressed
                if (value != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // RenderCheckboxProperty undo overload
    // Renders a boolean checkbox with undo support so each toggle can be reverted
    // Queues an undo command only when the stored value actually changes
    void RenderCheckboxProperty(const std::string& label, nlohmann::json& data, const std::string& key,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& propertyPath,
        const std::function<void(void*, uint32_t, uint32_t, const std::string&, const nlohmann::json&)>& applyFn)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return;
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());
        bool value = data.value(key, false);


        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);
        if (ImGui::Checkbox(("##" + label).c_str(), &value)) {
            bool oldVal = data.value(key, false);
            data[key] = value;

            // Wire this input through the undo system when editing live component data
            if (undoSystemPtr && worldPtr && componentTypeId != 0 && !propertyPath.empty()) {
                auto* undo = reinterpret_cast<Editor::UndoSystem*>(undoSystemPtr);
                auto* world = reinterpret_cast<ECS::World*>(worldPtr);
                bool isMultiSelect = s_selectedEntities && s_selectedEntities->size() > 1 && s_selectedEntities->count(entityId);

                if (isMultiSelect) {
                    std::vector<Editor::BatchComponentPropertyCommand::Entry> entries;

                    // Resolve component short name from registry so we can locate JSON in the serialized entity
                    const auto& metaInfo = ECS::ComponentRegistry::Meta(componentTypeId);
                    const std::string compShortName = ECS::ComponentRegistry::GetComponentNameFromHash(metaInfo.TypeHash);


                    // Apply the same edit to every secondary selected entity id
                    for (EntityId id : *s_selectedEntities) {

                        // Resolve stored entity id into a live ECS entity handle before applying changes
                        ECS::Entity other = world->Resolve(id);
                        if (!world->IsAlive(other)) continue;

                        nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*world, other);
                        nlohmann::json* dataPtr = nullptr;

                        if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
                            for (auto& comp : entityJson["Components"]) {
                                if (!comp.contains("TypeName") || !comp["TypeName"].is_string()) continue;
                                std::string typeName = comp["TypeName"];
                                if (typeName == compShortName || typeName == "ECS::Components::" + compShortName) {
                                    if (comp.contains("Data") && comp["Data"].is_object()) {
                                        dataPtr = &comp["Data"];
                                    }
                                    break;
                                }
                            }
                        }

                        nlohmann::json oldValEnt = false;
                        if (dataPtr && dataPtr->is_object()) {
                            oldValEnt = dataPtr->contains(key) ? (*dataPtr)[key] : nlohmann::json(false);
                        }

                        entries.push_back({ id, oldValEnt, nlohmann::json(value) });
                    }

                    if (!entries.empty()) {
                        auto cmd = std::make_unique<Editor::BatchComponentPropertyCommand>(
                            world, componentTypeId, propertyPath, std::move(entries),
                            [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid,
                                      const std::string& p, const nlohmann::json& v) {
                                applyFn(reinterpret_cast<void*>(w), e.Index, cid, p, v);
                            });
                        undo->ExecuteCommand(std::move(cmd));
                    }
                }
                else {
                    undo->RecordPropertyChange(entityId, componentTypeId, propertyPath,
                        oldVal, value,
                        [applyFn](ECS::World* w, ECS::Entity e, ECS::ComponentTypeId cid,
                                  const std::string& p, const nlohmann::json& v) {
                            applyFn(reinterpret_cast<void*>(w), e.Index, cid, p, v);
                        }
                    );
                }
            }
        }


        // Look up scoped defaults so this row can expose reset behavior
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key) && (*defaults)[key].is_boolean()) {
                const bool defaultValue = (*defaults)[key].get<bool>();

                // Apply default value when the reset button is pressed
                if (value != defaultValue && _renderResetButton(label)) {
                    data[key] = defaultValue;
                }
            }
        }
    }

    // Renders a checkbox based on a bool reference and returns true if it changed
    // This is useful when JSON is not written directly here and the caller wants to handle the change manually
    bool RenderCheckboxPropertyReturn(const std::string& label, bool& value) {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return false; // Skip rows that do not match the current filter
        }

        // Render the left side property label using the visible label text
        ImGui::Text("%s", _displayLabel(label).c_str());

        // Keep the next widget on the same row as the property label
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + ImGui::CalcTextSize("W").x + FIELD_LABEL_GAP);

        // Draw checkbox and return ImGui change result
        return ImGui::Checkbox(("##" + label).c_str(), &value);
    }

    // Renders two checkboxes on one row, both stored in JSON
    // The row label appears on the left and both checkboxes with their individual labels are rendered on the right
    void RenderCheckboxRow(const std::string& label, nlohmann::json& data, const std::string& key1, 
        const std::string& label1, const std::string& key2, const std::string& label2)
    {

        // Skip this row when the active property filter does not match the label
        if (!_filterAllowsLabel(label)) {
            return; // Skip rows that do not match the current filter
        }

        // Guarantee we are writing into a JSON object before touching keys
        _ensureObject(data);
        ImGui::Text("%s", label.c_str());

        // Load both boolean values
        bool value1 = data.value(key1, false);
        bool value2 = data.value(key2, false);


        // Use reference glyph width to keep axis label and field spacing consistent
        const float axisLabelWidth = ImGui::CalcTextSize("W").x;

        // Use a shared numeric field width so vector component inputs align cleanly
        const float fieldWidth = 90.0f;

        // First checkbox block
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP);
        if (ImGui::Checkbox(("##" + label + "##" + key1).c_str(), &value1)) {
            data[key1] = value1;
        }

        // Label for first checkbox
        ImGui::SameLine();
        ImGui::Text("%s", label1.c_str());

        // Second checkbox block
        ImGui::SameLine();

        // Align the input widget to the unified field column for this section
        ImGui::SetCursorPosX(valueStartOffset + axisLabelWidth + FIELD_LABEL_GAP + fieldWidth + FIELD_GAP);
        if (ImGui::Checkbox(("##" + label + "##" + key2).c_str(), &value2)) {
            data[key2] = value2;
        }

        // Label for second checkbox
        ImGui::SameLine();
        ImGui::Text("%s", label2.c_str());

        // Add a reset button when defaults exist and any value changed
        if (const nlohmann::json* defaults = _findDefaultsFor(data)) {

            // Only show reset when the default scope contains the required key set
            if (defaults->contains(key1) && defaults->contains(key2)) {
                const bool default1 = (*defaults)[key1].get<bool>();
                const bool default2 = (*defaults)[key2].get<bool>();

                // Apply default value when the reset button is pressed
                if ((value1 != default1 || value2 != default2) && _renderResetButton(label)) {
                    data[key1] = default1;
                    data[key2] = default2;
                }
            }
        }
    }

    // -------------------------------------------------------------------
    // Layout Helpers
    // -------------------------------------------------------------------

    // Return the shared X coordinate used as the start of aligned property fields
    float GetContentStartX() { return valueStartOffset; }

    // Return available horizontal content width clamped to non negative values
    float GetContentWidth() {
        return std::max(0.0f, ImGui::GetContentRegionAvail().x);
    }

} 
