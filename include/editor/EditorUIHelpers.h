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

    // This starts a property section with given labels
    // It sets up alignment and spacing
    void BeginPropertySection(const std::vector<std::string>& labels);
    // This closes the opened property section
    // It flushes layout and spacing
    void EndPropertySection();

    // This shows a read only value row
    // It does not change the JSON data
    void RenderStaticValueRow(const std::string& label, const std::string& value);

    // This edits a single float with a label
    // It writes value to the JSON key
    void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
        nlohmann::json& data, const std::string& key, float dragSpeed);

    // This edits a two component vector
    // It writes X and Y to JSON
    void RenderVector2DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, float dragSpeed);

    // This edits a three component vector
    // It writes X Y and Z to JSON
    void RenderVector3DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey,
        const std::string& zKey, float dragSpeed);

    // This edits a four component quaternion
    // It writes X Y Z and W to JSON
    void RenderQuaternionRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey,
        const std::string& zKey, const std::string& wKey, float dragSpeed);

    // This edits a color value
    // It writes rgba to the JSON data
    void RenderColorProperty(const std::string& label, nlohmann::json& colorData);

    // This edits a text value
    // It writes it to the JSON key
    void RenderTextProperty(const std::string& label, nlohmann::json& data, const std::string& key);

    // This edits an integer value
    // It writes it to the JSON key
    void RenderIntProperty(const std::string& label, nlohmann::json& data, const std::string& key);

    // This edits a single checkbox
    // It writes the boolean to JSON
    void RenderCheckboxProperty(const std::string& label, nlohmann::json& data, const std::string& key);

    // This shows a checkbox and returns if changed
    // It lets caller handle JSON write
    bool RenderCheckboxPropertyReturn(const std::string& label, bool& value);

    // This shows two checkboxes in one row
    // It writes both values to JSON
    void RenderCheckboxRow(const std::string& label, nlohmann::json& data,
        const std::string& key1, const std::string& label1,
        const std::string& key2, const std::string& label2);

    // This returns current label offset
    // It helps align content neatly
    float GetCurrentLabelOffset();
    // This returns the X position where content starts
    // It keeps layout consistent
    float GetContentStartX();

    // This returns the content width constant
    // It helps set up scrolling area
    constexpr float GetContentWidth() { return 700.0f; }

} // namespace EditorUI

#endif // EDITORUIHELPERS_H