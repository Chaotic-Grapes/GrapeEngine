/* Start Header *****************************************************************/
/*!
\file   ComponentWidgets.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th March 2026

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
#include <functional>
#include <cstdint>
#include <unordered_set>
#include "ecs/Entity.h"

struct ImFont;

// Provides stateless widgets used across all component inspectors
namespace EditorUI {
    // -------------------------------------------------------------------------
    // Filter + Defaults
    // -------------------------------------------------------------------------

    // Sets a case-insensitive filter used to hide non-matching property rows
    void SetPropertyFilter(const std::string& filter);

    // Clears the active property filter so all rows render
    void ClearPropertyFilter();

    // Returns true if the current property filter allows this label to render
    bool PropertyFilterAllows(const std::string& label);

    // Sets the symbols font used for icon-only buttons in property rows
    void SetSymbolsFont(ImFont* symbolsFont);

    // Registers a default-data mapping for the current component render
    void RegisterDefaultDataScope(nlohmann::json& data, const nlohmann::json& defaults);

    // Clears the active default-data mapping after a component finishes rendering
    void ClearDefaultDataScope();

    // Sets the list of selected entities for multi-select editing
    void SetSelectedEntities(const std::unordered_set<EntityId>* entities);

    // -------------------------------------------------------------------------
    // Section Management
    // -------------------------------------------------------------------------

    // Begins a property section with aligned labels and consistent spacing
    void BeginPropertySection(const std::vector<std::string>& labels);

    // Ends the property section and restores default ImGui layout state
    void EndPropertySection(bool addSpacing = true);

    // -------------------------------------------------------------------------
    // Basic Value Rendering
    // -------------------------------------------------------------------------

    // Displays a read-only value row that cannot be edited by user
    void RenderStaticValueRow(const std::string& label, const std::string& value, bool grayed = false);

    // Renders a float property editor with drag control and label
    void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
        nlohmann::json& data, const std::string& key, float dragSpeed, float min = 0.0f, float max = 0.0f);

    // Overload with undo context
    void RenderFloatRow(const std::string& label, const std::string& fieldLabel,
        nlohmann::json& data, const std::string& key, float dragSpeed, float min, float max,
        void* undoSystemPtr,            // Editor::UndoSystem*
        void* worldPtr,                 // ECS::World*
        uint32_t entityId,              // ECS::Entity id
        uint32_t componentTypeId,       // ECS::ComponentTypeId
        const std::string& propertyPath,
        const std::function<void(void* /*world*/, uint32_t /*entityId*/, uint32_t /*componentId*/, 
            const std::string& /*path*/, const nlohmann::json& /*value*/)>& applyFn);

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

    // Overload with undo context (2D)
    void RenderVector2DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, float dragSpeed,
        void* undoSystemPtr,            // Editor::UndoSystem*
        void* worldPtr,                 // ECS::World*
        uint32_t entityId,              // ECS::Entity id
        uint32_t componentTypeId,       // ECS::ComponentTypeId
        const std::string& basePath,    // e.g. "Offset" -> paths "Offset.X", "Offset.Y"
        const std::function<void(void* /*world*/, uint32_t /*entityId*/, uint32_t /*componentId*/, 
            const std::string& /*path*/, const nlohmann::json& /*value*/)>& applyFn);

    // Overload with undo context (3D)
    void RenderVector3DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, const std::string& zKey,
        float dragSpeed,
        void* undoSystemPtr,            // Editor::UndoSystem*
        void* worldPtr,                 // ECS::World*
        uint32_t entityId,              // ECS::Entity id
        uint32_t componentTypeId,       // ECS::ComponentTypeId
        const std::string& basePath,    // e.g. "Position" -> "Position.X", etc.
        const std::function<void(void* /*world*/, uint32_t /*entityId*/, uint32_t /*componentId*/, 
            const std::string& /*path*/, const nlohmann::json& /*value*/)>& applyFn);

    // Renders a 4D vector editor with X, Y, Z and W components as separate drag controls
    void RenderVector4DRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, const std::string& zKey,
        const std::string& wKey, float dragSpeed);

    // Renders a quaternion editor with X, Y, Z and W components for 3D rotation values
    void RenderQuaternionRow(const std::string& label, nlohmann::json& data,
        const std::string& xKey, const std::string& yKey, const std::string& zKey,
        const std::string& wKey, float dragSpeed);

    // -------------------------------------------------------------------------
    // Property Editing
    // -------------------------------------------------------------------------

    // Renders a color picker that edits RGBA values in JSON color data
    void RenderColorProperty(const std::string& label, nlohmann::json& colorData);

    // Renders a color picker aligned as a property row
    void RenderColorRow(const std::string& label, nlohmann::json& colorData);

    // Renders a text input field that edits string values in JSON data
    void RenderTextProperty(const std::string& label, nlohmann::json& data,
        const std::string& key);

    // Overload with undo context
    void RenderTextProperty(const std::string& label, nlohmann::json& data, const std::string& key,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId, const std::string& propertyPath,
        const std::function<void(void* /*world*/, uint32_t /*entityId*/, uint32_t /*componentId*/, 
            const std::string& /*path*/, const nlohmann::json& /*value*/)>& applyFn);

    // Renders an integer input field with drag control for numeric values
    void RenderIntProperty(const std::string& label, nlohmann::json& data, const std::string& key);

    // Overload with undo context
    void RenderIntProperty(const std::string& label, nlohmann::json& data, const std::string& key,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId, const std::string& propertyPath,
        const std::function<void(void* /*world*/, uint32_t /*entityId*/, uint32_t /*componentId*/, 
            const std::string& /*path*/, const nlohmann::json& /*value*/)>& applyFn);

    // Renders a dropdown to edit a bitmask (up to 32 bits) with labeled entries
    void RenderBitmaskDropdown(const std::string& label, nlohmann::json& data, const std::string& key, 
        const std::vector<std::string>& bitNames, uint32_t defaultMask = 0xFFFFFFFFu);

    // Overload with undo context
    void RenderBitmaskDropdown(const std::string& label, nlohmann::json& data, const std::string& key, 
        const std::vector<std::string>& bitNames, uint32_t defaultMask, void* undoSystemPtr, 
        void* worldPtr, uint32_t entityId, uint32_t componentTypeId, const std::string& propertyPath,
        const std::function<void(void* /*world*/, uint32_t /*entityId*/, uint32_t /*componentId*/, 
            const std::string& /*path*/, const nlohmann::json& /*value*/)>& applyFn);

    // Renders a checkbox that toggles boolean values in JSON data
    void RenderCheckboxProperty(const std::string& label, nlohmann::json& data, const std::string& key);

    // Overload with undo context
    void RenderCheckboxProperty(const std::string& label, nlohmann::json& data,
        const std::string& key,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& propertyPath,
        const std::function<void(void* /*world*/, uint32_t /*entityId*/, uint32_t /*componentId*/, 
            const std::string& /*path*/, const nlohmann::json& /*value*/)>& applyFn);

    // Overload with undo context
    void RenderColorProperty(const std::string& label, nlohmann::json& colorData,
        void* undoSystemPtr, void* worldPtr, uint32_t entityId, uint32_t componentTypeId,
        const std::string& basePath,  // expects keys "R","G","B","A"
        const std::function<void(void* /*world*/, uint32_t /*entityId*/, uint32_t /*componentId*/, 
            const std::string& /*path*/, const nlohmann::json& /*value*/)>& applyFn);

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

    // Returns content width for the current inspector panel layout
    float GetContentWidth();

}

#endif