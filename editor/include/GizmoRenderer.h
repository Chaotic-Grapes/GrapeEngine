/* Start Header *****************************************************************/
/*!
\file   GizmoRenderer.h
\author Samanta Leong (50%)
        Muhammad Nur Fadzly Bin Zulkifli (50%)
\par    s.leong@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\brief
Thin wrapper around ImGuizmo for rendering and computing transform changes.

This class encapsulates ImGuizmo rendering without any state management beyond
immediate render state. It computes transform outputs but does not apply them
to the world or manage undo. This separation allows GizmoRenderer to be tested
and reused independently of interaction logic.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GIZMO_RENDERER_H
#define GIZMO_RENDERER_H

#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

namespace Editor {

    /**
     * @brief Thin wrapper around ImGuizmo rendering
     * 
     * Responsibilities:
     * - Configure ImGuizmo viewport and projection
     * - Render gizmo handles
     * - Compute output transform from user interaction
     * - Expose ImGuizmo state (IsUsing, IsOver)
     * 
     * Does NOT:
     * - Apply transforms to ECS world
     * - Create undo commands
     * - Track interaction history
     * - Manage entity state
     */
    class GizmoRenderer {
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

        GizmoRenderer();
        ~GizmoRenderer();

        // ====================================================================
        // Configuration
        // ====================================================================

        /**
         * @brief Set the viewport region where gizmo is rendered
         * @param x Left edge in screen space
         * @param y Top edge in screen space
         * @param w Width in pixels
         * @param h Height in pixels
         */
        void SetViewport(float x, float y, float w, float h);

        /**
         * @brief Set the current gizmo operation mode
         */
        void SetOperation(Operation op);

        /**
         * @brief Set the current gizmo coordinate mode
         */
        void SetMode(Mode mode);

        /**
         * @brief Set whether camera is in perspective or orthographic mode
         */
        void SetPerspective(bool isPerspective);

        // ====================================================================
        // Rendering
        // ====================================================================

        /**
         * @brief Render the gizmo and compute output transform
         * 
         * This function:
         * 1. Calls ImGuizmo::BeginFrame() to set up ImGuizmo state
         * 2. Configures the gizmo viewport and projection
         * 3. Calls ImGuizmo::Manipulate() to render handles and compute output
         * 4. Returns the computed transform
         * 
         * @param viewMatrix Camera view matrix (row-major)
         * @param projMatrix Camera projection matrix (row-major)
         * @param inputTransform Input entity transform (will be read/modified by ImGuizmo)
         * @param outTransform [OUT] Computed transform from ImGuizmo manipulation
         * @return true if gizmo is currently being manipulated; false otherwise
         */
        bool Render(
            const glm::mat4& viewMatrix,
            const glm::mat4& projMatrix,
            const glm::mat4& inputTransform,
            glm::mat4& outTransform);

        // ====================================================================
        // State Queries
        // ====================================================================

        /**
         * @brief Check if gizmo is currently being interacted with (dragged)
         * @return true if user is actively manipulating the gizmo
         */
        virtual bool IsBeingUsed() const;

        /**
         * @brief Check if mouse is hovering over gizmo handles
         * @return true if mouse cursor is over any gizmo element
         */
        virtual bool IsMouseOver() const;

        /**
         * @brief Check if gizmo should consume input (used or hovered)
         * @return true if gizmo is active and should prevent other input handlers
         */
        virtual bool ShouldBlockInput() const;

        // ====================================================================
        // Accessors
        // ====================================================================

        Operation GetOperation() const { return m_operation; }
        Mode GetMode() const { return m_mode; }
        bool GetPerspective() const { return m_isPerspective; }

    private:
        Operation m_operation = Operation::Translate;
        Mode m_mode = Mode::Local;
        bool m_isPerspective = false;

        float m_viewportX = 0.0f;
        float m_viewportY = 0.0f;
        float m_viewportW = 1.0f;
        float m_viewportH = 1.0f;
    };

}  // namespace Editor

#endif  // GIZMO_RENDERER_H
