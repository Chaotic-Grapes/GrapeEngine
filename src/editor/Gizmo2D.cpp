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

// In Gizmo2D.cpp - use engine math (Matrix4x4 / TransformUtils) instead of glm
#include "ecs/systems/RendererSystem.h"
#include "core/Logger.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include "ecs/World.h"
#include "helpers/TransformUtils.h"
#include "math/Matrix4x4.h"
#include "math/Quaternion.h"
#include "math/Vector3D.h"

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
    // D. BUILD MODEL MATRIX FROM ENTITY WORLD TRANSFORM (T * R * S)
    // Use WorldTransform when available so gizmo is placed in world space
    // (this fixes incorrect offsets for child entities).
    // ------------------------------------------------------------------------

    // Build model matrix as engine Matrix4x4
    Matrix4x4 modelMatrix = Matrix4x4::Identity();

    bool usedWorld = false;
    if (world.Has<ECS::Components::WorldTransform>(entity)) {
        const auto& wt = world.Get<ECS::Components::WorldTransform>(entity);
        modelMatrix = wt.Matrix;
        usedWorld = true;
    }
    else {
        // Fallback to LocalTransform if WorldTransform not present
        const auto& t = world.Get<ECS::Components::LocalTransform>(entity);
        modelMatrix = TransformUtils::MakeTRS(t.Position, t.Rotation, t.Scale);
    }

    // ------------------------------------------------------------------------
    // E. BUILD CAMERA MATRICES REQUIRED BY IMGUIZMO
    // ------------------------------------------------------------------------

    if (!m_editorCamera) {
        return; //< Cannot manipulate without a camera
    }

    // Build view and projection using engine math so we avoid glm here.
    // Use the editor camera's transform and camera component to compute matrices.
    Matrix4x4 viewMat = Matrix4x4::Identity();
    Matrix4x4 projMat = Matrix4x4::Identity();

    if (auto camTransform = m_editorCamera->GetTransform()) {
        // Camera eye position
        Vector3D eye{ camTransform->Position.X, camTransform->Position.Y, camTransform->Position.Z };

        // Use GLM-like LookAt construction to match EditorCamera::GetViewMatrix()
        Vector3D up{ 0.0f, 1.0f, 0.0f };

        // Derive forward from stored rotation (canonical OpenGL forward is -Z)
        Vector3D forward = camTransform->Rotation.Rotate(Vector3D{ 0.0f, 0.0f, -1.0f });
        Vector3D target = eye + forward;

        Vector3D f = (target - eye).Normalized();
        Vector3D s = Vector3D::Cross(f, up).Normalized();
        Vector3D u = Vector3D::Cross(s, f);

        // Construct matrix in the same style glm::lookAt produces (column-major basis)
        // Fill row-major Matrix4x4 so MatToColMajor will yield the same column-major memory as glm
        viewMat = Matrix4x4(
            s.X, u.X, -f.X, 0.0f,
            s.Y, u.Y, -f.Y, 0.0f,
            s.Z, u.Z, -f.Z, 0.0f,
            -Vector3D::Dot(s, eye), -Vector3D::Dot(u, eye), Vector3D::Dot(f, eye), 1.0f
        );
    }

    if (auto camComp = m_editorCamera->GetCameraComponent()) {
        float aspect = camComp->AspectRatio;
        float halfSize = camComp->OrthoSize * 0.5f;
        projMat = Matrix4x4::Orthographic(-halfSize * aspect, halfSize * aspect,
            -halfSize, halfSize, camComp->NearPlane, camComp->FarPlane);
    }

    // ------------------------------------------------------------------------
    // F. EXECUTE GIZMO MANIPULATION
    // ------------------------------------------------------------------------

    // This call draws the gizmo and modifies modelMatrix based on mouse input.
    // Convert engine matrices to column-major float arrays for ImGuizmo
    float viewArr[16];
    float projArr[16];
    float modelArr[16];

    auto MatToColMajor = [](const Matrix4x4& M, float out[16]) {
        out[0] = M.m00; out[1] = M.m10; out[2] = M.m20; out[3] = M.m30;
        out[4] = M.m01; out[5] = M.m11; out[6] = M.m21; out[7] = M.m31;
        out[8] = M.m02; out[9] = M.m12; out[10] = M.m22; out[11] = M.m32;
        out[12] = M.m03; out[13] = M.m13; out[14] = M.m23; out[15] = M.m33;
    };

    MatToColMajor(viewMat, viewArr);
    MatToColMajor(projMat, projArr);
    MatToColMajor(modelMatrix, modelArr);

    ImGuizmo::Manipulate(
        viewArr,
        projArr,
        m_currentGizmoOperation,
        m_currentGizmoMode,
        modelArr
    );

    // ------------------------------------------------------------------------
    // G. APPLY MODIFIED TRANSFORM BACK TO ECS COMPONENT
    // ------------------------------------------------------------------------

    if (ImGuizmo::IsUsing())
    {
        LOG_DEBUG("GIZMO IS USING: Applying changes!");

        // modelArr now contains the modified world matrix in column-major order
        Matrix4x4 newWorldMat;
        // Convert column-major array back into engine Matrix4x4 (row-major storage)
        auto ColMajorToMat = [](const float in[16]) {
            Matrix4x4 M;
            M.m00 = in[0]; M.m01 = in[4]; M.m02 = in[8];  M.m03 = in[12];
            M.m10 = in[1]; M.m11 = in[5]; M.m12 = in[9];  M.m13 = in[13];
            M.m20 = in[2]; M.m21 = in[6]; M.m22 = in[10]; M.m23 = in[14];
            M.m30 = in[3]; M.m31 = in[7]; M.m32 = in[11]; M.m33 = in[15];
            return M;
        };

        newWorldMat = ColMajorToMat(modelArr);

        // Compute local matrix = inverse(parentWorld) * newWorldMat
        Matrix4x4 parentWorld = Matrix4x4::Identity();
        ECS::Entity parent = world.ParentOf(entity);
        if (!parent.IsNull() && world.Has<ECS::Components::WorldTransform>(parent)) {
            parentWorld = world.Get<ECS::Components::WorldTransform>(parent).Matrix;
        }

        Matrix4x4 invParent = parentWorld.Inverse();
        Matrix4x4 localMatrix = invParent * newWorldMat;

        // Decompose localMatrix into T/R/S using engine TransformUtils
        Vector3D outPos, outScale;
        Quaternion outRot;
        TransformUtils::DecomposeTRS(localMatrix, outPos, outRot, outScale);

        // Update LocalTransform component
        auto& lt = world.Get<ECS::Components::LocalTransform>(entity);
        lt.Position = outPos;
        lt.Scale = outScale;
        lt.Rotation = outRot;

        // Update WorldTransform immediately so subsequent frames use the
        // freshly edited world matrix (avoids snapping when hierarchy update
        // runs later).
        if (world.Has<ECS::Components::WorldTransform>(entity)) {
            world.Get<ECS::Components::WorldTransform>(entity).Matrix = newWorldMat;
            world.Get<ECS::Components::WorldTransform>(entity).Dirty = false;
        }
        else {
            ECS::Components::WorldTransform wt{};
            wt.Matrix = newWorldMat;
            wt.Dirty = false;
            world.Set<ECS::Components::WorldTransform>(entity, wt);
        }
    }
}