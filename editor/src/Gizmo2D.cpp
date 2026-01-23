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
#include "UndoSystem.h"
#include "core/Logger.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include "ecs/World.h"
#include "helpers/TransformUtils.h"
#include "math/Matrix4x4.h"
#include "math/Quaternion.h"
#include "math/Vector3D.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"

using namespace ECS;

namespace Editor {

    // Static member initialization
    EditorGizmo::Operation EditorGizmo::s_currentOperation = EditorGizmo::Operation::Translate;
    EditorGizmo::Mode EditorGizmo::s_currentMode = EditorGizmo::Mode::Local;

    void EditorGizmo::BeginFrame(ECS::World& world, uint32_t selectedEntityID) {        
        // If entity selection changed, reset drag context completely
        bool entitySelectionChanged = (this->m_dragContext.GetEntityID() != Entity::NPOS32 && 
                                       this->m_dragContext.GetEntityID() != selectedEntityID);
        if (entitySelectionChanged) {
            this->m_dragContext.Reset();
        }

        // Validate selected entity
        if (selectedEntityID == Entity::NPOS32) {
            return;
        }

        ECS::Entity entity = world.Resolve(selectedEntityID);
        if (entity.IsNull() || !world.IsAlive(entity)) {
            this->m_dragContext.Reset();
            return;
        }

        // Check if entity has transform
        if (!world.Has<ECS::Components::LocalTransform>(entity)) {
            this->m_dragContext.Reset();
            return;
        }

        // Capture initial transform at the start of frame for undo command creation
        if (!ImGuizmo::IsUsing() && this->m_dragContext.GetState() == GizmoDragContext::State::Idle) {
            const auto& lt = world.Get<ECS::Components::LocalTransform>(entity);
            this->m_dragContext.CaptureInitialTransform(lt.Position, lt.Rotation, lt.Scale);
        }
    }

    void EditorGizmo::EndFrame() {
        // Handle drag end - create undo command if transform was modified
        if (this->m_dragContext.JustEntered(GizmoDragContext::State::Ended) && m_undoSystem) {
            // The final transform was already applied during DrawGizmo
            // We just need to create the undo command with the captured initial state
            
            // Undo system will capture the final state internally
            // This is a placeholder for future undo integration
        }
    }

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
        // If entity selection changed, reset drag context completely
        // This prevents ImGuizmo state from previous entity affecting the new one
        bool entitySelectionChanged = (this->m_dragContext.GetEntityID() != Entity::NPOS32 && 
                                       this->m_dragContext.GetEntityID() != selectedEntityID);
        if (entitySelectionChanged) {
            this->m_dragContext.Reset();
        }

        // Validate selected entity
        if (selectedEntityID == Entity::NPOS32) {
            return false;
        }

        ECS::Entity entity = world.Resolve(selectedEntityID);

        // Skip if entity doesn't exist or was destroyed
        if (entity.IsNull() || !world.IsAlive(entity)) {
            this->m_dragContext.Reset();
            return false;
        }

        // Check if entity has transform
        if (!world.Has<ECS::Components::LocalTransform>(entity)) {
            this->m_dragContext.Reset();
            return false;
        }

        // ------------------------------------------------------------------------
        // B. SETUP IMGUIZMO CONTEXT
        // ------------------------------------------------------------------------

        ImGuizmo::BeginFrame();
        
        // Force ImGuizmo to clear its internal state when entity selection changes
        // ImGuizmo maintains global delta/matrix state that persists across frames
        if (entitySelectionChanged) {
            ImGuizmo::Enable(false);
            ImGuizmo::Enable(true);
        } else {
            // Normal case: just ensure gizmo is enabled
            ImGuizmo::Enable(true);
        }

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

        if (world.Has<ECS::Components::WorldTransform>(entity)) {
            const auto& wt = world.Get<ECS::Components::WorldTransform>(entity);
            modelMatrix = wt.Matrix;
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
        
        // CRITICAL: Skip Manipulate if selection just changed
        // If we just switched entities, calling Manipulate will cause ImGuizmo to
        // calculate a delta based on its cached mouse position from the previous entity,
        // resulting in a teleport. We'll just draw the gizmo without processing input.
        bool manipulated = false;
        bool isCurrentlyUsing = false;
        
        if (!entitySelectionChanged) {
            // Selection is stable - safe to call Manipulate
            manipulated = ImGuizmo::Manipulate(
                viewMatrix,
                projMatrix,
                static_cast<ImGuizmo::OPERATION>(s_currentOperation),
                static_cast<ImGuizmo::MODE>(s_currentMode),
                modelArr
            );
            isCurrentlyUsing = ImGuizmo::IsUsing();
        }
        // else: entitySelectionChanged = true
        //       Skip Manipulate entirely - gizmo will render but won't process input
        //       This allows ImGuizmo's internal state to stabilize before the next frame

        // Apply transform to entity while dragging
        if (isCurrentlyUsing)
        {
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

            float parentDet = 0.0f;
            Matrix4x4 invParent = parentWorld.Inverse(&parentDet);
            if (parentDet == 0.0f) {
                invParent = Matrix4x4::Identity();
            }
            Matrix4x4 localMatrix = invParent * newWorldMat;

            // Decompose localMatrix into T/R/S using engine TransformUtils
            Vector3D outPos, outScale;
            Quaternion outRot;
            TransformUtils::DecomposeTRS(localMatrix, outPos, outRot, outScale);

            // Engine translation convention uses last column (see Matrix4x4::Translation).
            // Force position extraction to match that convention so gizmo translation
            // always updates correctly.
            outPos = Vector3D(localMatrix.m03, localMatrix.m13, localMatrix.m23);

            // Update LocalTransform component
            auto lt = world.Get<ECS::Components::LocalTransform>(entity);
            lt.Position = outPos;
            lt.Scale = outScale;
            lt.Rotation = outRot;
            world.Set<ECS::Components::LocalTransform>(entity, lt);

            // Update WorldTransform immediately so subsequent frames use the
            // freshly edited world matrix (avoids snapping when hierarchy update
            // runs later).
            if (world.Has<ECS::Components::WorldTransform>(entity)) {
                auto wt = world.Get<ECS::Components::WorldTransform>(entity);
                wt.Matrix = newWorldMat;
                wt.Dirty = false;
                world.Set<ECS::Components::WorldTransform>(entity, wt);
            }
            else {
                ECS::Components::WorldTransform wt{};
                wt.Matrix = newWorldMat;
                wt.Dirty = false;
                world.Set<ECS::Components::WorldTransform>(entity, wt);
            }
        }

        // ------------------------------------------------------------------------
        // F. DRAG STATE MACHINE & EVENT EMISSION
        // ------------------------------------------------------------------------
        
        // Update drag state machine
        // Only track dragging if we're not in the middle of switching entities
        this->m_dragContext.Update(isCurrentlyUsing && !entitySelectionChanged, selectedEntityID);
        
        // Handle state transitions and emit appropriate events
        if (this->m_dragContext.JustEntered(GizmoDragContext::State::Started)) {
            // Capture initial transform snapshot
            const auto& lt = world.Get<ECS::Components::LocalTransform>(entity);
            this->m_dragContext.CaptureInitialTransform(lt.Position, lt.Rotation, lt.Scale);
        }
        
        if (this->m_dragContext.JustEntered(GizmoDragContext::State::InProgress)) {
            // Emit GizmoDragStarted event
            Messaging::MessageSystem::Notify(
                Messaging::GizmoDragStarted(
                    selectedEntityID,
                    this->m_dragContext.GetInitialPosition(),
                    this->m_dragContext.GetInitialRotation(),
                    this->m_dragContext.GetInitialScale()
                )
            );
        }
        
        if (this->m_dragContext.GetState() == GizmoDragContext::State::InProgress && isCurrentlyUsing) {
            // Emit GizmoDragging event while dragging
            const auto& lt_current = world.Get<ECS::Components::LocalTransform>(entity);
            Messaging::MessageSystem::Notify(
                Messaging::GizmoDragging(
                    selectedEntityID,
                    lt_current.Position,
                    lt_current.Rotation,
                    lt_current.Scale
                )
            );
        }
        
        if (this->m_dragContext.JustEntered(GizmoDragContext::State::Ended)) {
            // Emit GizmoDragEnded event
            const auto& lt_final = world.Get<ECS::Components::LocalTransform>(entity);
            
            // Only emit if transform actually changed
            if (this->m_dragContext.GetInitialPosition() != lt_final.Position ||
                this->m_dragContext.GetInitialRotation() != lt_final.Rotation ||
                this->m_dragContext.GetInitialScale() != lt_final.Scale) {
                
                Messaging::MessageSystem::Notify(
                    Messaging::GizmoDragEnded(
                        selectedEntityID,
                        lt_final.Position,
                        lt_final.Rotation,
                        lt_final.Scale,
                        this->m_dragContext.GetInitialPosition(),
                        this->m_dragContext.GetInitialRotation(),
                        this->m_dragContext.GetInitialScale()
                    )
                );
                
                // Mark scene as modified
                Messaging::MessageSystem::Notify(Messaging::SceneModified("Gizmo manipulation"));
            }
        }

        // "manipulated" is true when the matrix actually changed this frame.
        // Also treat the active drag as manipulation so callers can optionally
        // suppress other input while the gizmo is held.
        
        return manipulated || isCurrentlyUsing;
    }

} // namespace Editor