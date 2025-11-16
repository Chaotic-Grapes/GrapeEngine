/* Start Header *****************************************************************/
/*!
\file   ComponentPropertyEditor.h
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

#ifndef COMPONENT_PROPERTY_EDITOR_H
#define COMPONENT_PROPERTY_EDITOR_H

#include <nlohmann/json.hpp>
#include <imgui.h>

// Handles rendering of all component UIs in the inspector
class ComponentUI {
public:
    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    // Sets fonts for component UI rendering to maintain visual consistency across 
    // all inspectors
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    // -------------------------------------------------------------------------
    // Component Rendering
    // -------------------------------------------------------------------------

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

    // Renders acceleration vector for forces applied to 2D physics bodies
    void RenderAcceleration2D(nlohmann::json& data);

    // Renders physics material properties including friction and restitution
    void RenderPhysicsMaterial2D(nlohmann::json& data);

    // Renders sprite sheet animation properties including frame data and playback settings
    void RenderSpriteSheetAnimation2D(nlohmann::json& data);

    // Renders sprite flip properties for mirroring sprites horizontally or vertically
    void RenderSpriteFlip2D(nlohmann::json& data);

    // Renders Z-index for controlling 2D rendering order
    void RenderZIndex2D(nlohmann::json& data);

private:
    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // Font references for UI styling
    ImFont* m_mainFont;
    ImFont* m_boldFont;
    ImFont* m_symbolsFont;
};

#endif
