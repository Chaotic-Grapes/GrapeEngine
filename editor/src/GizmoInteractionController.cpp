/* Start Header *****************************************************************/
/*!
\file   GizmoInteractionController.cpp
\author Samantha Leong (50%)
        Muhammad Nur Fadzly Bin Zulkifli (50%)
\par    s.leong@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\brief
Implements GizmoInteractionController state machine and event firing.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "GizmoInteractionController.h"
#include "GizmoRenderer.h"

namespace Editor {

    TransformDelta GizmoInteractionController::ComputeDelta() const {
        return m_initialTransform.ComputeDelta(m_finalTransform);
    }

    void GizmoInteractionController::Update(const GizmoRenderer& gizmo, uint32_t activeEntityId, const CachedTransformState& currentTransform) {
        // Store previous state to detect transitions
        m_previousState = m_state;

        // Check gizmo feedback
        bool gizmoBeingUsed = gizmo.IsBeingUsed();
        bool gizmoMouseOver = gizmo.IsMouseOver();

        // State machine
        switch (m_state) {
            case State::Idle:
                // Transition to Hovering if mouse is over gizmo
                if (gizmoMouseOver) {
                    m_state = State::Hovering;
                    m_draggedEntityId = activeEntityId;
                }
                break;

            case State::Hovering:
                // Transition to Dragging if gizmo is actively being used
                if (gizmoBeingUsed) {
                    m_state = State::Dragging;
                    m_draggedEntityId = activeEntityId;
                    m_initialTransform = currentTransform;
                    _fireOnDragStart();
                }
                // Return to Idle if mouse leaves gizmo
                else if (!gizmoMouseOver) {
                    m_state = State::Idle;
                    m_draggedEntityId = 0;
                }
                break;

            case State::Dragging:
                // Stay in Dragging while gizmo is being used
                if (!gizmoBeingUsed) {
                    // Transition to Releasing when mouse released
                    m_state = State::Releasing;
                    m_finalTransform = currentTransform;
                    _fireOnDragEnd();
                }
                break;

            case State::Releasing:
                // Automatically return to Idle (one-frame delay for event processing)
                m_state = State::Idle;
                m_draggedEntityId = 0;
                break;
        }
    }

    void GizmoInteractionController::Reset() {
        m_state = State::Idle;
        m_previousState = State::Idle;
        m_draggedEntityId = 0;
        m_initialTransform = CachedTransformState();
        m_finalTransform = CachedTransformState();
    }

    void GizmoInteractionController::_fireOnDragStart() {
        if (OnDragStart) {
            // At drag start, delta is zero (no change yet)
            TransformDelta zeroDelta;
            OnDragStart(m_draggedEntityId, zeroDelta);
        }
    }

    void GizmoInteractionController::_fireOnDragEnd() {
        if (OnDragEnd) {
            TransformDelta delta = ComputeDelta();
            OnDragEnd(m_draggedEntityId, delta);
        }
    }

}  // namespace Editor
