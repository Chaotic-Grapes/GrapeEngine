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

        // Get entity's current transform
        CachedTransformState entityState = _getEntityTransform(world, selectedEntityId);

        // Convert to glm::mat4 for ImGuizmo
        // Build matrix using proper TRS (Translate-Rotate-Scale) order
        glm::vec3 position(entityState.Position.X, entityState.Position.Y, entityState.Position.Z);
        glm::vec3 scale(entityState.Scale.X, entityState.Scale.Y, entityState.Scale.Z);
        glm::quat rotation(entityState.Rotation.W, entityState.Rotation.X, entityState.Rotation.Y, entityState.Rotation.Z);

        // Build TRS matrix: M = T * R * S
        glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
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
        // Extract position, rotation, scale from glm::mat4
        
        // Extract position from translation column
        glm::vec3 position = glm::vec3(transformMatrix[3]);
        
        // Extract scale from column magnitudes
        glm::vec3 scale;
        scale.x = glm::length(glm::vec3(transformMatrix[0]));
        scale.y = glm::length(glm::vec3(transformMatrix[1]));
        scale.z = glm::length(glm::vec3(transformMatrix[2]));
        
        // Extract rotation (remove scale first)
        glm::mat3 rotationMatrix = glm::mat3(transformMatrix);
        rotationMatrix[0] /= scale.x;
        rotationMatrix[1] /= scale.y;
        rotationMatrix[2] /= scale.z;
        glm::quat rotation = glm::quat_cast(rotationMatrix);

        // Create transform state and apply
        CachedTransformState state = _getEntityTransform(world, entityId);
        state.Position = Vector3D(position.x, position.y, position.z);
        state.Rotation = Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
        state.Scale = Vector3D(scale.x, scale.y, scale.z);
        
        _setEntityTransform(world, entityId, state);
    }

    CachedTransformState ViewportInteractionManager::_getEntityTransform(ECS::World& world, uint32_t entityId) const {
        ECS::Entity entity(entityId);
        
        if (!world.IsAlive(entity)) {
            return CachedTransformState();
        }

        // Try to get LocalTransform component
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
