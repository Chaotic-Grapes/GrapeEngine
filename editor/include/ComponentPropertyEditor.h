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
#include "ecs/Entity.h"
#include "ecs/World.h"

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

    // Renders entity name component for identifying entities in hierarchy
    void RenderName(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders active/enabled state toggle for entity activation control
    void RenderActive(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders tag mask bitfield for entity tagging and filtering systems
    void RenderTagMask(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders transform properties including position, rotation and scale values
    void RenderLocalTransform(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders sprite properties including material reference and color tint values
    void RenderSpriteRenderer2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders physics body properties including mass, drag and gravity settings
    void RenderRigidbody2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders linear velocity vector for 2D movement with X and Y components
    void RenderLinearVelocity2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders angular velocity for rotation around Z-axis in radians per second
    void RenderAngularVelocity2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders circle collider properties including radius and center offset values
    void RenderCircleCollider2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders box collider properties including width, height and offset values
    void RenderBoxCollider2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders circle shape properties for visual representation including radius
    void RenderShapeCircle2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders box shape properties for visual representation including dimensions
    void RenderShapeBox2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders line shape properties including start and end point coordinates
    void RenderShapeLine2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders camera properties including projection type and near/far clipping planes
    void RenderCamera3D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders acceleration vector for forces applied to 2D physics bodies
    void RenderAcceleration2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders physics material properties including friction and restitution
    void RenderPhysicsMaterial2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders sprite sheet animation properties including frame data and playback settings
    void RenderSpriteSheetAnimation2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders Z-index for controlling 2D rendering order
    void RenderZIndex2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders 2D light component for lighting effects
    void RenderLight2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders text component for displaying text on screen
    void RenderText(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders animation state for sprite sheet animation playback
    void RenderAnimationState2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders AudioSource for component 
    void RenderAudioSource(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders SceneButton component for scene transitions
    void RenderSceneButton(nlohmann::json& data, ECS::Entity entity, ECS::World* world);
    
    // Renders entity layer
    void RenderLayer2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Generic renderer for unknown (C#) components. Uses JSON field types to
    // render simple editors for booleans, integers, floats and strings.
    void RenderGenericComponent(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders material component for assigning materials to renderers
	void RenderMaterial2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    void RenderGUICanvas(nlohmann::json& data, ECS::Entity entity, ECS::World* world);
    void RenderGUIElement(nlohmann::json& data, ECS::Entity entity, ECS::World* world);
    void RenderGUIPanel(nlohmann::json& data, ECS::Entity entity, ECS::World* world);
    void RenderGUIText(nlohmann::json& data, ECS::Entity entity, ECS::World* world);
    void RenderGUIImage(nlohmann::json& data, ECS::Entity entity, ECS::World* world);
    void RenderGUIInput(nlohmann::json& data, ECS::Entity entity, ECS::World* world);
    void RenderGUIStateStyle(nlohmann::json& data, ECS::Entity entity, ECS::World* world);
    void RenderGUIButton(nlohmann::json& data, ECS::Entity entity, ECS::World* world);
    void RenderGUISlider(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders drag/drop validation feedback popups triggered by asset fields
    void RenderAssetDropFeedbackPopup();

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
