/* Start Header *****************************************************************/
/*!
\file   ViewportInteractionManager.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
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

    /**
     * @brief Manages viewport interaction: gizmo, picking, and selection
     * 
     * This manager is responsible for:
     * 1. Rendering gizmo handles via GizmoRenderer
     * 2. Detecting interaction state via GizmoInteractionController
     * 3. Requesting entity picks via PickingQueryManager
     * 4. Updating entity selection based on picks
     * 5. Applying gizmo-computed transforms to ECS entities
     * 6. Notifying listeners when transforms change (for undo system)
     * 
     * The manager maintains per-frame state:
     * - Current selected entity ID
     * - Viewport bounds (position and size)
     * - Camera matrices (view and projection)
     * - Whether gizmo is in orthographic mode
     * 
     * Interaction Flow:
     * 1. On mouse click: Request pick from GPU
     * 2. When pick result ready: Update selection
     * 3. If gizmo hovering: Don't select, let gizmo handle input
     * 4. When gizmo dragging: GizmoInteractionController fires events
     * 5. On drag end: Apply final transform, notify undo system
     * 
     * Does NOT manage:
     * - Entity creation/deletion
     * - Viewport camera movement
     * - UI synchronization (hierarchy, inspector)
     */
    class ViewportInteractionManager {
    public:
        ViewportInteractionManager();
        ~ViewportInteractionManager();

        /**
         * @brief Prepare viewport for a frame
         * 
         * Must be called once per frame before Update().
         * Sets up viewport bounds and camera matrices.
         * 
         * @param viewportPos Top-left corner in screen space
         * @param viewportSize Viewport dimensions in pixels
         * @param viewMatrix Camera view matrix
         * @param projMatrix Camera projection matrix
         * @param isPerspective Whether camera is perspective (true) or orthographic (false)
         */
        void PrepareFrame(
            const glm::vec2& viewportPos,
            const glm::vec2& viewportSize,
            const glm::mat4& viewMatrix,
            const glm::mat4& projMatrix,
            bool isPerspective);

        /**
         * @brief Update interaction state and apply changes to ECS
         * 
         * This is the main update method called each frame. It:
         * 1. Polls pending pick requests
         * 2. Updates gizmo interaction state
         * 3. Applies gizmo transforms to entities
         * 4. Handles new picks (updates selection)
         * 
         * @param world ECS world to query/modify
         * @param selectedEntityId Currently selected entity
         * @return Updated selected entity ID (may change due to picks)
         */
        uint32_t Update(ECS::World& world, uint32_t selectedEntityId);

        /**
         * @brief Render the gizmo for the selected entity
         * 
         * Must be called during ImGui rendering phase.
         * Typically called from ShowEditorWindows().
         * 
         * @param world ECS world (for reading entity transforms)
         * @param selectedEntityId Entity to show gizmo for
         */
        void RenderGizmo(ECS::World& world, uint32_t selectedEntityId);

        /**
         * @brief Reset interaction state (e.g., when selection changes externally)
         * 
         * Aborts any ongoing drag and returns to idle state.
         * Call this when selection changes from UI, not from picking.
         */
        void ResetInteraction();

        // ====================================================================
        // Gizmo Configuration
        // ====================================================================

        /**
         * @brief Set the gizmo operation mode
         */
        void SetGizmoOperation(GizmoRenderer::Operation op) {
            m_gizmo.SetOperation(op);
        }

        /**
         * @brief Set the gizmo coordinate mode
         */
        void SetGizmoMode(GizmoRenderer::Mode mode) {
            m_gizmo.SetMode(mode);
        }

        /**
         * @brief Get the current gizmo operation
         */
        GizmoRenderer::Operation GetGizmoOperation() const {
            return m_gizmo.GetOperation();
        }

        /**
         * @brief Get the current gizmo mode
         */
        GizmoRenderer::Mode GetGizmoMode() const {
            return m_gizmo.GetMode();
        }

        /**
         * @brief Check if gizmo should block input (being used or hovered)
         */
        bool ShouldBlockInput() const {
            return m_gizmo.ShouldBlockInput();
        }

        /**
         * @brief Request a pick at the given screen position
         * 
         * @param screenX Absolute screen X coordinate
         * @param screenY Absolute screen Y coordinate
         * @param rendererSystem Renderer system to query from
         */
        void RequestPick(float screenX, float screenY, ECS::RendererSystem* rendererSystem);

        // ====================================================================
        // Event Callbacks
        // ====================================================================

        /**
         * @brief Callback fired when entity selection changes
         * 
         * Parameters: (oldEntityId, newEntityId)
         * Viewport can use this to sync hierarchy, inspector, etc.
         */
        std::function<void(uint32_t, uint32_t)> OnSelectionChanged;

        /**
         * @brief Callback fired when a gizmo drag completes
         * 
         * Parameters: (entityId, initialTransform, finalTransform, delta)
         * Viewport/undo system can use this to create undo commands.
         */
        std::function<void(uint32_t, const CachedTransformState&, const CachedTransformState&, const TransformDelta&)> OnTransformChanged;

    private:
        // Components owned by this manager
        GizmoRenderer m_gizmo;
        GizmoInteractionController m_gizmoController;
        std::unique_ptr<PickingQueryManager> m_pickingManager;

        // Per-frame state
        glm::vec2 m_viewportPos{0.0f, 0.0f};
        glm::vec2 m_viewportSize{1920.0f, 1080.0f};
        glm::mat4 m_viewMatrix = glm::mat4(1.0f);
        glm::mat4 m_projMatrix = glm::mat4(1.0f);
        bool m_isPerspective = false;

        // Interaction state
        uint32_t m_selectedEntityId = 0;
        uint32_t m_pendingPickRequestId = 0;  // 0 = no pending request
        CachedTransformState m_dragInitialTransform;

        /**
         * @brief Handle a completed pick result
         * @param pickedEntityId Entity ID from pick, or 0 if nothing picked
         * @param shouldUpdateSelection Whether to update selection (false if dragging)
         * @return New selected entity ID
         */
        uint32_t _handlePickResult(uint32_t pickedEntityId, bool shouldUpdateSelection);

        /**
         * @brief Apply gizmo-computed transform to the entity
         */
        void _applyGizmoTransform(ECS::World& world, uint32_t entityId, const glm::mat4& transformMatrix);

        /**
         * @brief Extract LocalTransform from entity
         */
        CachedTransformState _getEntityTransform(ECS::World& world, uint32_t entityId) const;

        /**
         * @brief Set entity's LocalTransform from CachedTransformState
         */
        void _setEntityTransform(ECS::World& world, uint32_t entityId, const CachedTransformState& state);
    };

}  // namespace Editor

#endif  // VIEWPORT_INTERACTION_MANAGER_H
