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
#include "core/Logger.h"
#include "math/Matrix4x4.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Editor {

    ViewportInteractionManager::ViewportInteractionManager()
        : m_pickingManager(std::make_unique<PickingQueryManager>())
        , m_selectedEntityId(0)
        , m_pendingPickRequestId(0) {
        
        // Set up gizmo interaction callbacks
        m_gizmoController.OnDragStart = [this](uint32_t entityId, const TransformDelta& delta) {
            // Capture initial transform when drag starts
            // (will be set by RenderGizmo before this fires)
        };

        m_gizmoController.OnDragEnd = [this](uint32_t entityId, const TransformDelta& delta) {
            // Notify observers that transform changed
            if (OnTransformChanged) {
                OnTransformChanged(entityId, m_dragInitialTransform, m_gizmoController.GetFinalTransform(), delta);
            }
        };
    }

    ViewportInteractionManager::~ViewportInteractionManager() = default;

    void ViewportInteractionManager::PrepareFrame(
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        bool isPerspective) {
        
        m_viewportPos = viewportPos;
        m_viewportSize = viewportSize;
        m_viewMatrix = viewMatrix;
        m_projMatrix = projMatrix;
        m_isPerspective = isPerspective;

        // Update gizmo viewport configuration
        m_gizmo.SetViewport(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);
        m_gizmo.SetPerspective(isPerspective);
    }

    uint32_t ViewportInteractionManager::Update(ECS::World& world, uint32_t selectedEntityId) {
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

        // Step 2: Update gizmo interaction state (verify entity is valid first)
        if (m_selectedEntityId != 0 && m_selectedEntityId != ECS::Entity::NPOS32) {
            ECS::Entity entity(m_selectedEntityId);
            if (world.IsAlive(entity)) {
                m_gizmoController.Update(m_gizmo, m_selectedEntityId);
            }
        }

        // Step 3: Check for new picks (on mouse click)
        // Note: This is a simplified version. In reality, we'd check ImGui input.
        // For now, just maintain the pending request.

        // Step 4: Apply gizmo transforms if entity is selected and being manipulated
        if (m_selectedEntityId != 0 && m_selectedEntityId != ECS::Entity::NPOS32 && (m_gizmoController.IsDragging() || m_gizmoController.JustStartedDrag())) {
            ECS::Entity entity(m_selectedEntityId);
            if (world.IsAlive(entity)) {
                // Capture initial transform on drag start
                if (m_gizmoController.JustStartedDrag()) {
                    m_dragInitialTransform = _getEntityTransform(world, m_selectedEntityId);
                }

                // Get gizmo output transform and apply to entity
                // (This happens in RenderGizmo, not here)
            }
        }

        return m_selectedEntityId;
    }

    void ViewportInteractionManager::RenderGizmo(ECS::World& world, uint32_t selectedEntityId) {
        if (selectedEntityId == ECS::Entity::NPOS32) {
            return;  // No entity selected, nothing to render
        }

        // Verify entity exists in the world
        ECS::Entity entity(selectedEntityId);
        if (!world.IsAlive(entity)) {
            return;  // Entity no longer exists
        }

        // Get entity's LOCAL transform (what we modify with gizmo)
        CachedTransformState localState = _getEntityTransform(world, selectedEntityId);

        // Get entity's WORLD position for correct gizmo rendering
        glm::vec3 worldPosition(0.0f);
        auto* worldTransform = world.TryGet<ECS::Components::WorldTransform>(entity);
        if (worldTransform) {
            worldPosition = glm::vec3(worldTransform->Matrix.m03, worldTransform->Matrix.m13, worldTransform->Matrix.m23);
        } else {
            // No world transform, use local position (entity has no parent)
            worldPosition = glm::vec3(localState.Position.X, localState.Position.Y, localState.Position.Z);
        }

        // Build matrix: world position + local rotation/scale
        // This ensures gizmo renders at correct world location while manipulating in local space
        glm::vec3 scale(localState.Scale.X, localState.Scale.Y, localState.Scale.Z);
        glm::quat rotation(localState.Rotation.W, localState.Rotation.X, localState.Rotation.Y, localState.Rotation.Z);

        glm::mat4 T = glm::translate(glm::mat4(1.0f), worldPosition);
        glm::mat4 R = glm::mat4_cast(rotation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
        glm::mat4 entityTransform = T * R * S;
        
        // Render and compute output transform using the entity's transform
        glm::mat4 outputTransform;
        bool isManipulating = m_gizmo.Render(m_viewMatrix, m_projMatrix, entityTransform, outputTransform);

        // Update gizmo interaction state
        m_gizmoController.Update(m_gizmo, selectedEntityId);

        // If gizmo is being used, apply the output transform to the entity
        if (isManipulating) {
            _applyGizmoTransform(world, selectedEntityId, outputTransform);

            // Update final transform snapshot on drag end
            if (m_gizmoController.JustEndedDrag()) {
                CachedTransformState finalState = _getEntityTransform(world, selectedEntityId);
                // (The manager internally stores this for the callback)
            }
        }
    }

    void ViewportInteractionManager::ResetInteraction() {
        m_gizmoController.Reset();
        m_pendingPickRequestId = 0;
        m_dragInitialTransform = CachedTransformState();
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
        // ImGuizmo in LOCAL mode outputs: world_position + local_rotation/scale matrix
        // For child entities with parent transforms, extracting scale from this hybrid matrix
        // can produce incorrect results due to numerical errors and parent transform interactions.
        // Solution: Only extract scale when explicitly in Scale mode; otherwise preserve original.
        // For rotation with non-uniform scale: use delta rotation to avoid extraction errors.
        
        ECS::Entity entity(entityId);
        CachedTransformState originalState = _getEntityTransform(world, entityId);
        
        // Extract components from gizmo output matrix
        glm::vec3 worldPosition = _extractPositionFromMatrix(transformMatrix);
        glm::vec3 gizmoScale = _extractScaleFromMatrix(transformMatrix);
        glm::quat newRotation = _extractRotationFromMatrix(transformMatrix);

        // Convert world position to local position for child entities
        glm::vec3 localPosition = _convertWorldToLocalPosition(world, entity, worldPosition);

        // Determine which scale to use based on gizmo mode
        glm::vec3 originalScale(originalState.Scale.X, originalState.Scale.Y, originalState.Scale.Z);
        glm::vec3 finalScale = originalScale;
        
        // Only extract scale from gizmo matrix if in Scale mode
        // In Rotate/Translate modes, preserve original scale to avoid corruption from parent transforms
        if (m_gizmo.GetOperation() == GizmoRenderer::Operation::Scale) {
            finalScale = gizmoScale;
        }

        // For rotation with non-uniform scale: use delta rotation to avoid extraction errors
        glm::quat finalRotation = newRotation;
        if (m_gizmo.GetOperation() == GizmoRenderer::Operation::Rotate) {
            // Detect non-uniform scale by comparing scale components
            float epsilon = 0.0001f;
            bool isNonUniform = (glm::abs(finalScale.x - finalScale.y) > epsilon) ||
                                (glm::abs(finalScale.y - finalScale.z) > epsilon) ||
                                (glm::abs(finalScale.x - finalScale.z) > epsilon);
            
            if (isNonUniform) {
                // Use delta rotation mode: apply delta rotation to original rotation
                // This avoids extraction errors from the hybrid world/local matrix with non-uniform scale
                glm::quat originalRotation(originalState.Rotation.W, originalState.Rotation.X, 
                                          originalState.Rotation.Y, originalState.Rotation.Z);
                
                // Compute delta rotation: newRot = deltaRot * origRot
                // Therefore: deltaRot = newRot * inv(origRot)
                glm::quat deltaRotation = newRotation * glm::conjugate(originalRotation);
                
                // Apply delta to original for more robust result
                finalRotation = deltaRotation * originalRotation;
            }
        }
        
        // Create transform state with local position, final rotation, and appropriate scale
        CachedTransformState localState(Vector3D(localPosition.x, localPosition.y, localPosition.z), 
                                        Quaternion(finalRotation.x, finalRotation.y, finalRotation.z, finalRotation.w),
                                        Vector3D(finalScale.x, finalScale.y, finalScale.z));
        
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
            auto* parentWorldTransform = world.TryGet<ECS::Components::WorldTransform>(parentEntity);
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
        ECS::Entity entity(entityId);
        
        if (!world.IsAlive(entity)) {
            return CachedTransformState();
        }

        // Read from LocalTransform (what we actually modify in gizmo)
        auto* localTransform = world.TryGet<ECS::Components::LocalTransform>(entity);
        if (localTransform) {
            return CachedTransformState(localTransform->Position, localTransform->Rotation, localTransform->Scale);
        }

        // Fallback: return identity
        return CachedTransformState();
    }

    void ViewportInteractionManager::_setEntityTransform(ECS::World& world, uint32_t entityId, const CachedTransformState& state) {
        ECS::Entity entity(entityId);

        if (!world.IsAlive(entity)) {
            return;
        }

        // Get or add LocalTransform component
        auto* localTransform = world.TryGet<ECS::Components::LocalTransform>(entity);
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
            world.Set(entity, newTransform);
        }
    }

}  // namespace Editor
