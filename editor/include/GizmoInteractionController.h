/* Start Header *****************************************************************/
/*!
\file   GizmoInteractionController.h
\author Samanta Leong (50%)
        Muhammad Nur Fadzly Bin Zulkifli (50%)
\par    s.leong@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\brief
Encapsulates gizmo drag interaction state machine and transform snapshots.

This controller tracks the lifecycle of a gizmo interaction:
    IDLE → HOVERING → DRAGGING → RELEASING → IDLE

It manages:
- State transitions based on ImGuizmo feedback
- Initial and final transform snapshots for undo
- Event callbacks (OnDragStart, OnDragEnd) for external observers

This is a pure interaction observer - it does NOT modify ECS or integrate with undo.
The viewport/manager connects callbacks to undo/ECS systems as needed.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GIZMO_INTERACTION_CONTROLLER_H
#define GIZMO_INTERACTION_CONTROLLER_H

#include "TransformState.h"
#include <cstdint>
#include <functional>

namespace Editor {

    // Forward declaration
    class GizmoRenderer;

    /**
     * @brief Manages the state machine and lifecycle of gizmo interactions
     * 
     * Tracks transitions between interaction states and captures transform
     * snapshots at appropriate boundaries. Emits events to external observers
     * (e.g., undo system, UI feedback).
     * 
     * State Machine:
     *   IDLE: No interaction, gizmo not being used
     *   HOVERING: Mouse over gizmo handle, ready to drag
     *   DRAGGING: Actively manipulating gizmo
     *   RELEASING: Just released, final transform captured
     *   (returns to IDLE after one frame)
     * 
     * Snapshots:
     *   - Initial: Captured when transitioning to DRAGGING
     *   - Final: Captured when transitioning to RELEASING
     *   - Delta: Computed from initial → final for undo
     */
    class GizmoInteractionController {
    public:
        enum class State {
            Idle,       // No interaction
            Hovering,   // Mouse over gizmo
            Dragging,   // Actively manipulating
            Releasing   // Just released, preparing to return to idle
        };

        GizmoInteractionController() = default;
        ~GizmoInteractionController() = default;

        /**
         * @brief Update the interaction state based on gizmo feedback
         * 
         * Call this once per frame with the current gizmo state.
         * This method:
         * 1. Reads ImGuizmo state from GizmoRenderer
         * 2. Updates internal state machine
         * 3. Captures snapshots at transitions
         * 4. Fires events when appropriate
         * 
         * @param gizmo The GizmoRenderer providing feedback
         * @param activeEntityId ID of the entity being manipulated (0 if none)
         */
        void Update(const GizmoRenderer& gizmo, uint32_t activeEntityId);

        /**
         * @brief Reset to idle state (e.g., when entity selection changes)
         * 
         * This forcefully aborts any ongoing interaction and returns to idle.
         * No events are fired. Use this when the selection changes mid-drag.
         */
        void Reset();

        // ====================================================================
        // State Queries
        // ====================================================================

        /**
         * @brief Get the current interaction state
         */
        State GetState() const { return m_state; }

        /**
         * @brief Check if we just transitioned into DRAGGING this frame
         */
        bool JustStartedDrag() const { return m_previousState == State::Hovering && m_state == State::Dragging; }

        /**
         * @brief Check if we just transitioned out of DRAGGING this frame
         */
        bool JustEndedDrag() const { return m_previousState == State::Dragging && m_state == State::Releasing; }

        /**
         * @brief Check if currently dragging
         */
        bool IsDragging() const { return m_state == State::Dragging; }

        /**
         * @brief Check if hovering over gizmo
         */
        bool IsHovering() const { return m_state == State::Hovering; }

        // ====================================================================
        // Transform Snapshots
        // ====================================================================

        /**
         * @brief Get the transform captured at drag start
         * 
         * Valid during and after drag. Returns zero-initialized state if never dragged.
         */
        const CachedTransformState& GetInitialTransform() const { return m_initialTransform; }

        /**
         * @brief Get the transform captured at drag end
         * 
         * Valid only after drag completes. Returns zero-initialized state if never dragged.
         */
        const CachedTransformState& GetFinalTransform() const { return m_finalTransform; }

        /**
         * @brief Get the entity ID that was being dragged
         * 
         * Returns 0 if no drag in progress or completed.
         */
        uint32_t GetDraggedEntityId() const { return m_draggedEntityId; }

        /**
         * @brief Compute the transform delta from initial to final
         * 
         * @return TransformDelta representing the change
         */
        TransformDelta ComputeDelta() const;

        // ====================================================================
        // Event Callbacks
        // ====================================================================

        /**
         * @brief Callback fired when drag starts (transition to DRAGGING)
         * 
         * Parameters: (entityId, delta)
         * Note: Delta is zero at start; use GetInitialTransform() instead.
         */
        std::function<void(uint32_t entityId, const TransformDelta&)> OnDragStart;

        /**
         * @brief Callback fired when drag ends (transition to RELEASING)
         * 
         * Parameters: (entityId, delta from initial to final)
         * This is the appropriate place to create an undo command.
         */
        std::function<void(uint32_t entityId, const TransformDelta&)> OnDragEnd;

    private:
        State m_state = State::Idle;
        State m_previousState = State::Idle;
        uint32_t m_draggedEntityId = 0;

        // Transform snapshots
        CachedTransformState m_initialTransform;  // Captured at drag start
        CachedTransformState m_finalTransform;    // Captured at drag end

        /**
         * @brief Fire OnDragStart event if callback is set
         */
        void _fireOnDragStart();

        /**
         * @brief Fire OnDragEnd event if callback is set
         */
        void _fireOnDragEnd();
    };

}  // namespace Editor

#endif  // GIZMO_INTERACTION_CONTROLLER_H
