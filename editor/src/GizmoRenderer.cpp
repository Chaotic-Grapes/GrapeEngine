/* Start Header *****************************************************************/
/*!
\file   GizmoRenderer.cpp
\author Samantha Leong (50%)
        Muhammad Nur Fadzly Bin Zulkifli (50%)
\par    s.leong@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\brief
Implementation of GizmoRenderer ImGuizmo wrapper.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "GizmoRenderer.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "core/Logger.h"

namespace Editor {

    GizmoRenderer::GizmoRenderer()
        : m_operation(Operation::Translate)
        , m_mode(Mode::Local)
        , m_isPerspective(false)
        , m_viewportX(0.0f)
        , m_viewportY(0.0f)
        , m_viewportW(1.0f)
        , m_viewportH(1.0f) {
    }

    GizmoRenderer::~GizmoRenderer() {
    }

    // Set the screen-space rectangle that ImGuizmo will draw within
    void GizmoRenderer::SetViewport(float x, float y, float w, float h) {
        m_viewportX = x;
        m_viewportY = y;
        m_viewportW = w;
        m_viewportH = h;
    }

    // Set the active gizmo operation (Translate, Rotate, or Scale)
    void GizmoRenderer::SetOperation(Operation op) {
        m_operation = op;
    }

    // Set the transform space (Local or World) used when manipulating the gizmo
    void GizmoRenderer::SetMode(Mode mode) {
        m_mode = mode;
    }

    // Switch between orthographic (false) and perspective (true) projection mode for gizmo rendering
    void GizmoRenderer::SetPerspective(bool isPerspective) {
        m_isPerspective = isPerspective;
    }

    // Submit the gizmo to ImGuizmo using the given view/projection matrices; returns true if the user manipulated it this frame
    bool GizmoRenderer::Render(
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        const glm::mat4& inputTransform,
        glm::mat4& outTransform,
        const float* snap,
        glm::mat4* outDeltaTransform) {

        // Validate viewport size FIRST
        if (m_viewportW <= 0 || m_viewportH <= 0) {
            LOG_WARNING("GizmoRenderer::Render - Invalid viewport: " << m_viewportW << "x" << m_viewportH);
            return false;  // Can't render with invalid viewport
        }

        // Check if we're in a valid ImGui context
        if (!ImGui::GetCurrentContext()) {
            LOG_ERROR("GizmoRenderer::Render - No ImGui context!");
            return false;
        }

        // Initialize ImGuizmo state
        ImGuizmo::BeginFrame();

        // Enable the gizmo explicitly
        ImGuizmo::Enable(true);

        // Configure orthographic/perspective mode
        ImGuizmo::SetOrthographic(!m_isPerspective);

        // Set the draw list for ImGui integration (use current window drawlist)
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (!drawList) {
            LOG_WARNING("GizmoRenderer::Render - No ImGui draw list available!");
            return false;
        }

        ImGuizmo::SetDrawlist(drawList);

        // SetRect must use the actual viewport coordinates (image rendering area)
        // NOT the full window coordinates (which include toolbar)
        ImGuizmo::SetRect(m_viewportX, m_viewportY, m_viewportW, m_viewportH);

        // Convert operation and mode to ImGuizmo types (use 2D constraints in ortho)
        ImGuizmo::OPERATION imguizmoOp = static_cast<ImGuizmo::OPERATION>(static_cast<int>(m_operation));
        if (!m_isPerspective) {
            if (m_operation == Operation::Translate) {
                imguizmoOp = static_cast<ImGuizmo::OPERATION>(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y);
            } else if (m_operation == Operation::Rotate) {
                imguizmoOp = ImGuizmo::ROTATE_Z;
            } else if (m_operation == Operation::Scale) {
                imguizmoOp = static_cast<ImGuizmo::OPERATION>(ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y);
            }
        }
        ImGuizmo::MODE imguizmoMode = static_cast<ImGuizmo::MODE>(static_cast<int>(m_mode));

        // Copy input transform to output (ImGuizmo modifies in-place)
        outTransform = inputTransform;

        // Call ImGuizmo::Manipulate and get whether it's being used
        // Note: ImGuizmo accepts both column-major (glm) and row-major matrices
        // Passing glm matrices directly (column-major)
        
        // Use explicit column-major float array for ImGuizmo input/output
        float matrixArr[16];
        const float* src = glm::value_ptr(outTransform);
        for (int i = 0; i < 16; ++i) matrixArr[i] = src[i];

        // Forward optional snap increments to ImGuizmo (null disables snapping).
        bool isManipulating = ImGuizmo::Manipulate(
            glm::value_ptr(viewMatrix),
            glm::value_ptr(projMatrix),
            imguizmoOp,
            imguizmoMode,
            matrixArr,
            nullptr,
            snap
        );
        
        // Copy back modified matrix into outTransform
        if (isManipulating) {
            float* dst = glm::value_ptr(outTransform);
            for (int i = 0; i < 16; ++i) dst[i] = matrixArr[i];
        }

        // If delta transform requested, initialize to identity
        // ImGuizmo doesn't directly expose delta matrices, but we can compute them in the caller
        if (outDeltaTransform && isManipulating) {
            *outDeltaTransform = glm::mat4(1.0f);
        }

        return isManipulating;
    }

    // Return true if the user is actively manipulating the gizmo this frame
    bool GizmoRenderer::IsBeingUsed() const {
        return ImGuizmo::IsUsing();
    }

    // Return true if the mouse cursor is hovering over any part of the gizmo
    bool GizmoRenderer::IsMouseOver() const {
        return ImGuizmo::IsOver();
    }

    // Return true if the gizmo is being used or hovered, indicating input should not pass through to the viewport
    bool GizmoRenderer::ShouldBlockInput() const {
        return ImGuizmo::IsUsing() || ImGuizmo::IsOver();
    }

}  // namespace Editor
