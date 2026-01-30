/* Start Header *****************************************************************/
/*!
\file   ViewportInteractionManager.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of ViewportInteractionManager for coordinating gizmo, picking,
and selection.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ViewportInteractionManager.h"
#include "ecs/Components.h"
#include "EditorECSUtils.h"
#include "core/Logger.h"
#include "math/Matrix4x4.h"
#include "helpers/TransformUtils.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Editor {
    namespace {
        glm::mat4 ToGlmMat4(const Matrix4x4& m) {
            glm::mat4 out(1.0f);
            out[0][0] = m.m00; out[1][0] = m.m01; out[2][0] = m.m02; out[3][0] = m.m03;
            out[0][1] = m.m10; out[1][1] = m.m11; out[2][1] = m.m12; out[3][1] = m.m13;
            out[0][2] = m.m20; out[1][2] = m.m21; out[2][2] = m.m22; out[3][2] = m.m23;
            out[0][3] = m.m30; out[1][3] = m.m31; out[2][3] = m.m32; out[3][3] = m.m33;
            return out;
        }

        Matrix4x4 ToMatrix4x4(const glm::mat4& m) {
            return Matrix4x4(
                m[0][0], m[1][0], m[2][0], m[3][0],
                m[0][1], m[1][1], m[2][1], m[3][1],
                m[0][2], m[1][2], m[2][2], m[3][2],
                m[0][3], m[1][3], m[2][3], m[3][3]
            );
        }
    }

    ViewportInteractionManager::ViewportInteractionManager()
        : m_pickingManager{std::make_unique<PickingQueryManager>()}
        , m_selectedEntityId{ECS::Entity::NPOS32}
        , m_pendingPickRequestId{0} {
        
        // Set up gizmo interaction callbacks
        m_gizmoController.OnDragStart = [this](const uint32_t entityId, const TransformDelta& delta) {
            // Capture initial transform when drag starts
            // (will be set by RenderGizmo before this fires)

            // nothing right now so
			(void)entityId;
            (void)delta;
        };

        m_gizmoController.OnDragEnd = [this](const uint32_t entityId, const TransformDelta& delta) {
            // Notify observers that transform changed
            if (OnTransformChanged) {
                OnTransformChanged(entityId, m_gizmoController.GetInitialTransform(), m_gizmoController.GetFinalTransform(), delta);
            }
        };
    }

    ViewportInteractionManager::~ViewportInteractionManager() = default;

    void ViewportInteractionManager::PrepareFrame(
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        const bool isPerspective) {
        
        m_viewportPos = viewportPos;
        m_viewportSize = viewportSize;
        m_viewMatrix = viewMatrix;
        m_projMatrix = projMatrix;
        m_isPerspective = isPerspective;

        // Update gizmo viewport configuration
        m_gizmo.SetViewport(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);
        m_gizmo.SetPerspective(isPerspective);
    }

    uint32_t ViewportInteractionManager::Update(ECS::World& world, const uint32_t selectedEntityId) {
        m_selectedEntityId = selectedEntityId;

        // Step 1: Poll pending pick results from last frame
        if (m_pendingPickRequestId != 0) {
            uint32_t pickedEntityId;
            if (m_pickingManager->TryGetPickResult(m_pendingPickRequestId, pickedEntityId)) {
                // Pick result is ready
                bool shouldUpdateSelection = !m_gizmoController.IsDragging();
                m_selectedEntityId = _handlePickResult(pickedEntityId, shouldUpdateSelection);
                m_pendingPickRequestId = 0;
            }
        }

        // Step 2: Gizmo interaction state is updated in RenderGizmo after ImGuizmo runs.

        // Step 3: Check for new picks (on mouse click)
        // Note: This is a simplified version. In reality, we'd check ImGui input.
        // For now, just maintain the pending request.

        // Step 4: Gizmo transform application happens in RenderGizmo.

        return m_selectedEntityId;
    }

    void ViewportInteractionManager::RenderGizmo(ECS::World& world, uint32_t selectedEntityId) {
        if (selectedEntityId == ECS::Entity::NPOS32) {
            return;  // No entity selected, nothing to render
        }

        // Verify entity exists in the world
        ECS::Entity entity = world.Resolve(selectedEntityId);
        if (!world.IsAlive(entity)) {
            return;  // Entity no longer exists
        }

        // Get entity's LOCAL transform (what we modify with gizmo)
        CachedTransformState localState = _getEntityTransform(world, selectedEntityId);

        // Build world transform matrix for correct gizmo rendering
        Matrix4x4 worldMatrix;
        bool hasWorldMatrix = false;

        auto* worldTransform = Editor::ECSUtils::GetComponentPtr<ECS::Components::WorldTransform>(
            &world, entity, "WorldTransform");
        if (worldTransform && !worldTransform->Dirty) {
            worldMatrix = worldTransform->Matrix;
            hasWorldMatrix = true;
        }

        if (!hasWorldMatrix) {
            const Matrix4x4 localMatrix = TransformUtils::MakeTRS(localState.Position, localState.Rotation, localState.Scale);
            ECS::Entity parentEntity = world.ParentOf(entity);
            if (!parentEntity.IsNull()) {
                auto* parentWorld = Editor::ECSUtils::GetComponentPtr<ECS::Components::WorldTransform>(
                    &world, parentEntity, "WorldTransform");
                if (parentWorld && !parentWorld->Dirty) {
                    worldMatrix = parentWorld->Matrix * localMatrix;
                    hasWorldMatrix = true;
                }
            }

            if (!hasWorldMatrix) {
                worldMatrix = localMatrix;
            }
        }

        Vector3D worldPosition;
        Quaternion worldRotation;
        Vector3D worldScale;
        TransformUtils::DecomposeTRS(worldMatrix, worldPosition, worldRotation, worldScale);

        glm::vec3 position(worldPosition.X, worldPosition.Y, worldPosition.Z);
        glm::quat rotation(worldRotation.W, worldRotation.X, worldRotation.Y, worldRotation.Z);
        glm::vec3 scale(worldScale.X, worldScale.Y, worldScale.Z);

        glm::mat4 entityTransform = glm::translate(glm::mat4(1.0f), position) *
                                    glm::mat4_cast(rotation) *
                                    glm::scale(glm::mat4(1.0f), scale);
        
        // Render and compute output transform using the entity's transform
        glm::mat4 outputTransform;
        bool isManipulating = m_gizmo.Render(m_viewMatrix, m_projMatrix, entityTransform, outputTransform);

        // Update gizmo interaction state
        m_gizmoController.Update(m_gizmo, selectedEntityId, localState);

        // If gizmo is being used, apply the output transform to the entity
        if (isManipulating) {
            _applyGizmoTransform(world, selectedEntityId, outputTransform);
        }
    }

    void ViewportInteractionManager::ResetInteraction() {
        m_gizmoController.Reset();
        m_pendingPickRequestId = 0;
    }

    void ViewportInteractionManager::RequestPick(float screenX, float screenY, ECS::RendererSystem* rendererSystem) {
        if (!rendererSystem) {
            return;  // No renderer available
        }

        // Only request a pick if no pick is already pending
        if (m_pendingPickRequestId == 0) {
            m_pendingPickRequestId = m_pickingManager->RequestPick(
                screenX,
                screenY,
                m_viewportPos,
                m_viewportSize,
                rendererSystem);
        }
    }

    uint32_t ViewportInteractionManager::_handlePickResult(uint32_t pickedEntityId, bool shouldUpdateSelection) {
        if (!shouldUpdateSelection) {
            return m_selectedEntityId;  // Keep current selection
        }

        uint32_t oldSelection = m_selectedEntityId;
        uint32_t newSelection = pickedEntityId;

        if (oldSelection != newSelection) {
            if (OnSelectionChanged) {
                OnSelectionChanged(oldSelection, newSelection);
            }
        }

        return newSelection;
    }

    void ViewportInteractionManager::_applyGizmoTransform(ECS::World& world, uint32_t entityId, const glm::mat4& transformMatrix) {
        ECS::Entity entity = world.Resolve(entityId);
        glm::mat4 localMatrix = transformMatrix;
        ECS::Entity parentEntity = world.ParentOf(entity);
        if (parentEntity.Index != ECS::Entity::NPOS32) {
            auto* parentWorldTransform = Editor::ECSUtils::GetComponentPtr<ECS::Components::WorldTransform>(
                &world, parentEntity, "WorldTransform");
            if (parentWorldTransform && !parentWorldTransform->Dirty) {
                glm::mat4 parentWorld = ToGlmMat4(parentWorldTransform->Matrix);
                localMatrix = glm::inverse(parentWorld) * transformMatrix;
            }
        }

        Vector3D localPosition;
        Quaternion localRotation;
        Vector3D localScale;
        const Matrix4x4 localMatrix4x4 = ToMatrix4x4(localMatrix);
        TransformUtils::DecomposeTRS(localMatrix4x4, localPosition, localRotation, localScale);

        CachedTransformState localState(localPosition, localRotation, localScale);
        _setEntityTransform(world, entityId, localState);
    }

    glm::vec3 ViewportInteractionManager::_extractPositionFromMatrix(const glm::mat4& matrix) const {
        return glm::vec3(matrix[3]);
    }

    glm::vec3 ViewportInteractionManager::_extractScaleFromMatrix(const glm::mat4& matrix) const {
        // GLM uses column-major matrices; each matrix[i] is column i.
        // Extract scale as the length of each column vector (handles both uniform and non-uniform scale).
        glm::vec3 scale;
        scale.x = glm::length(glm::vec3(matrix[0])); // Column 0
        scale.y = glm::length(glm::vec3(matrix[1])); // Column 1
        scale.z = glm::length(glm::vec3(matrix[2])); // Column 2
        return scale;
    }

    glm::quat ViewportInteractionManager::_extractRotationFromMatrix(const glm::mat4& matrix) const {
        glm::vec3 scale = _extractScaleFromMatrix(matrix);
        
        glm::mat3 rotationMatrix = glm::mat3(matrix);
        if (scale.x > 0.0001f) rotationMatrix[0] /= scale.x;
        if (scale.y > 0.0001f) rotationMatrix[1] /= scale.y;
        if (scale.z > 0.0001f) rotationMatrix[2] /= scale.z;
        
        return glm::quat_cast(rotationMatrix);
    }

    glm::vec3 ViewportInteractionManager::_convertWorldToLocalPosition(ECS::World& world, ECS::Entity entity, const glm::vec3& worldPosition) const {
        glm::vec3 localPosition = worldPosition;
        ECS::Entity parentEntity = world.ParentOf(entity);
        
        if (parentEntity.Index != ECS::Entity::NPOS32) {
            auto* parentWorldTransform = Editor::ECSUtils::GetComponentPtr<ECS::Components::WorldTransform>(
                &world, parentEntity, "WorldTransform");
            if (parentWorldTransform) {
                glm::vec3 parentWorldPos(parentWorldTransform->Matrix.m03, parentWorldTransform->Matrix.m13, parentWorldTransform->Matrix.m23);
                glm::vec3 parentScale = _extractParentScale(parentWorldTransform->Matrix);
                glm::quat parentRotation = _extractParentRotation(parentWorldTransform->Matrix, parentScale);
                
                // Convert world position to local:
                // local_pos = inv(parent_scale) * inv(parent_rot) * (world_pos - parent_pos)
                // This is necessary so child position stays constant when parent scale changes
                glm::vec3 relativePos = worldPosition - parentWorldPos;
                glm::quat parentRotInv = glm::conjugate(parentRotation);
                localPosition = parentRotInv * relativePos;
                
                // Divide by parent scale to get true local position
                if (parentScale.x > 0.0001f) localPosition.x /= parentScale.x;
                if (parentScale.y > 0.0001f) localPosition.y /= parentScale.y;
                if (parentScale.z > 0.0001f) localPosition.z /= parentScale.z;
            }
        }
        
        return localPosition;
    }

    glm::vec3 ViewportInteractionManager::_extractParentScale(const Matrix4x4& parentMatrix) const {
        return glm::vec3(
            std::sqrt(parentMatrix.m00 * parentMatrix.m00 + parentMatrix.m10 * parentMatrix.m10 + parentMatrix.m20 * parentMatrix.m20),
            std::sqrt(parentMatrix.m01 * parentMatrix.m01 + parentMatrix.m11 * parentMatrix.m11 + parentMatrix.m21 * parentMatrix.m21),
            std::sqrt(parentMatrix.m02 * parentMatrix.m02 + parentMatrix.m12 * parentMatrix.m12 + parentMatrix.m22 * parentMatrix.m22)
        );
    }

    glm::quat ViewportInteractionManager::_extractParentRotation(const Matrix4x4& parentMatrix, const glm::vec3& parentScale) const {
        // Matrix4x4 is row-major (m00,m01,m02 is first row, m10,m11,m12 is second row)
        // GLM mat3 constructor expects column-major (3 columns), so transpose the indexing
        // Each scale factor normalizes the corresponding column of the rotation matrix
        glm::mat3 parentRotMat(
            parentMatrix.m00 / parentScale.x, parentMatrix.m10 / parentScale.x, parentMatrix.m20 / parentScale.x,  // Column 0
            parentMatrix.m01 / parentScale.y, parentMatrix.m11 / parentScale.y, parentMatrix.m21 / parentScale.y,  // Column 1
            parentMatrix.m02 / parentScale.z, parentMatrix.m12 / parentScale.z, parentMatrix.m22 / parentScale.z   // Column 2
        );
        return glm::quat_cast(parentRotMat);
    }

    CachedTransformState ViewportInteractionManager::_getEntityTransform(ECS::World& world, uint32_t entityId) const {
        ECS::Entity entity = world.Resolve(entityId);
        
        if (!world.IsAlive(entity)) {
            return CachedTransformState();
        }

        // Read from LocalTransform (what we actually modify in gizmo)
        auto* localTransform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(
            &world, entity, "LocalTransform");
        if (localTransform) {
            return CachedTransformState(localTransform->Position, localTransform->Rotation, localTransform->Scale);
        }

        // Fallback: return identity
        return CachedTransformState();
    }

    void ViewportInteractionManager::_setEntityTransform(ECS::World& world, uint32_t entityId, const CachedTransformState& state) {
        ECS::Entity entity = world.Resolve(entityId);

        if (!world.IsAlive(entity)) {
            return;
        }

        // Get or add LocalTransform component
        auto* localTransform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(
            &world, entity, "LocalTransform");
        if (localTransform) {
            localTransform->Position = state.Position;
            localTransform->Rotation = state.Rotation;
            localTransform->Scale = state.Scale;
        } else {
            // Add component if missing
            ECS::Components::LocalTransform newTransform;
            newTransform.Position = state.Position;
            newTransform.Rotation = state.Rotation;
            newTransform.Scale = state.Scale;
            Editor::ECSUtils::SetComponent(&world, entity, "LocalTransform", newTransform);
        }
    }

}  // namespace Editor
