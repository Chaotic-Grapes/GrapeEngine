

#define GLM_ENABLE_EXPERIMENTAL
// In Gizmo2D.cpp
#include "../engine/ecs/systems/RendererSystem.h"
#include <imgui.h>
#include "ImGuizmo.h"
#include "ecs/World.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp> 
// Ensure your project includes these GLM extensions for matrix decomposition

using namespace ECS;

void RendererSystem::DrawEditorGizmo(ECS::World& world, float drawPosX, float drawPosY, float drawSizeX, float drawSizeY)
{
    // 1. Pre-check: Stop if the current selected entity is invalid
    if (m_selectedEntityID == 0)
        return;

    ECS::Entity entity = world.Resolve(m_selectedEntityID);
    if (entity.IsNull() || !world.IsAlive(entity))
        return;

    // Check if entity has transform
    if (!world.Has<ECS::Components::LocalTransform>(entity))
        return;

    // --- A. SETUP IMGUIZMO CONTEXT ---

    // Set orthographic mode for 2D.
    ImGuizmo::SetOrthographic(true);

    // Tell ImGuizmo to draw on the current window.
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

    // Set the screen area (the viewport image bounds).
    ImGuizmo::SetRect(drawPosX, drawPosY, drawSizeX, drawSizeY);

    // --- B. HANDLE TOOL SWITCH INPUT (W, E, R) ---
    // Updates the private member m_currentGizmoOperation
    if (ImGui::IsWindowFocused() && !ImGuizmo::IsUsing())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) { m_currentGizmoOperation = ImGuizmo::TRANSLATE; }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) { m_currentGizmoOperation = ImGuizmo::ROTATE; }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) { m_currentGizmoOperation = ImGuizmo::SCALE; }
    }

    // --- C. BUILD MODEL MATRIX (Entity's World Transform) ---
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

    // --- D. BUILD CAMERA MATRICES ---
    if (!m_editorCamera) {
        return; // No camera, can't render gizmo
    }

    // Get view and projection from EditorCamera
    glm::mat4 viewMatrix = m_editorCamera->GetViewMatrix();
    glm::mat4 projectionMatrix = m_editorCamera->GetProjectionMatrix();

    // --- E. EXECUTE MANIPULATION ---
    // This call draws the gizmo and modifies modelMatrix based on mouse input.
    ImGuizmo::Manipulate(
        glm::value_ptr(viewMatrix),
        glm::value_ptr(projectionMatrix),
        m_currentGizmoOperation,
        m_currentGizmoMode,
        glm::value_ptr(modelMatrix)
    );

    // --- F. DECOMPOSE AND APPLY CHANGES ---
    if (ImGuizmo::IsUsing())
    {
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

            t.Scale.X = scale.x;
            t.Scale.Y = scale.y;
            t.Scale.Z = scale.z;

            t.Rotation.X = rotation.x;
            t.Rotation.Y = rotation.y;
            t.Rotation.Z = rotation.z;
            t.Rotation.W = rotation.w;
            
        }
    }
}