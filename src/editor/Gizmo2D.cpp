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
#include "ecs/Hierarchy.h"
#include "core/Logger.h"
#include <imgui.h>
#include <ImGuizmo.h>
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

    // Resolve raw ID
    ECS::Entity entity = world.Resolve(m_selectedEntityID);

    // Skip if entity doesn't exist or was destroyed
    if (entity.IsNull() || !world.IsAlive(entity))
        return;

    // Check if entity has transform
    if (!world.Has<ECS::Components::LocalTransform>(entity))
        return;

    auto& local = world.Get<ECS::Components::LocalTransform>(entity);

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
    

std::vector<Entity> hierarchy;
Entity current = world.ParentOf(entity);  // Start from parent, not self
while (!current.IsNull())
{
    hierarchy.push_back(current);
    current = world.ParentOf(current);
}

// Build parent world matrix from root down to immediate parent
glm::mat4 worldMatrix(1.0f);
for (auto it = hierarchy.rbegin(); it != hierarchy.rend(); ++it)
{
    if (world.Has<Components::LocalTransform>(*it))
    {
        const auto& lt = world.Get<Components::LocalTransform>(*it);
        glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(lt.Position.X, lt.Position.Y, lt.Position.Z));
        glm::mat4 R = glm::mat4_cast(glm::quat(lt.Rotation.W, lt.Rotation.X, lt.Rotation.Y, lt.Rotation.Z));
        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(lt.Scale.X, lt.Scale.Y, lt.Scale.Z));
        worldMatrix = worldMatrix * T * R * S;
    }
}

// Now apply the selected entity's local transform on top
ECS::Components::LocalTransform& t = world.Get<ECS::Components::LocalTransform>(entity);
glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(t.Position.X, t.Position.Y, t.Position.Z));
glm::mat4 R = glm::mat4_cast(glm::quat(t.Rotation.W, t.Rotation.X, t.Rotation.Y, t.Rotation.Z));
glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(t.Scale.X, t.Scale.Y, t.Scale.Z));
worldMatrix = worldMatrix * T * R * S;

    
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
        glm::value_ptr(worldMatrix)
    );

    // ------------------------------------------------------------------------
    // G. APPLY MODIFIED TRANSFORM BACK TO ECS COMPONENT
    // ------------------------------------------------------------------------

    if (ImGuizmo::IsUsing())
    {
        LOG_DEBUG("GIZMO IS USING: Applying changes!");

        // Build parent world matrix (identity if no parent)
        glm::mat4 parentWorld(1.0f);
        Entity parent = world.ParentOf(entity);
        if (!parent.IsNull())
        {
            // Collect parent hierarchy
            std::vector<Entity> parentHierarchy;
            Entity p = parent;
            while (!p.IsNull())
            {
                parentHierarchy.push_back(p);
                p = world.ParentOf(p);
            }

            // Apply from root down to immediate parent
            for (auto it = parentHierarchy.rbegin(); it != parentHierarchy.rend(); ++it)
            {
                if (world.Has<Components::LocalTransform>(*it))
                {
                    const auto& lt = world.Get<Components::LocalTransform>(*it);
                    glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(lt.Position.X, lt.Position.Y, lt.Position.Z));  
                    glm::mat4 R = glm::mat4_cast(glm::quat(lt.Rotation.W, lt.Rotation.X, lt.Rotation.Y, lt.Rotation.Z));
                    glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(lt.Scale.X, lt.Scale.Y, lt.Scale.Z));  
                    parentWorld = parentWorld * T * R * S;
                }
            }
        }

        // Convert modified world matrix back to local space
        glm::mat4 localMatrix = glm::inverse(parentWorld) * worldMatrix;

        glm::vec3 pos, scale, skew;
        glm::vec4 perspective;
        glm::quat rot;

        if (glm::decompose(localMatrix, scale, rot, pos, skew, perspective))
        {
            // Write back to LocalTransform
            local.Position = { pos.x, pos.y, pos.z };
            local.Scale = { scale.x, scale.y, scale.z };
            local.Rotation = { rot.x, rot.y, rot.z, rot.w };

            // Mark this entity + all children dirty using only existing ForChildren
            std::function<void(Entity)> markDirty = [&](Entity e)
                {
                    if (world.Has<Components::WorldTransform>(e))
                        world.Get<Components::WorldTransform>(e).Dirty = true;

                    world.ForChildren(e, markDirty);
                };
            markDirty(entity);
        }
        
    }
}