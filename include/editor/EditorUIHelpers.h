/* Start Header *****************************************************************/
/*!
\file   EditorUIHelpers.h
\author Foo Rui Qin
\date   2nd November 2025
\brief
Provides reusable, stateless ImGui helper functions for editing JSON-backed
properties in the editor. These helpers are shared between AssetBrowser and
future Inspector implementations.

Features:
- Render float, int, text, color, checkbox rows
- Render 2D vector rows with labeled X/Y fields
- Render read-only labeled text rows
*/
/* End Header *******************************************************************/

#ifndef EDITOR_UI_HELPERS_H
#define EDITOR_UI_HELPERS_H

#include <string>
#include <nlohmann/json.hpp>

namespace EditorUI {

// Render a property with X and Y fields (Position, Scale, Velocity, Size, etc.)
void RenderVector2DRow(const std::string& label, nlohmann::json& data,
    const std::string& xKey, const std::string& yKey, float dragSpeed = 1.0f,
    float labelOffset = 20.0f);

// Render a single float property with custom field label (Mass, Rotation, Volume, etc.)
void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
    nlohmann::json& data, const std::string& key, float dragSpeed = 1.0f,
    float labelOffset = 20.0f);

// Render a text input property (Name, Tag, TexturePath, etc.)
void RenderTextProperty(const std::string& label, nlohmann::json& data,
    const std::string& key, float labelOffset = 20.0f);

// Render an integer drag property (SortingOrder, MaxParticles, FontSize, etc.)
void RenderIntProperty(const std::string& label, nlohmann::json& data,
    const std::string& key, float labelOffset = 20.0f);

// Render a color picker property (works for any RGBA color in JSON)
void RenderColorProperty(const std::string& label, nlohmann::json& colorData,
    float labelOffset = 20.0f);

// Render read-only text with label (for displaying non-editable info)
void RenderReadOnlyText(const std::string& label, const std::string& value,
    float labelOffset = 10.0f);

// Render two checkboxes on same row (FlipX/FlipY, Loop/PlayOnAwake, etc.)
void RenderCheckboxRow(const std::string& label, nlohmann::json& data,
    const std::string& key1, const std::string& label1, const std::string& key2,
    const std::string& label2, float labelOffset = 30.0f);

} // namespace EditorUI

#endif // EDITOR_UI_HELPERS_H