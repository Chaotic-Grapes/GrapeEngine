/* Start Header *****************************************************************/
/*!
\file   ComponentPropertyEditor.h
\author Foo Rui Qin (90%)
        Samantha Leong (10%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   12th March 2026
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
#include <unordered_set>
#include "ecs/Entity.h"
#include "ecs/World.h"
namespace Editor { class UndoSystem; }

// Handles rendering of all component UIs in the inspector
class ComponentUI {
public:
    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize fonts used by inspector component widgets.
     * @param mainFont Regular UI font.
     * @param boldFont Emphasis font used for headings.
     * @param symbolsFont Icon/symbol font.
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    /**
     * @brief Set undo system used to record component edits.
     * @param undo Undo system instance.
     */
    void SetUndoSystem(Editor::UndoSystem* undo) { m_undo = undo; }

    /**
     * @brief Set selected entity set used for multi-edit operations.
     * @param entities Pointer to selected entity id set.
     */
    void SetSelectedEntities(const std::unordered_set<EntityId>* entities);

    // -------------------------------------------------------------------------
    // Component Rendering
    // -------------------------------------------------------------------------

    /**
     * @brief Render editor controls for Name component data.
     * @param data JSON component payload.
     * @param entity Target entity.
     * @param world World containing the entity.
     */
    void RenderName(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    /**
     * @brief Render editor controls for Active component data.
     * @param data JSON component payload.
     * @param entity Target entity.
     * @param world World containing the entity.
     */
    void RenderActive(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    /**
     * @brief Render editor controls for TagMask component data.
     * @param data JSON component payload.
     * @param entity Target entity.
     * @param world World containing the entity.
     */
    void RenderTagMask(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    /**
     * @brief Render editor controls for LocalTransform component data.
     * @param data JSON component payload.
     * @param entity Target entity.
     * @param world World containing the entity.
     */
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

    // Renders 2D light properties including color, intensity and radius
    void RenderLight2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders text component properties including string content and font settings
    void RenderText(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders animation state properties for sprite sheet playback control
    void RenderAnimationState2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders audio source properties including clip reference and playback settings
    void RenderAudioSource(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders layer index for controlling entity collision and rendering layer assignment
    void RenderLayer2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    /**
     * @brief Render editor controls for TileMap component data.
     * @param data JSON component payload.
     * @param entity Target entity.
     * @param world World containing the entity.
     */
    void RenderTileMapComponent(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    /**
     * @brief Render generic JSON fields for unknown or managed component types.
     * @param data JSON component payload.
     * @param entity Target entity.
     * @param world World containing the entity.
     * @param addSpacing True to append standard inspector spacing after render.
     */
    void RenderGenericComponent(nlohmann::json& data, ECS::Entity entity, ECS::World* world, bool addSpacing = true);

    // Renders material component for assigning materials to renderers
    void RenderMaterial2D(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders boid flock properties including separation, alignment and cohesion weights
    void RenderBoidFlock(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders particle emitter properties including spawn rate, lifetime and velocity range
    void RenderParticleEmitter(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Apply a preset configuration to the given JSON data by preset ID
    void _ApplyPresetToJson(nlohmann::json& data, int presetId);

    // Renders GUI canvas root properties including render mode and sort order
    void RenderGUICanvas(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders GUI render mode selection for screen-space or world-space canvas
    void RenderGUIRenderMode(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders base GUI element properties including anchors, pivot and rect offset
    void RenderGUIElement(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders GUI panel properties including background color and image reference
    void RenderGUIPanel(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders GUI text properties including string content, font and alignment
    void RenderGUIText(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders GUI image properties including sprite reference and color tint
    void RenderGUIImage(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders GUI input field properties including placeholder text and input type
    void RenderGUIInput(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders GUI state style properties for normal, hovered and pressed visual states
    void RenderGUIStateStyle(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders GUI button properties including click callbacks and transition style
    void RenderGUIButton(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    // Renders GUI slider properties including min, max and current value
    void RenderGUISlider(nlohmann::json& data, ECS::Entity entity, ECS::World* world);

    /**
     * @brief Render popups used for asset drag/drop validation feedback.
     */
    void RenderAssetDropFeedbackPopup();

private:
    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // Fonts used for all UI text and icons
    ImFont* m_mainFont = nullptr;                                       
    ImFont* m_boldFont = nullptr;                                       
    ImFont* m_symbolsFont = nullptr;                

    // System references
    Editor::UndoSystem* m_undo = nullptr;                               // Undo system for recording edits
    const std::unordered_set<EntityId>* m_selectedEntities = nullptr;   // Currently selected entities for multi-edit
};

#endif