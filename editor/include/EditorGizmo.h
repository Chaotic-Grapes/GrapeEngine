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

namespace Editor {

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
        static bool DrawGizmo(
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

    private:
        static Operation s_currentOperation;
        static Mode s_currentMode;
    };

}

#endif
