/* Start Header *****************************************************************/
/*!
\file   Gizmo2D.cpp
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   5th November 2025

\brief
Implements editor gizmo rendering and manipulation for 2D/3D entities.

This is an editor-only utility that uses ImGuizmo to render translation/rotation/
scale gizmos inside the viewport. The gizmo allows real-time manipulation of entity
transforms in ECS::World.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "EditorGizmo.h"
#include "core/Logger.h"
#include <imgui.h>
#include "ecs/World.h"
#include "helpers/TransformUtils.h"
#include "math/Matrix4x4.h"
#include "math/Quaternion.h"
#include "math/Vector3D.h"

using namespace ECS;

namespace Editor {

    // Static member initialization
    EditorGizmo::Operation EditorGizmo::s_currentOperation = EditorGizmo::Operation::Translate;
    EditorGizmo::Mode EditorGizmo::s_currentMode = EditorGizmo::Mode::Local;

    bool EditorGizmo::DrawGizmo(
        ECS::World& world,
        uint32_t selectedEntityID,
        const float* viewMatrix,
        const float* projMatrix,
        float drawPosX,
        float drawPosY,
        float drawSizeX,
        float drawSizeY,
        bool isPerspective)
    {
        // ------------------------------------------------------------------------
        // A. VALIDATION
        // ------------------------------------------------------------------------

        // Pre-check: Stop if the current selected entity is invalid
        if (selectedEntityID == Entity::NPOS32)
            return false;

        ECS::Entity entity = world.Resolve(selectedEntityID);

        // Skip if entity doesn't exist or was destroyed
        if (entity.IsNull() || !world.IsAlive(entity))
            return false;

        // Check if entity has transform
        if (!world.Has<ECS::Components::LocalTransform>(entity))
            return false;

        // ------------------------------------------------------------------------
        // B. SETUP IMGUIZMO CONTEXT
        // ------------------------------------------------------------------------

        ImGuizmo::BeginFrame();

        // Set orthographic/perspective mode
        ImGuizmo::SetOrthographic(!isPerspective);

        // Tell ImGuizmo to draw on the current window.
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

        // Set the screen area (the viewport image bounds).
        ImGuizmo::SetRect(drawPosX, drawPosY, drawSizeX, drawSizeY);

        // ------------------------------------------------------------------------
        // C. HANDLE TOOL SWITCH INPUT (T = Move, E = Rotate, R = Scale)
        // ------------------------------------------------------------------------

        if (ImGui::IsWindowFocused() && !ImGuizmo::IsUsing())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_T)) { s_currentOperation = Operation::Translate; }
            if (ImGui::IsKeyPressed(ImGuiKey_E)) { s_currentOperation = Operation::Rotate; }
            if (ImGui::IsKeyPressed(ImGuiKey_R)) { s_currentOperation = Operation::Scale; }
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
        // E. EXECUTE GIZMO MANIPULATION
        // ------------------------------------------------------------------------

        // This call draws the gizmo and modifies modelMatrix based on mouse input.
        // Convert engine Matrix4x4 to column-major float array for ImGuizmo
        float modelArr[16];

        auto MatToColMajor = [](const Matrix4x4& M, float out[16]) {
            out[0] = M.m00; out[1] = M.m10; out[2] = M.m20; out[3] = M.m30;
            out[4] = M.m01; out[5] = M.m11; out[6] = M.m21; out[7] = M.m31;
            out[8] = M.m02; out[9] = M.m12; out[10] = M.m22; out[11] = M.m32;
            out[12] = M.m03; out[13] = M.m13; out[14] = M.m23; out[15] = M.m33;
        };

        MatToColMajor(modelMatrix, modelArr);

        ImGuizmo::Manipulate(
            viewMatrix,
            projMatrix,
            static_cast<ImGuizmo::OPERATION>(s_currentOperation),
            static_cast<ImGuizmo::MODE>(s_currentMode),
            modelArr
        );

        // ------------------------------------------------------------------------
        // F. APPLY MODIFIED TRANSFORM BACK TO ECS COMPONENT
        // ------------------------------------------------------------------------

        bool wasManipulated = false;

        if (ImGuizmo::IsUsing())
        {
            wasManipulated = true;
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

        return wasManipulated;
    }

} // namespace Editor