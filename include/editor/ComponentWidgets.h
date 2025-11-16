/* Start Header *****************************************************************/
/*!
\file   ComponentWidgets.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   2nd November 2025

\brief
Reusable ImGui helper functions for editing JSON-backed properties.

Provides consistent UI widgets for editing component properties stored as JSON data.
All functions work directly with nlohmann::json objects for unified data handling.
Used by component inspectors for both entity and prefab editing workflows.
*/
/* End Header *******************************************************************/

#ifndef COMPONENT_WIDGETS_H
#define COMPONENT_WIDGETS_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// Provides stateless widgets used across all component inspectors
namespace EditorUI {

    // -------------------------------------------------------------------------
    // Section Management
    // -------------------------------------------------------------------------
    
    // Begins a property section with aligned labels and consistent spacing
    void BeginPropertySection(const std::vector<std::string>& labels);

    // Ends the property section and restores default ImGui layout state
    void EndPropertySection();

    // -------------------------------------------------------------------------
    // Basic Value Rendering
    // -------------------------------------------------------------------------
   
    // Displays a read-only value row that cannot be edited by user
    void RenderStaticValueRow(const std::string& label, const std::string& value);

    // Renders a float property editor with drag control and label
    void RenderFloatRow(const std::string& label, const std::string& fieldLabel, 
        nlohmann::json& data, const std::string& key, float dragSpeed);

    // -------------------------------------------------------------------------
    // Vector Editing
    // -------------------------------------------------------------------------
    
    // Renders a 2D vector editor with X and Y components as separate drag controls
    void RenderVector2DRow(const std::string& label, nlohmann::json& data, 
        const std::string& xKey, const std::string& yKey, float dragSpeed);

    // Renders a 3D vector editor with X, Y and Z components as separate drag controls
    void RenderVector3DRow(const std::string& label, nlohmann::json& data, 
        const std::string& xKey, const std::string& yKey, const std::string& zKey, 
        float dragSpeed);

    // Renders a quaternion editor with X, Y, Z and W components
    // Used for rotation values in 3D space
    void RenderQuaternionRow(const std::string& label, nlohmann::json& data, 
        const std::string& xKey, const std::string& yKey, const std::string& zKey, 
        const std::string& wKey, float dragSpeed);

    // -------------------------------------------------------------------------
    // Property Editing
    // -------------------------------------------------------------------------
    
    // Renders a color picker that edits RGBA values in JSON color data
    void RenderColorProperty(const std::string& label, nlohmann::json& colorData);

    // Renders a text input field that edits string values in JSON data
    void RenderTextProperty(const std::string& label, nlohmann::json& data, 
        const std::string& key);

    // Renders an integer input field with drag control for numeric values
    void RenderIntProperty(const std::string& label, nlohmann::json& data, 
        const std::string& key);

    // Renders a checkbox that toggles boolean values in JSON data
    void RenderCheckboxProperty(const std::string& label, nlohmann::json& data, 
        const std::string& key);

    // Renders a checkbox and returns whether value changed, letting caller handle JSON update
    bool RenderCheckboxPropertyReturn(const std::string& label, bool& value);

    // Renders two checkboxes in a single row for compact boolean editing
    void RenderCheckboxRow(const std::string& label, nlohmann::json& data, 
        const std::string& key1, const std::string& label1, const std::string& key2, 
        const std::string& label2);

    // -------------------------------------------------------------------------
    // Layout Helpers
    // -------------------------------------------------------------------------
    
    // Returns X position where property content starts after labels
    float GetContentStartX();

    // Returns fixed content width used for consistent inspector panel layout
    constexpr float GetContentWidth() { return 730.0f; }

} 

#endif