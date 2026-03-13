/* Start Header *****************************************************************/
/*!
\file   ViewportInteractionManager.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (95%)
        Foo Rui Qin (5%)
\par    muhammadnurfadzly.b@digipen.edu
        ruiqin.foo@digipen.edu
\date   12th March 2026
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
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Editor {
    namespace {
        // Converts engine Matrix4x4 storage into GLM layout used by gizmo and matrix math helpers
        glm::mat4 ToGlmMat4(const Matrix4x4& m) {
            glm::mat4 out(1.0f);
            out[0][0] = m.m00; out[1][0] = m.m01; out[2][0] = m.m02; out[3][0] = m.m03;
            out[0][1] = m.m10; out[1][1] = m.m11; out[2][1] = m.m12; out[3][1] = m.m13;
            out[0][2] = m.m20; out[1][2] = m.m21; out[2][2] = m.m22; out[3][2] = m.m23;
            out[0][3] = m.m30; out[1][3] = m.m31; out[2][3] = m.m32; out[3][3] = m.m33;
            return out;
        }

        // Converts GLM matrix values back into engine Matrix4x4 format for transform utility calls
        Matrix4x4 ToMatrix4x4(const glm::mat4& m) {
            return Matrix4x4(
                m[0][0], m[1][0], m[2][0], m[3][0],
                m[0][1], m[1][1], m[2][1], m[3][1],
                m[0][2], m[1][2], m[2][2], m[3][2],
                m[0][3], m[1][3], m[2][3], m[3][3]
            );
        }
    }

    // Initializes picking and gizmo interaction state and binds gizmo drag callbacks
    ViewportInteractionManager::ViewportInteractionManager()
        : m_pickingManager{std::make_unique<PickingQueryManager>()}
        , m_selectedEntityId{ECS::Entity::NPOS32}
        , m_pendingPickRequestId{0} {

        // Register drag start callback even though we currently do not consume drag start payload
        m_gizmoController.OnDragStart = [this](const uint32_t entityId, const TransformDelta& delta) {
            // Keep parameters intentionally unused so callback shape stays stable for future behavior
            (void)entityId;
            (void)delta;
        };

        // Drag end callback forwards old and new state so inspector and history systems can record edits
        m_gizmoController.OnDragEnd = [this](const uint32_t entityId, const TransformDelta& delta) {
            if (OnTransformChanged) {
                OnTransformChanged(entityId, m_gizmoController.GetInitialTransform(), m_gizmoController.GetFinalTransform(), delta);
            }
        };
    }

    // Uses default cleanup because owned members are RAII containers and smart pointers
    ViewportInteractionManager::~ViewportInteractionManager() = default;

    // Stores per frame viewport and camera matrices then updates gizmo viewport configuration
    void ViewportInteractionManager::PrepareFrame(
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        const bool isPerspective) {

        // Cache frame state because pick and gizmo math use these values in later calls
        m_viewportPos = viewportPos;
        m_viewportSize = viewportSize;
        m_viewMatrix = viewMatrix;
        m_projMatrix = projMatrix;
        m_isPerspective = isPerspective;

        // Keep gizmo camera setup synchronized with latest viewport dimensions and projection mode
        m_gizmo.SetViewport(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);
        m_gizmo.SetPerspective(isPerspective);
    }

    // Polls asynchronous pick results and updates selected id while preserving drag interaction rules
    uint32_t ViewportInteractionManager::Update(ECS::World& world, const uint32_t selectedEntityId) {
        (void)world;
        m_selectedEntityId = selectedEntityId;

        // Poll outstanding pick request first so click from previous frame can update selection this frame
        if (m_pendingPickRequestId != 0) {
            uint32_t pickedEntityId;
            if (m_pickingManager->TryGetPickResult(m_pendingPickRequestId, pickedEntityId)) {
                // Ignore pick driven selection changes while drag is active to avoid jumpy gizmo ownership
                bool shouldUpdateSelection = !m_gizmoController.IsDragging();
                m_selectedEntityId = _handlePickResult(pickedEntityId, shouldUpdateSelection);
                m_pendingPickRequestId = 0;
            }
        }

        // Gizmo interaction state is updated inside RenderGizmo after manipulation call completes
        // New pick requests are triggered externally through RequestPick on click handling path
        // Transform writes are applied in RenderGizmo only when gizmo reports manipulation
        return m_selectedEntityId;
    }

    // Renders gizmo for selected entity and applies manipulated transform back into LocalTransform
    void ViewportInteractionManager::RenderGizmo(ECS::World& world, uint32_t selectedEntityId) {
        if (selectedEntityId == ECS::Entity::NPOS32) {
            return;  // No selected entity means there is no gizmo target
        }

        // Resolve selected id each frame because entities can be deleted between updates
        ECS::Entity entity = world.Resolve(selectedEntityId);
        if (!world.IsAlive(entity)) {
            return;  // Stop if selected id no longer maps to a living entity
        }

        // Local transform is the editable state we write back after gizmo manipulation
        CachedTransformState localState = _getEntityTransform(world, selectedEntityId);

        // Build world space matrix so gizmo appears at correct global position under parent hierarchy
        Matrix4x4 worldMatrix;
        bool hasWorldMatrix = false;

        auto* worldTransform = Editor::ECSUtils::GetComponentPtr<ECS::Components::WorldTransform>(
            &world, entity, "WorldTransform");
        if (worldTransform && !worldTransform->Dirty) {
            worldMatrix = worldTransform->Matrix;
            hasWorldMatrix = true;
        }

        // Reconstruct world matrix when cached world transform is missing or dirty
        if (!hasWorldMatrix) {
            const Matrix4x4 localMatrix = TransformUtils::MakeTRS(localState.Position, localState.Rotation, localState.Scale);
            ECS::Entity parentEntity = world.ParentOf(entity);
            if (!parentEntity.IsNull()) {
                auto* parentWorld = Editor::ECSUtils::GetComponentPtr<ECS::Components::WorldTransform>(
                    &world, parentEntity, "WorldTransform");
                if (parentWorld && !parentWorld->Dirty) {
                    // Child world matrix is parent world multiplied by child local matrix
                    worldMatrix = parentWorld->Matrix * localMatrix;
                    hasWorldMatrix = true;
                }
            }

            if (!hasWorldMatrix) {
                // Without valid parent world data local matrix already represents world matrix
                worldMatrix = localMatrix;
            }
        }

        // Decompose world matrix into translation rotation and scale for GLM transform reconstruction
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

        // Output transform receives manipulated world matrix produced by gizmo render pass
        glm::mat4 outputTransform;

        // Pick correct snap array based on active operation because translate rotate and scale use different units
        const float* snapPtr = nullptr;
        float snapTranslate[3] = { m_snapTranslate, m_snapTranslate, m_snapTranslate };
        float snapScale[3] = { m_snapScale, m_snapScale, m_snapScale };
        float snapRotate = m_snapRotate;

        if (m_snapEnabled) {
            switch (m_gizmo.GetOperation()) {
            case GizmoRenderer::Operation::Translate:
                snapPtr = snapTranslate;
                break;
            case GizmoRenderer::Operation::Rotate:
                snapPtr = &snapRotate;
                break;
            case GizmoRenderer::Operation::Scale:
                snapPtr = snapScale;
                break;
            default:
                break;
            }
        }

        // Render gizmo and request manipulated output matrix when user is actively interacting
        bool isManipulating = m_gizmo.Render(m_viewMatrix, m_projMatrix, entityTransform, outputTransform, snapPtr);

        // Update drag lifecycle state after render so callbacks observe latest manipulation outcome
        m_gizmoController.Update(m_gizmo, selectedEntityId, localState);

        // Apply manipulated matrix only while operation is active so idle gizmo does not rewrite transform
        if (isManipulating) {
            _applyGizmoTransform(world, selectedEntityId, outputTransform);
        }
    }

    // Clears active interaction state including pending pick request and gizmo drag state
    void ViewportInteractionManager::ResetInteraction() {
        m_gizmoController.Reset();
        m_pendingPickRequestId = 0;
    }

    // Queues asynchronous pick request in viewport coordinates when renderer is available
    void ViewportInteractionManager::RequestPick(float screenX, float screenY, ECS::RendererSystem* rendererSystem) {
        if (!rendererSystem) {
            return;  // Picking needs renderer submitted id buffer data
        }

        // Avoid queueing another request while one is still unresolved
        if (m_pendingPickRequestId == 0) {
            m_pendingPickRequestId = m_pickingManager->RequestPick(
                screenX,
                screenY,
                m_viewportPos,
                m_viewportSize,
                rendererSystem);
        }
    }

    // Stores one shot fallback id used when next pick misses all entities
    void ViewportInteractionManager::SetNextPickFallback(uint32_t entityId) {
        m_nextPickFallbackId = entityId;
    }

    // Resolves pick result into final selection id and emits selection changed callback when needed
    uint32_t ViewportInteractionManager::_handlePickResult(uint32_t pickedEntityId, bool shouldUpdateSelection) {
        if (!shouldUpdateSelection) {
            // Clear fallback because this pick cycle is consumed even if selection update is suppressed
            m_nextPickFallbackId = ECS::Entity::NPOS32;
            return m_selectedEntityId;  // Keep current selection while dragging
        }

        uint32_t oldSelection = m_selectedEntityId;
        uint32_t newSelection = pickedEntityId;

        // When pick misses, use pre registered fallback target such as tilemap surface entity
        if (newSelection == ECS::Entity::NPOS32 && m_nextPickFallbackId != ECS::Entity::NPOS32) {
            newSelection = m_nextPickFallbackId;
        }

        // Fallback is one use only so always clear it after processing this pick result
        m_nextPickFallbackId = ECS::Entity::NPOS32;

        // Emit callback only when effective selection actually changed
        if (oldSelection != newSelection) {
            if (OnSelectionChanged) {
                OnSelectionChanged(oldSelection, newSelection);
            }
        }

        return newSelection;
    }

    // Converts manipulated world matrix into local space then writes it into entity LocalTransform
    void ViewportInteractionManager::_applyGizmoTransform(ECS::World& world, uint32_t entityId, const glm::mat4& transformMatrix) {
        ECS::Entity entity = world.Resolve(entityId);
        glm::mat4 localMatrix = transformMatrix;
        ECS::Entity parentEntity = world.ParentOf(entity);
        if (parentEntity.Index != ECS::Entity::NPOS32) {
            auto* parentWorldTransform = Editor::ECSUtils::GetComponentPtr<ECS::Components::WorldTransform>(
                &world, parentEntity, "WorldTransform");
            if (parentWorldTransform && !parentWorldTransform->Dirty) {
                // Convert manipulated world matrix into child local space by multiplying inverse parent world
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

    // Extracts translation from matrix fourth column in GLM column major layout
    glm::vec3 ViewportInteractionManager::_extractPositionFromMatrix(const glm::mat4& matrix) const {
        return glm::vec3(matrix[3]);
    }

    // Extracts scale by taking length of basis vectors from transform matrix columns
    glm::vec3 ViewportInteractionManager::_extractScaleFromMatrix(const glm::mat4& matrix) const {
        // GLM uses column major matrices where matrix[i] is basis column i
        // Basis vector length gives scale magnitude even for non uniform scale
        glm::vec3 scale;
        scale.x = glm::length(glm::vec3(matrix[0])); // Column 0
        scale.y = glm::length(glm::vec3(matrix[1])); // Column 1
        scale.z = glm::length(glm::vec3(matrix[2])); // Column 2
        return scale;
    }

    // Extracts rotation by normalizing scaled basis vectors then casting to quaternion
    glm::quat ViewportInteractionManager::_extractRotationFromMatrix(const glm::mat4& matrix) const {
        glm::vec3 scale = _extractScaleFromMatrix(matrix);

        glm::mat3 rotationMatrix = glm::mat3(matrix);
        if (scale.x > 0.0001f) rotationMatrix[0] /= scale.x;
        if (scale.y > 0.0001f) rotationMatrix[1] /= scale.y;
        if (scale.z > 0.0001f) rotationMatrix[2] /= scale.z;

        return glm::quat_cast(rotationMatrix);
    }

    // Converts world position into entity local position using parent inverse translation rotation and scale
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

                // Convert world position to local by undoing parent translation then parent rotation then parent scale
                // This preserves child local offset even when parent has non unit scale
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

    // Extracts parent scale from matrix basis lengths in engine Matrix4x4 layout
    glm::vec3 ViewportInteractionManager::_extractParentScale(const Matrix4x4& parentMatrix) const {
        return glm::vec3(
            std::sqrt(parentMatrix.m00 * parentMatrix.m00 + parentMatrix.m10 * parentMatrix.m10 + parentMatrix.m20 * parentMatrix.m20),
            std::sqrt(parentMatrix.m01 * parentMatrix.m01 + parentMatrix.m11 * parentMatrix.m11 + parentMatrix.m21 * parentMatrix.m21),
            std::sqrt(parentMatrix.m02 * parentMatrix.m02 + parentMatrix.m12 * parentMatrix.m12 + parentMatrix.m22 * parentMatrix.m22)
        );
    }

    // Builds parent rotation quaternion from normalized parent basis vectors
    glm::quat ViewportInteractionManager::_extractParentRotation(const Matrix4x4& parentMatrix, const glm::vec3& parentScale) const {
        // Matrix4x4 fields are stored in row major naming while GLM constructor consumes column vectors
        // Values are arranged per column then normalized by corresponding scale axis
        glm::mat3 parentRotMat(
            parentMatrix.m00 / parentScale.x, parentMatrix.m10 / parentScale.x, parentMatrix.m20 / parentScale.x,  // Column 0
            parentMatrix.m01 / parentScale.y, parentMatrix.m11 / parentScale.y, parentMatrix.m21 / parentScale.y,  // Column 1
            parentMatrix.m02 / parentScale.z, parentMatrix.m12 / parentScale.z, parentMatrix.m22 / parentScale.z   // Column 2
        );
        return glm::quat_cast(parentRotMat);
    }

    // Reads LocalTransform for entity and falls back to identity when missing or invalid
    CachedTransformState ViewportInteractionManager::_getEntityTransform(ECS::World& world, uint32_t entityId) const {
        ECS::Entity entity = world.Resolve(entityId);

        if (!world.IsAlive(entity)) {
            return CachedTransformState();
        }

        // LocalTransform is the source of truth for editor manipulations
        auto* localTransform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(
            &world, entity, "LocalTransform");
        if (localTransform) {
            return CachedTransformState(localTransform->Position, localTransform->Rotation, localTransform->Scale);
        }

        // Missing local transform means use identity to keep gizmo path safe
        return CachedTransformState();
    }

    // Writes local transform state back to entity creating LocalTransform when absent
    void ViewportInteractionManager::_setEntityTransform(ECS::World& world, uint32_t entityId, const CachedTransformState& state) {
        ECS::Entity entity = world.Resolve(entityId);

        if (!world.IsAlive(entity)) {
            return;
        }

        // Update existing LocalTransform when available for minimal mutation path
        auto* localTransform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(
            &world, entity, "LocalTransform");
        if (localTransform) {
            localTransform->Position = state.Position;
            localTransform->Rotation = state.Rotation;
            localTransform->Scale = state.Scale;
        }
        else {
            // Create LocalTransform on demand so entities without it can still be manipulated
            ECS::Components::LocalTransform newTransform;
            newTransform.Position = state.Position;
            newTransform.Rotation = state.Rotation;
            newTransform.Scale = state.Scale;
            Editor::ECSUtils::SetComponent(&world, entity, "LocalTransform", newTransform);
        }
    }

}  // namespace Editor
