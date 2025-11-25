/**
 * @Name: Samantha Leong, 2403088
 * @email: s.leong@digipen.edu
 * @file Gizmo2D.cpp
 * @brief Implements editor gizmo rendering and manipulation for 2D/3D entities.
 *
 * This file uses ImGuizmo to render translation/rotation/scale gizmos inside
 * the editor viewport. The gizmo allows real-time manipulation of entity
 * transforms in ECS::World. The system converts LocalTransform components
 * into matrices, sends them to ImGuizmo, and applies back any modifications.
 */

#define GLM_ENABLE_EXPERIMENTAL


#include "../engine/ecs/systems/RendererSystem.h"
#include "core/Logger.h"
#include <imgui.h>
#include "ImGuizmo.h"
#include "ecs/World.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp> 
// Ensure your project includes these GLM extensions for matrix decomposition

using namespace ECS;

/**
 * @brief Draws and applies the editor gizmo (translate/rotate/scale) for the currently selected entity.
 *
 * This function:
 *  - Validates whether a selected entity exists and has a LocalTransform.
 *  - Sets up the ImGuizmo context (draw list, rect, orthographic mode).
 *  - Handles gizmo tool switching via W/E/R keys.
 *  - Builds a TRS model matrix from the entity's LocalTransform.
 *  - Executes ImGuizmo::Manipulate using the editor camera matrices.
 *  - Decomposes the modified model matrix back into T/R/S and writes it back.
 *
 * @param world Reference to the ECS world.
 * @param drawPosX Viewport draw X position.
 * @param drawPosY Viewport draw Y position.
 * @param drawSizeX Viewport width.
 * @param drawSizeY Viewport height.
 */
void RendererSystem::DrawEditorGizmo(ECS::World& world, float drawPosX, float drawPosY, float drawSizeX, float drawSizeY)
{
    // ------------------------------------------------------------------------
    // A. VALIDATION
    // ------------------------------------------------------------------------

    // Pre-check: Stop if the current selected entity is invalid
    if (m_selectedEntityID == Entity::NPOS32)
        return;

    ECS::Entity entity = world.Resolve(m_selectedEntityID);

    // Skip if entity doesn't exist or was destroyed
    if (entity.IsNull() || !world.IsAlive(entity))
        return;

    // Check if entity has transform
    if (!world.Has<ECS::Components::LocalTransform>(entity))
        return;

    // ------------------------------------------------------------------------
    // B. SETUP IMGUIZMO CONTEXT
    // ------------------------------------------------------------------------

    ImGuizmo::BeginFrame();

    // Set orthographic mode for 2D.
    ImGuizmo::SetOrthographic(true);

    // Tell ImGuizmo to draw on the current window.
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

    // Set the screen area (the viewport image bounds).
    ImGuizmo::SetRect(drawPosX, drawPosY, drawSizeX, drawSizeY);

    // ------------------------------------------------------------------------
    // C. HANDLE TOOL SWITCH INPUT (T = Move, E = Rotate, R = Scale)
    // ------------------------------------------------------------------------

    // Updates the private member m_currentGizmoOperation
    if (ImGui::IsWindowFocused() && !ImGuizmo::IsUsing())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_T)) { m_currentGizmoOperation = ImGuizmo::TRANSLATE; }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) { m_currentGizmoOperation = ImGuizmo::ROTATE; }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) { m_currentGizmoOperation = ImGuizmo::SCALE; }
    }

    // ------------------------------------------------------------------------
    // D. BUILD MODEL MATRIX FROM ENTITY TRANSFORM (T * R * S)
    // ------------------------------------------------------------------------

    ECS::Components::LocalTransform& t = world.Get<ECS::Components::LocalTransform>(entity);
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // Build the matrix using the entity's current position, rotation, and scale.
    //modelMatrix = glm::translate(modelMatrix, glm::vec3(t.Position.X, t.Position.Y, 0.0f));
    //modelMatrix = glm::rotate(modelMatrix, glm::radians(t.Rotation), glm::vec3(0, 0, 1)); // Z-axis rotation for 2D
    //modelMatrix = glm::scale(modelMatrix, glm::vec3(t.Scale.X, t.Scale.Y, 1.0f));

    // 1. Translate (T)
    modelMatrix = glm::translate(modelMatrix, glm::vec3(t.Position.X, t.Position.Y, t.Position.Z)); 
    

    // 2. Rotate (R) - Apply the existing quaternion rotation
    //    glm::mat4_cast converts the quaternion directly to a rotation matrix
    glm::quat glmQuat(t.Rotation.W, t.Rotation.X, t.Rotation.Y, t.Rotation.Z);
    modelMatrix *= glm::mat4_cast(glmQuat);

    // 3. Scale (S)
    modelMatrix = glm::scale(modelMatrix, glm::vec3(t.Scale.X, t.Scale.Y, t.Scale.Z));

    // ------------------------------------------------------------------------
    // E. BUILD CAMERA MATRICES REQUIRED BY IMGUIZMO
    // ------------------------------------------------------------------------

    if (!m_editorCamera) {
        return; //< Cannot manipulate without a camera
    }

    // Get view and projection from EditorCamera
    glm::mat4 viewMatrix = m_editorCamera->GetViewMatrix();
    glm::mat4 projectionMatrix = m_editorCamera->GetProjectionMatrix();

    // ------------------------------------------------------------------------
    // F. EXECUTE GIZMO MANIPULATION
    // ------------------------------------------------------------------------

    // This call draws the gizmo and modifies modelMatrix based on mouse input.
    ImGuizmo::Manipulate(
        glm::value_ptr(viewMatrix),
        glm::value_ptr(projectionMatrix),
        m_currentGizmoOperation,        //< translate / rotate / scale
        m_currentGizmoMode,            //< local / world
        glm::value_ptr(modelMatrix)
    );

    // ------------------------------------------------------------------------
    // G. APPLY MODIFIED TRANSFORM BACK TO ECS COMPONENT
    // ------------------------------------------------------------------------

    if (ImGuizmo::IsUsing())
    {
        LOG_DEBUG("GIZMO IS USING: Applying changes!");

        glm::vec3 scale, translation, skew;
        glm::quat rotation;
        glm::vec4 perspective;

        // Decompose the modified matrix back into T, R, S components
        if (glm::decompose(modelMatrix, scale, rotation, translation, skew, perspective))
        {
            //glm::vec3 euler = glm::eulerAngles(rotation);
            //float newRotation = glm::degrees(euler.z); // Extract Z-axis rotation

            // Update the entity's Local Transform component
            t.Position.X = translation.x;
            t.Position.Y = translation.y;
            t.Position.Z = translation.z;

            // Update the entity's Local Scale component
            t.Scale.X = scale.x;
            t.Scale.Y = scale.y;
            t.Scale.Z = scale.z;

            // Update the entity's Local Rotation component
            t.Rotation.X = rotation.x;
            t.Rotation.Y = rotation.y;
            t.Rotation.Z = rotation.z;
            t.Rotation.W = rotation.w;
            
        }
    }
}