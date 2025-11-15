/* Start Header *****************************************************************/
/*!
\file   ComponentInspectorUI.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Provides unified component rendering UI for both entity inspection and prefab editing.

This class handles the visual editing of all component types through a JSON-based 
interface. It provides rendering functions for each component type that modify JSON 
data directly. The same UI functions work for both live entities (via JSON conversion) 
and prefab files. Component data is converted to JSON for editing then applied back 
to C++ components.
*/
/* End Header *******************************************************************/

#ifndef COMPONENT_INSPECTOR_UI_H
#define COMPONENT_INSPECTOR_UI_H

#include <nlohmann/json.hpp>
#include <imgui.h>
#include "ecs/Components.h"
#include "serialization/EntitySerializer.h"  // For to_json/from_json

class ComponentUI {
public:
    // Sets fonts for component UI rendering to maintain visual consistency across all inspectors
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    // Renders transform properties including position, rotation and scale values
    void RenderLocalTransform(nlohmann::json& data);

    // Renders sprite properties including material reference and color tint values
    void RenderSpriteRenderer2D(nlohmann::json& data);

    // Renders physics body properties including mass, drag and gravity settings
    void RenderRigidbody2D(nlohmann::json& data);

    // Renders linear velocity vector for 2D movement with X and Y components
    void RenderLinearVelocity2D(nlohmann::json& data);

    // Renders angular velocity for rotation around Z-axis in radians per second
    void RenderAngularVelocity2D(nlohmann::json& data);

    // Renders circle collider properties including radius and center offset values
    void RenderCircleCollider2D(nlohmann::json& data);

    // Renders box collider properties including width, height and offset values
    void RenderBoxCollider2D(nlohmann::json& data);

    // Renders circle shape properties for visual representation including radius
    void RenderShapeCircle2D(nlohmann::json& data);

    // Renders box shape properties for visual representation including dimensions
    void RenderShapeBox2D(nlohmann::json& data);

    // Renders line shape properties including start and end point coordinates
    void RenderShapeLine2D(nlohmann::json& data);

    // Renders camera properties including projection type and near/far clipping planes
    void RenderCamera3D(nlohmann::json& data);

    // Converts C++ component to JSON format for unified editing interface
    // Uses auto-generated to_json functions from NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE macros
    template<typename T>
    nlohmann::json ComponentToJson(const T& component) {
        nlohmann::json j;
        to_json(j, component);
        return j;
    }

    // Applies JSON data back to C++ component after UI modifications are complete
    // Uses auto-generated from_json functions for bidirectional data conversion
    template<typename T>
    void JsonToComponent(const nlohmann::json& data, T& component) {
        from_json(data, component);
    }

private:
    // Font references for UI styling
    ImFont* m_mainFont;     
    ImFont* m_boldFont;       
    ImFont* m_symbolsFont; 
};

#endif