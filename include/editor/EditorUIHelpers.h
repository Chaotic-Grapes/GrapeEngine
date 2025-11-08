/* Start Header *****************************************************************
\file   EditorUIHelpers.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   2nd November 2025
\brief
Declares reusable, stateless ImGui helper functions for editing
JSON-backed properties in the editor.
***************************************************************************** */

#ifndef EDITORUIHELPERS_H
#define EDITORUIHELPERS_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace EditorUI {

    // Section management: Call BeginPropertySection with all labels before rendering rows
    void BeginPropertySection(const std::vector<std::string>& labels);
    void EndPropertySection();

    // Static value display (non-editable)
    void RenderStaticValueRow(const std::string& label, const std::string& value);

    // Single float with optional unit label
    void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
        nlohmann::json& data, const std::string& key, float dragSpeed);

    // Vector2D (X Y on one line)
    void RenderVector2DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, float dragSpeed);

    // Vector3D (X Y Z on one line)
    void RenderVector3DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey,
        const std::string& zKey, float dragSpeed);

    // Quaternion (X Y Z W on one line)
    void RenderQuaternionRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey,
        const std::string& zKey, const std::string& wKey, float dragSpeed);

    // Color picker
    void RenderColorProperty(const std::string& label, nlohmann::json& colorData);

    // Text input
    void RenderTextProperty(const std::string& label, nlohmann::json& data, const std::string& key);

    // Integer drag
    void RenderIntProperty(const std::string& label, nlohmann::json& data, const std::string& key);

    // Single checkbox (writes directly to JSON)
    void RenderCheckboxProperty(const std::string& label, nlohmann::json& data, const std::string& key);

    // Single checkbox (returns if changed, caller handles JSON write)
    // Use this when you need to manipulate bitflags or do custom processing
    bool RenderCheckboxPropertyReturn(const std::string& label, bool& value);

    // Two checkboxes on one row
    void RenderCheckboxRow(const std::string& label, nlohmann::json& data,
        const std::string& key1, const std::string& label1,
        const std::string& key2, const std::string& label2);

    // Accessors for alignment (used internally)
    float GetCurrentLabelOffset();
    float GetContentStartX();

    // Content width for scrolling (hardcoded for now)
    constexpr float GetContentWidth() { return 700.0f; }

} // namespace EditorUI

#endif // EDITORUIHELPERS_H