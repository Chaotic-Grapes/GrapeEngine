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
    
    // --- Toolbar (Translate / Rotate / Scale + Local/World) -----------------
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));

    const float btnH = ImGui::GetFrameHeight();
    // Operation buttons
    auto curOp = Editor::EditorGizmo::GetOperation();

    auto opButton = [&](const char* label, Editor::EditorGizmo::Operation op) {
        const bool active = (curOp == op);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label, ImVec2(0, btnH))) {
            Editor::EditorGizmo::SetOperation(op);
        }
        if (active) ImGui::PopStyleColor();
    };

    ImGui::PushID("GizmoOps");
    opButton("Translate", Editor::EditorGizmo::Operation::Translate);
    ImGui::SameLine();
    opButton("Rotate", Editor::EditorGizmo::Operation::Rotate);
    ImGui::SameLine();
    opButton("Scale", Editor::EditorGizmo::Operation::Scale);
    ImGui::PopID();

    ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();

    // Mode toggle (Local / World)
    auto curMode = Editor::EditorGizmo::GetMode();
    if (curMode == Editor::EditorGizmo::Mode::Local) {
        if (ImGui::Button("Local", ImVec2(0, btnH))) Editor::EditorGizmo::SetMode(Editor::EditorGizmo::Mode::Local);
        ImGui::SameLine();
        if (ImGui::Button("World", ImVec2(0, btnH))) Editor::EditorGizmo::SetMode(Editor::EditorGizmo::Mode::World);
    }
    else {
        if (ImGui::Button("Local", ImVec2(0, btnH))) Editor::EditorGizmo::SetMode(Editor::EditorGizmo::Mode::Local);
        ImGui::SameLine();
        if (ImGui::Button("World", ImVec2(0, btnH))) Editor::EditorGizmo::SetMode(Editor::EditorGizmo::Mode::World);
    }

    ImGui::PopStyleVar();
    ImGui::EndGroup();

    ImGui::Separator();

    // Get viewport size and position (remaining area after toolbar)
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
            // Use IsMousePressed to detect a single click event (edge-triggered)
            if (ImGui::IsItemHovered() && Input::IsMousePressed(MOUSE_LEFT)) {
                double mx = 0, my = 0;
                Input::GetMousePosition(mx, my);

                // Enqueue async pick via renderer and store returned request id
                // Only enqueue if no existing pending request to avoid spamming
                if (m_pendingPickRequestId == 0) {
                    uint32_t req = Editor::ViewportPicking::RequestAsyncPick(
                    static_cast<float>(mx), static_cast<float>(my),
                    glm::vec2(gizmoPos.x, gizmoPos.y), glm::vec2(size.x, size.y),
                    rendererSystem
                    );

                    if (req != 0) {
                        m_pendingPickRequestId = req;
                        LOG_DEBUG("[SceneViewport] Enqueued pick request: " << req << " at (" << mx << ", " << my << ") viewport=(" << gizmoPos.x << "," << gizmoPos.y << "," << size.x << "," << size.y << ")");
                    }
                }
            }

            // Poll for async pick result (non-blocking). If a result is ready
            // resolve selection and notify callbacks.
            if (m_pendingPickRequestId != 0) {
                uint32_t pickedEntity = 0;
                if (Editor::ViewportPicking::TryGetAsyncPickResult(m_pendingPickRequestId, pickedEntity, rendererSystem)) {
                    m_pendingPickRequestId = 0; // consumed
                    if (pickedEntity != 0 && pickedEntity != ECS::Entity::NPOS32) {
                        SetSelectedEntity(pickedEntity);
                    }
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
                    // Draw wireframe outline around selected entity
                    Editor::SelectionOutlineRenderer::RenderOutline(
                        *m_world,
                        m_selectedEntityId,
                        rendererSystem,
                        camera->OrthoSize,
                        size.y
                    );

                    // Draw gizmo for transform manipulation
                    glm::mat4 view = camera->GetViewMatrix();
                    glm::mat4 proj = camera->GetProjectionMatrix();
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

    // Update mouse-down latch so clicks only trigger once per press
    m_wasMouseDownLastFrame = Input::IsMouseDown(MOUSE_LEFT);
}
