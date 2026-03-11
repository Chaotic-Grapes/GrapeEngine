/* Start Header *****************************************************************/
/*!
\file    ViewportInteractionManager.h
\author  Muhammad Nur Fadzly Bin Zulkifli (95%)
         Foo Rui Qin (5%)
\par     muhammadnurfadzly.b@digipen.edu
         ruiqin.foo@digipen.edu
\date    11th March 2026
\brief
Coordinates gizmo rendering, interaction state, and object picking for a viewport.

This manager handles:
- GizmoRenderer: ImGuizmo rendering and transformation
- GizmoInteractionController: Interaction state machine (idle/hovering/dragging/releasing)
- PickingQueryManager: GPU-based entity picking
- Selection updates: Changes selected entity based on picks
- Transform application: Applies gizmo-computed transforms to ECS entities
- Undo integration: Creates undo commands for transform changes

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef VIEWPORT_INTERACTION_MANAGER_H
#define VIEWPORT_INTERACTION_MANAGER_H

#include "GizmoRenderer.h"
#include "GizmoInteractionController.h"
#include "PickingQueryManager.h"
#include "TransformState.h"
#include "ecs/World.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <functional>
#include <memory>

namespace Editor {

    // Forward declarations
    class GizmoRenderer;
    class GizmoInteractionController;
    class PickingQueryManager;
    class TransformState;
}

namespace ECS {
    class RendererSystem;
}

namespace Editor {

    // Manages viewport interaction: gizmo rendering, entity picking and selection updates
    // Applies gizmo-computed transforms to ECS entities and notifies the undo system on drag end
    class ViewportInteractionManager {
    public:
        // -------------------------------------------------------------------------
        // Lifecycle
        // -------------------------------------------------------------------------

        ViewportInteractionManager();
        ~ViewportInteractionManager();

        // -------------------------------------------------------------------------
        // Per-Frame Update
        // -------------------------------------------------------------------------

        // Set up viewport bounds and camera matrices for the current frame
        // Must be called once per frame before Update()
        void PrepareFrame(
            const glm::vec2& viewportPos,
            const glm::vec2& viewportSize,
            const glm::mat4& viewMatrix,
            const glm::mat4& projMatrix,
            bool isPerspective);

        // Poll pending picks, update gizmo interaction state and apply transforms to entities
        // Returns the updated selected entity ID (may change due to picks)
        uint32_t Update(ECS::World& world, uint32_t selectedEntityId);

        // Render the gizmo for the selected entity; call during ImGui rendering phase
        void RenderGizmo(ECS::World& world, uint32_t selectedEntityId);

        // Abort any ongoing drag and return to idle state
        // Call when selection changes from UI rather than from picking
        void ResetInteraction();

        // -------------------------------------------------------------------------
        // Gizmo Configuration
        // -------------------------------------------------------------------------

        // Set the gizmo operation mode (translate, rotate, scale)
        void SetGizmoOperation(GizmoRenderer::Operation op) {
            m_gizmo.SetOperation(op);
        }

        // Set the gizmo coordinate mode (local or world space)
        void SetGizmoMode(GizmoRenderer::Mode mode) {
            m_gizmo.SetMode(mode);
        }

        // Configure gizmo snapping behavior for translate, rotate and scale axes
        void SetGizmoSnap(bool enabled, float translate, float rotate, float scale) {
            m_snapEnabled = enabled;
            m_snapTranslate = translate;
            m_snapRotate = rotate;
            m_snapScale = scale;
        }

        // Return the current gizmo operation mode
        GizmoRenderer::Operation GetGizmoOperation() const {
            return m_gizmo.GetOperation();
        }

        // Return the current gizmo coordinate mode
        GizmoRenderer::Mode GetGizmoMode() const {
            return m_gizmo.GetMode();
        }

        // Return true if the gizmo is being used or hovered and should block input
        bool ShouldBlockInput() const {
            return m_gizmo.ShouldBlockInput();
        }

        // -------------------------------------------------------------------------
        // Picking
        // -------------------------------------------------------------------------

        // Request a GPU entity pick at the given absolute screen coordinates
        void RequestPick(float screenX, float screenY, ECS::RendererSystem* rendererSystem);

        // Set a fallback entity ID to use if the next pick returns no result
        void SetNextPickFallback(uint32_t entityId);

        // -------------------------------------------------------------------------
        // Event Callbacks
        // -------------------------------------------------------------------------

        // Fired when entity selection changes; parameters are (oldEntityId, newEntityId)
        std::function<void(uint32_t, uint32_t)> OnSelectionChanged;

        // Fired when a gizmo drag completes; parameters are (entityId, initialTransform, finalTransform, delta)
        std::function<void(uint32_t, const CachedTransformState&, const CachedTransformState&, const TransformDelta&)> OnTransformChanged;

    private:
        // -------------------------------------------------------------------------
        // Internal Helpers
        // -------------------------------------------------------------------------

        // Handle a completed pick result and optionally update the selection
        // Returns the new selected entity ID
        uint32_t _handlePickResult(uint32_t pickedEntityId, bool shouldUpdateSelection);

        // Apply a gizmo-computed transform matrix to the given entity
        void _applyGizmoTransform(ECS::World& world, uint32_t entityId, const glm::mat4& transformMatrix);

        // Read and return the current LocalTransform of an entity as a CachedTransformState
        CachedTransformState _getEntityTransform(ECS::World& world, uint32_t entityId) const;

        // Write a CachedTransformState back to an entity's LocalTransform component
        void _setEntityTransform(ECS::World& world, uint32_t entityId, const CachedTransformState& state);

        // Extract the translation component from a 4x4 matrix
        glm::vec3 _extractPositionFromMatrix(const glm::mat4& matrix) const;

        // Extract the per-axis scale from a 4x4 matrix using column magnitudes
        glm::vec3 _extractScaleFromMatrix(const glm::mat4& matrix) const;

        // Extract the rotation quaternion from a 4x4 matrix
        glm::quat _extractRotationFromMatrix(const glm::mat4& matrix) const;

        // Convert a world-space position to local space, accounting for parent transform
        glm::vec3 _convertWorldToLocalPosition(ECS::World& world, ECS::Entity entity, const glm::vec3& worldPosition) const;

        // Extract the scale component from a parent entity's WorldTransform matrix
        glm::vec3 _extractParentScale(const Matrix4x4& parentMatrix) const;

        // Extract the rotation quaternion from a parent entity's WorldTransform matrix
        glm::quat _extractParentRotation(const Matrix4x4& parentMatrix, const glm::vec3& parentScale) const;

        // -------------------------------------------------------------------------
        // State
        // -------------------------------------------------------------------------

        // Owned subsystems
        GizmoRenderer m_gizmo;                              // ImGuizmo rendering and transform computation
        GizmoInteractionController m_gizmoController;       // Interaction state machine (idle/hover/drag/release)
        std::unique_ptr<PickingQueryManager> m_pickingManager; // GPU-based entity picking

        // Per-frame viewport and camera state
        glm::vec2 m_viewportPos{ 0.0f, 0.0f };              // Top-left corner of the viewport in screen space
        glm::vec2 m_viewportSize{ 1920.0f, 1080.0f };       // Viewport dimensions in pixels
        glm::mat4 m_viewMatrix = glm::mat4(1.0f);           // Camera view matrix
        glm::mat4 m_projMatrix = glm::mat4(1.0f);           // Camera projection matrix
        bool m_isPerspective = false;                       // True for perspective, false for orthographic

        // Per-frame snap configuration
        bool m_snapEnabled = false;                         // Whether snapping is enabled
        float m_snapTranslate = 1.0f;                       // Translation snap increment in world units
        float m_snapRotate = 15.0f;                         // Rotation snap increment in degrees
        float m_snapScale = 0.1f;                           // Scale snap increment

        // Interaction state
        uint32_t m_selectedEntityId = 0;                    // Currently selected entity ID
        uint32_t m_pendingPickRequestId = 0;                // ID of the pending pick request; 0 if none
        uint32_t m_nextPickFallbackId = ECS::Entity::NPOS32; // Fallback entity ID if the next pick returns nothing
    };

}  // namespace Editor

#endif  // VIEWPORT_INTERACTION_MANAGER_H