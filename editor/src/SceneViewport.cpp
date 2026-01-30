/* Start Header *****************************************************************/
/*!
\file   SceneViewport.cpp
\author Samantha Leong (45%)
        Foo Rui Qin    (45%)
        Muhammad Nur Fadzly Bin Zulkifli (10%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   3rd November 2025
\brief
Implementation of SceneViewport class for the editor scene view with editor camera,
entity selection, gizmo manipulation via ViewportInteractionManager.

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
#include "SelectionOutlineRenderer.h"
#include "EditorCamera.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "services/Input.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <scene/SceneManager.h>
#include "ecs/systems/RendererSystem.h"  
#include "graphics/RenderGraph.hpp"
#include "services/TimeSystem.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
void SceneViewport::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
                                ECS::World* world, Scenes::SceneManager* sceneManager) {
    BaseViewport::Initialize(mainFont, boldFont, symbolsFont, world, sceneManager);
    
    // Set viewport type to Scene
    SetViewportType(ViewportType::Scene);
}

SceneViewport::~SceneViewport() {
    // Cleanup handled by base class and RAII members
}

void SceneViewport::BeginFrame() {
    // Only the scene viewport uses the editor camera input handling
    if (m_editorCamera) {
        m_editorCamera->BeginFrame();
    }
}

void SceneViewport::EndFrame() {
    BaseViewport::EndFrame();
}

// -------------------------------------------------------------------------
// Update
// -------------------------------------------------------------------------
void SceneViewport::HandleInWorldInteraction() {
    if (!HasValidWorld()) return;

    // Toggle FPS overlay in the Scene viewport (editor-only)
    if (Input::IsKeyPressed(KEY_F)) {
        m_showSceneFpsOverlay = !m_showSceneFpsOverlay;
    }

    // Update viewport interaction manager (gizmo, picking, selection, transforms)
    uint32_t newSelectedEntity = m_interactionMgr.Update(*m_world, m_selectedEntity.Index);
    
    // If selection changed due to picking, update our selected entity
    if (newSelectedEntity != m_selectedEntity.Index) {
        SetSelectedEntity(newSelectedEntity);
        // Publish selection change message
        Messaging::MessageSystem::Broadcast(Messaging::EntitySelectionRequested(newSelectedEntity, false));
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
    
    // Get current gizmo operation and mode from interaction manager
    auto curOp = m_interactionMgr.GetGizmoOperation();
    auto curMode = m_interactionMgr.GetGizmoMode();

    // Operation buttons
    auto opButton = [&](const char* label, Editor::GizmoRenderer::Operation op) {
        const bool active = (curOp == op);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label, ImVec2(0, btnH))) {
            m_interactionMgr.SetGizmoOperation(op);
        }
        if (active) ImGui::PopStyleColor();
    };

    ImGui::PushID("GizmoOps");
    opButton("Translate", Editor::GizmoRenderer::Operation::Translate);
    ImGui::SameLine();
    opButton("Rotate", Editor::GizmoRenderer::Operation::Rotate);
    ImGui::SameLine();
    opButton("Scale", Editor::GizmoRenderer::Operation::Scale);
    ImGui::PopID();

    ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();

    // Mode buttons (Local / World) with visual feedback
    bool isLocal = (curMode == Editor::GizmoRenderer::Mode::Local);
    
    // Local button
    if (isLocal) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button("Local", ImVec2(0, btnH))) {
        m_interactionMgr.SetGizmoMode(Editor::GizmoRenderer::Mode::Local);
    }
    if (isLocal) ImGui::PopStyleColor();
    
    ImGui::SameLine();
    
    // World button
    if (!isLocal) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button("World", ImVec2(0, btnH))) {
        m_interactionMgr.SetGizmoMode(Editor::GizmoRenderer::Mode::World);
    }
    if (!isLocal) ImGui::PopStyleColor();

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
            ImVec2 viewportScreenPos = ImGui::GetItemRectMin();

            // Handle mouse click picking on the viewport image
            // Don't pick if gizmo is being used or hovered
            if (isSceneImageHovered && Input::IsMousePressed(MOUSE_LEFT)) {
                // Check if gizmo should block input
                bool gizmoBlocking = m_interactionMgr.ShouldBlockInput();
                
                if (!gizmoBlocking) {
                    double mx = 0, my = 0;
                    Input::GetMousePosition(mx, my);
                    
                    // Request a pick via the interaction manager's picking manager
                    // We need access to renderer system to make the request
                    auto* rs = _getRendererSystem();
                    if (rs && m_world) {
                        m_interactionMgr.RequestPick(
                            static_cast<float>(mx),
                            static_cast<float>(my),
                            rs);
                    }
                }
            }

            // Prepare interaction manager with current viewport state
            // IMPORTANT: Use viewportScreenPos which is the actual rendered image position
            if (m_editorCamera) {
                auto* camera = m_editorCamera->GetCamera();
                if (camera) {
                    glm::mat4 view = camera->GetViewMatrix();
                    glm::mat4 proj = camera->GetProjectionMatrix();
                    
                    // Prepare interaction manager frame (must be called before Update in HandleInWorldInteraction)
                    // Note: This is called here after camera is updated
                    m_interactionMgr.PrepareFrame(
                        glm::vec2(viewportScreenPos.x, viewportScreenPos.y),
                        glm::vec2(size.x, size.y),
                        view,
                        proj,
                        camera->UsePerspective
                    );

                    // Draw selection outline around selected entity
                    if (!m_selectedEntity.IsNull()) {
                        Editor::SelectionOutlineRenderer::RenderOutline(
                            *m_world,
                            m_selectedEntity.Index,
                            rendererSystem->GetRenderer(),
                            rendererSystem->GetShader(),
                            proj * view,
                            camera->OrthoSize,
                            size.x,
                            size.y
                        );
                    }

                    // Submit collider debug visualization for selected entity
                    if (!m_selectedEntity.IsNull() && m_world) {
                        const glm::vec4 colliderColor{ 1.0f, 0.64f, 0.0f, 0.45f }; // Orange with some transparency
                        rendererSystem->SubmitColliderDebugDraw(*m_world, m_selectedEntity.Index, colliderColor);
                    }

                    // Render gizmo via interaction manager
                    if (m_world && m_selectedEntity.Index != ECS::Entity::NPOS32) {
                        m_interactionMgr.RenderGizmo(*m_world, m_selectedEntity.Index);
                    }
                }
            }

            // Draw FPS overlay if enabled
            if (m_showSceneFpsOverlay) {
                _drawFpsOverlay(viewportScreenPos, size);
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
