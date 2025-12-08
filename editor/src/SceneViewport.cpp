/* Start Header *****************************************************************/
/*!
\file   SceneViewport.cpp
\author Samantha Leong (50%)
        Foo Rui Qin    (45%)
        Muhammad Nur Fadzly Bin Zulkifli (5%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   3rd November 2025
\brief
Implementation of SceneViewport class for the editor scene view with editor camera,
entity selection, gizmo manipulation, and drag-to-move functionality.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include "SceneViewport.h"
#include "EditorGizmo.h"
#include "SelectionOutlineRenderer.h"
#include "EditorCamera.hpp"
#include "ViewportPicking.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "services/Input.h"
#include <imgui.h>
#include "core/Application.h"
#include "ecs/systems/RendererSystem.h"  
#include "graphics/RenderGraph.hpp"
#include "services/TimeSystem.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"

// -------------------------------------------------------------------------
// Update
// -------------------------------------------------------------------------
void SceneViewport::HandleInWorldInteraction() {
    if (!HasValidWorld()) return;

    if (m_undoSystem) {
        m_undoSystem->Update();
    }

    // Toggle FPS overlay in the Scene viewport (editor-only)
    if (Input::IsKeyPressed(KEY_F)) {
        m_showSceneFpsOverlay = !m_showSceneFpsOverlay;
    }

    // Editor camera input is controlled via SetViewportFocused() in Scene window
    // Selection is handled by ViewportPicking utility in editor
    // Viewport maintains its own m_selectedEntityId state
    
    // Handle editor-specific entity manipulation (drag-to-move)
    // This only works when viewport is hovered
    if (m_isViewportHovered) {
        _handleEntityDragToMove();
    }
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------
void SceneViewport::ShowEditorWindows() {
    _renderViewport();
}

// -------------------------------------------------------------------------
// Private Rendering Implementation
// -------------------------------------------------------------------------
void SceneViewport::_renderViewport() {
    // Begin scene viewport window
    ImGui::Begin("Scene", nullptr);

    auto* rendererSystem = _getRendererSystem();
    
    // Get viewport size and position
    const auto size = ImGui::GetContentRegionAvail();
    const auto pos = ImGui::GetCursorScreenPos();
    m_sceneDrawPos = pos;
    m_sceneDrawSize = size;

    // Broadcast viewport resize event for camera aspect ratio updates
    Messaging::MessageSystem::Broadcast(Messaging::ViewportResized(size.x, size.y));

    if (rendererSystem) {
        // Configure renderer to use EDITOR camera for Scene viewport
        rendererSystem->SetCamera(m_editorCamera->GetCamera());

        // Render camera frustum overlay if enabled
        if (m_world && m_showCameraFrustum) {
            _renderCameraFrustum();
        }

        auto* rg = rendererSystem->GetRenderGraph();
        if (rg) {
            ResourceAccessor acc(rg);
            const uint32_t textureId = acc.GetTexture("LDR");
            if (textureId > 0) {
                ImGui::Image(textureId, size, ImVec2(0, 1), ImVec2(1, 0));
            }

            // Check if image is hovered AFTER drawing it
            bool isSceneImageHovered = ImGui::IsItemHovered();

            // Update m_isViewportHovered only when image is hovered
            m_isViewportHovered = isSceneImageHovered;

            // Update editor camera when image is hovered
            if (m_editorCamera) {
                m_editorCamera->SetViewportFocused(isSceneImageHovered);
                m_editorCamera->Update(static_cast<float>(TimeSystem::Instance().GetDeltaTime()));
            }

            // Get the drawing position of the rendered image
            ImVec2 gizmoPos = ImGui::GetItemRectMin();

            // Handle mouse click picking on the Scene viewport image
            if (ImGui::IsItemHovered() && Input::IsMouseDown(MOUSE_LEFT) && !m_wasMouseDownLastFrame) {
                double mx = 0, my = 0;
                Input::GetMousePosition(mx, my);

                // Call picker with absolute mouse position and viewport rect
                uint32_t picked = Editor::ViewportPicking::PickEntityAtScreenPosition(
                    static_cast<float>(mx), static_cast<float>(my),
                    glm::vec2(gizmoPos.x, gizmoPos.y), glm::vec2(size.x, size.y),
                    rendererSystem
                );

                if (picked != 0) {
                    SetSelectedEntity(picked);
                }

                if (m_onSelectionChanged) {
                    m_onSelectionChanged(GetSelectedEntityId());
                }
            }

            // Draw FPS overlay if enabled (before gizmo so it appears behind)
            if (m_showSceneFpsOverlay) {
                _drawFpsOverlay(gizmoPos, size);
            }

            // Draw selection outline and gizmo for selected entity
            if (m_selectedEntityId != 0 && m_editorCamera) {
                auto* camera = m_editorCamera->GetCamera();
                if (camera) {
                    glm::mat4 view = camera->GetViewMatrix();
                    glm::mat4 proj = camera->GetProjectionMatrix();
                    const glm::mat4 viewProj = proj * view;

                    // Draw wireframe outline around selected entity
                    Editor::SelectionOutlineRenderer::RenderOutline(
                        *m_world,
                        m_selectedEntityId,
                        rendererSystem->GetRenderer(),
                        rendererSystem->GetShader(),
                        viewProj,
                        camera->OrthoSize,
                        size.y
                    );

                    // Draw gizmo for transform manipulation
                    Editor::EditorGizmo::DrawGizmo(
                        *m_world,
                        m_selectedEntityId,
                        glm::value_ptr(view),
                        glm::value_ptr(proj),
                        gizmoPos.x, gizmoPos.y,
                        size.x, size.y,
                        false  // isPerspective
                    );
                }
            }
        }
    }
    else {
        ImGui::TextDisabled("No renderer available");
        
        // Still allow camera movement even if renderer isn't ready
        m_isViewportHovered = false;
        if (m_editorCamera) {
            m_editorCamera->SetViewportFocused(false);
        }
    }

    ImGui::End();
}
