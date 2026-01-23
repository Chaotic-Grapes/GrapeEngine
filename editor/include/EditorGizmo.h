/* Start Header *****************************************************************/
/*!
\file   EditorGizmo.h
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   5th November 2025

\brief
Declares editor gizmo rendering and manipulation for 2D/3D entities.

This is an editor-only utility that uses ImGuizmo to render translation/rotation/
scale gizmos inside the viewport. The gizmo allows real-time manipulation of entity
transforms in ECS::World.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_GIZMO_H
#define EDITOR_GIZMO_H

#include "ecs/World.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include "math/Vector3D.h"
#include "math/Quaternion.h"

namespace Editor {

    /**
     * @brief Encapsulates the state of an ongoing gizmo drag operation
     * 
     * Uses a state machine to track drag lifecycle and only emits events
     * at appropriate state transitions. This isolates drag state into a single,
     * reusable context rather than scattered static variables.
     */
    class GizmoDragContext {
    public:
        enum class State { Idle, Started, InProgress, Ended };

        GizmoDragContext() = default;

        /// Reset to idle state (e.g., when entity selection changes)
        void Reset() {
            m_state = State::Idle;
            m_draggingEntityID = ECS::Entity::NPOS32;
        }

        /// Update state machine based on ImGuizmo feedback
        void Update(bool isUsing, uint32_t selectedEntityID) {
            m_previousState = m_state;
            
            switch (m_state) {
                case State::Idle:
                    if (isUsing) {
                        m_state = State::Started;
                        m_draggingEntityID = selectedEntityID;
                    }
                    break;
                case State::Started:
                    m_state = State::InProgress;
                    break;
                case State::InProgress:
                    if (!isUsing) {
                        m_state = State::Ended;
                    }
                    break;
                case State::Ended:
                    m_state = State::Idle;
                    m_draggingEntityID = ECS::Entity::NPOS32;
                    break;
            }
        }

        /// Check if we transitioned to a particular state this frame
        bool JustEntered(State state) const { return m_previousState != state && m_state == state; }

        /// Check if we transitioned from a particular state this frame
        bool JustLeft(State state) const { return m_previousState == state && m_state != state; }

        State GetState() const { return m_state; }
        uint32_t GetEntityID() const { return m_draggingEntityID; }

        // Capture and retrieve drag snapshot
        void CaptureInitialTransform(const Vector3D& pos, const Quaternion& rot, const Vector3D& scale) {
            m_initialPosition = pos;
            m_initialRotation = rot;
            m_initialScale = scale;
        }

        Vector3D GetInitialPosition() const { return m_initialPosition; }
        Quaternion GetInitialRotation() const { return m_initialRotation; }
        Vector3D GetInitialScale() const { return m_initialScale; }

    private:
        State m_state = State::Idle;
        State m_previousState = State::Idle;
        uint32_t m_draggingEntityID = ECS::Entity::NPOS32;

        // Initial transform snapshot at start of drag
        Vector3D m_initialPosition;
        Quaternion m_initialRotation;
        Vector3D m_initialScale;
    };

    /**
     * @brief Editor utility for rendering and manipulating entity transforms via gizmos
     */
    class EditorGizmo {
    public:
        enum class Operation {
            Translate = ImGuizmo::TRANSLATE,
            Rotate = ImGuizmo::ROTATE,
            Scale = ImGuizmo::SCALE
        };

        enum class Mode {
            Local = ImGuizmo::LOCAL,
            World = ImGuizmo::WORLD
        };

        /**
         * @brief Begin frame processing - capture initial transform state
         * Call this at the start of the frame before rendering
         */
        void BeginFrame(ECS::World& world, uint32_t selectedEntityID);

        /**
         * @brief End frame processing - finalize transform changes and create undo commands
         * Call this at the end of the frame after rendering
         */
        void EndFrame();

        /**
         * @brief Draws and applies the editor gizmo for the selected entity
         * 
         * @param world Reference to the ECS world
         * @param selectedEntityID ID of the currently selected entity
         * @param viewMatrix View matrix from camera
         * @param projMatrix Projection matrix from camera
         * @param drawPosX Viewport draw X position
         * @param drawPosY Viewport draw Y position
         * @param drawSizeX Viewport width
         * @param drawSizeY Viewport height
         * @param isPerspective Whether camera is in perspective mode
         * @return true if gizmo was manipulated this frame
         */
        bool DrawGizmo(
            ECS::World& world,
            uint32_t selectedEntityID,
            const float* viewMatrix,
            const float* projMatrix,
            float drawPosX,
            float drawPosY,
            float drawSizeX,
            float drawSizeY,
            bool isPerspective = false
        );

        /**
         * @brief Set the current gizmo operation mode
         */
        static void SetOperation(Operation op) { s_currentOperation = op; }

        /**
         * @brief Set the current gizmo coordinate mode
         */
        static void SetMode(Mode mode) { s_currentMode = mode; }

        /**
         * @brief Get the current gizmo operation
         */
        static Operation GetOperation() { return s_currentOperation; }

        /**
         * @brief Get the current gizmo mode
         */
        static Mode GetMode() { return s_currentMode; }

        /**
         * @brief Check if gizmo is currently being interacted with (dragged)
         * @return true if user is actively manipulating the gizmo
         */
        static bool IsBeingUsed() { return ImGuizmo::IsUsing(); }

        /**
         * @brief Check if mouse is hovering over gizmo handles
         * @return true if mouse is over any gizmo element
         */
        static bool IsMouseOverGizmo() { return ImGuizmo::IsOver(); }

        /**
         * @brief Check if gizmo should block input (either being used or hovered)
         * @return true if gizmo is active and should prevent other input
         */
        static bool ShouldBlockInput() { return ImGuizmo::IsUsing() || ImGuizmo::IsOver(); }

        /**
         * @brief Check if entity selection changes should be blocked
         * @return true if gizmo is actively being manipulated and selection should not change
         */
        static bool ShouldBlockSelection() { return ImGuizmo::IsUsing(); }

        /**
         * @brief Set the undo system for creating transform commands
         */
        void SetUndoSystem(class UndoSystem* undoSystem) { m_undoSystem = undoSystem; }

    private:
        static Operation s_currentOperation;
        static Mode s_currentMode;
        
        // Instance-based drag context (non-static)
        GizmoDragContext m_dragContext;
        
        // Undo system for creating transform change commands
        class UndoSystem* m_undoSystem = nullptr;
    };

}

#endif
