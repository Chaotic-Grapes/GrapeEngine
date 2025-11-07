/* Start Header *****************************************************************/
/*!
\file   EditorUIHelpers.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   2nd November 2025
\brief
Provides reusable, stateless ImGui helper functions for editing JSON-backed
properties in the editor. These helpers are shared between AssetBrowser and
future Inspector implementations.

Features:
- Render float, int, text, color, checkbox rows
- Render 2D vector rows with labeled X/Y fields
- Render read-only labeled text rows
- Automatic label alignment within property sections
*/
/* End Header *******************************************************************/

#ifndef EDITOR_UI_HELPERS_H
#define EDITOR_UI_HELPERS_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace EditorUI {

// Begin a property section with automatic label alignment
// Calculate max label width from provided labels for perfect alignment
void BeginPropertySection(const std::vector<std::string>& labels);

// End a property section (resets alignment state)
void EndPropertySection();

// Render a property with X and Y fields (Position, Scale, Velocity, Size, etc.)
void RenderVector2DRow(const std::string& label, nlohmann::json& data,
    const std::string& xKey, const std::string& yKey, float dragSpeed = 1.0f);

// Render a property with X, Y and Z fields (3D vectors)
void RenderVector3DRow(const std::string& label, nlohmann::json& data,
    const std::string& xKey, const std::string& yKey, const std::string& zKey, float dragSpeed = 1.0f);

// Render a quaternion with X, Y, Z, W fields
void RenderQuaternionRow(const std::string& label, nlohmann::json& data,
    const std::string& xKey, const std::string& yKey, const std::string& zKey, const std::string& wKey, float dragSpeed = 0.1f);

// Render a single float property with custom field label (Mass, Rotation, Volume, etc.)
void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
    nlohmann::json& data, const std::string& key, float dragSpeed = 1.0f);

// Render a text input property (Name, Tag, TexturePath, etc.)
void RenderTextProperty(const std::string& label, nlohmann::json& data,
    const std::string& key);

// Render an integer drag property (SortingOrder, MaxParticles, FontSize, etc.)
void RenderIntProperty(const std::string& label, nlohmann::json& data,
    const std::string& key);

// Render a color picker property (works for any RGBA color in JSON)
void RenderColorProperty(const std::string& label, nlohmann::json& colorData);

// Render read-only text with label (for displaying non-editable info)
void RenderReadOnlyText(const std::string& label, const std::string& value);

// Render two checkboxes on same row (FlipX/FlipY, Loop/PlayOnAwake, etc.)
void RenderCheckboxRow(const std::string& label, nlohmann::json& data,
    const std::string& key1, const std::string& label1, const std::string& key2,
    const std::string& label2);

// Get the current alignment offset (for custom rendering that needs to align with other properties)
float GetCurrentLabelOffset();

// Get the unified content start X (label column + axis label width + pad)
float GetContentStartX();

} // namespace EditorUI

#endif