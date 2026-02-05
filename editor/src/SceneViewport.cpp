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
#include "EditorStyle.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
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
#include "TilePalettePanel.h"
#include "graphics/Viewport.hpp"

namespace {
    constexpr const char* kSceneViewportName = "Scene";
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
void SceneViewport::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
                                ECS::World* world, Scenes::SceneManager* sceneManager) {
    BaseViewport::Initialize(mainFont, boldFont, symbolsFont, world, sceneManager);
    
    // Set viewport type to Scene
    SetViewportType(ViewportType::Scene);
    SetViewportName(kSceneViewportName);
    Graphics::ViewportManager::Create(kSceneViewportName, m_editorCamera ? m_editorCamera->GetCamera() : nullptr, 1, 1);
}

SceneViewport::~SceneViewport() {
    // Cleanup handled by base class and RAII members
    Graphics::ViewportManager::Destroy(kSceneViewportName);
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

    // Focus selected entity when the scene viewport is active.
    if (Input::IsKeyPressed(KEY_F) && !m_selectedEntity.IsNull()) {
        FocusOnEntity(m_selectedEntity.Index);
    }
    // Toggle viewport maximize/restore via hotkey.
    if (Input::IsKeyPressed(KEY_F11)) {
        if (!m_maximizeViewport) {
            m_requestRestore = false;
        } else {
            m_requestRestore = true;
        }
        m_maximizeViewport = !m_maximizeViewport;
    }

    // Update viewport interaction manager (gizmo, picking, selection, transforms)
    uint32_t newSelectedEntity = m_interactionMgr.Update(*m_world, m_selectedEntity.Index);
    
    // If selection changed due to picking, update our selected entity
    if (newSelectedEntity != m_selectedEntity.Index) {
        SetSelectedEntity(newSelectedEntity);
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
    // Handle maximize/restore state before rendering the window.
    ImGuiWindowFlags windowFlags = 0;
    if (!m_maximizeViewport && m_requestRestore && m_restoreDockValid) {
        if (m_restoreDockId != 0) {
            ImGui::SetNextWindowDockID(m_restoreDockId, ImGuiCond_Always);
        } else {
            ImGui::SetNextWindowPos(m_restorePos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(m_restoreSize, ImGuiCond_Always);
        }
        m_requestRestore = false;
    }
    if (m_maximizeViewport) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(vp->Size, ImGuiCond_Always);
        ImGui::SetNextWindowViewport(vp->ID);
        ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
        windowFlags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse |
                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    }

    // Begin scene viewport window
    ImGui::Begin("Scene", nullptr, windowFlags);
    if (!m_maximizeViewport) {
        // Cache docking state for restoring the viewport later.
        m_restoreDockId = ImGui::GetWindowDockID();
        m_restoreDockValid = true;
        m_restorePos = ImGui::GetWindowPos();
        m_restoreSize = ImGui::GetWindowSize();
    }

    auto* rendererSystem = _getRendererSystem();
    
    // --- Viewport Header -----------------------------------------------------
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));
    const float iconButtonSize = 28.0f;
    const float headerHeight = iconButtonSize + 10.0f;
    ImGui::BeginChild("##SceneViewportHeader", ImVec2(0.0f, headerHeight), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    // Vertically center controls within the header strip.
    const float centerOffset = std::max(0.0f, (headerHeight - iconButtonSize) * 0.5f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + centerOffset);

    // Clamp helper for per-group tinting.
    auto tintScale = [](const ImVec4& c, float s) {
        return ImVec4(
            std::min(c.x * s, 1.0f),
            std::min(c.y * s, 1.0f),
            std::min(c.z * s, 1.0f),
            c.w
        );
    };

    // Small helper to scope button palette overrides.
    auto pushButtonColors = [](const ImVec4& normal, const ImVec4& hover, const ImVec4& active, const ImVec4& text) {
        ImGui::PushStyleColor(ImGuiCol_Button, normal);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        return 4;
    };

    // Icon-only button with optional active tint and tooltip.
    auto iconButtonTinted = [&](const char* id, const char* icon, const char* tooltip, bool active, const ImVec4& tint, bool useSymbols) {
        const ImVec4 normal = active ? tintScale(tint, 1.25f) : tint;
        const ImVec4 hover = active ? tintScale(tint, 1.35f) : tintScale(tint, 1.12f);
        const ImVec4 pressed = active ? tintScale(tint, 1.15f) : tintScale(tint, 0.95f);
        const ImVec4 text = active ? EditorStyle::Text : EditorStyle::Muted;
        const int popCount = pushButtonColors(normal, hover, pressed, text);
        ImGui::PushID(id);
        if (useSymbols && m_symbolsFont) ImGui::PushFont(m_symbolsFont);
        // Remove padding inside viewport icon buttons
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
        const bool clicked = ImGui::Button(icon, ImVec2(iconButtonSize, iconButtonSize));
        ImGui::PopStyleVar(2);
        if (useSymbols && m_symbolsFont) ImGui::PopFont();
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(tooltip);
            ImGui::EndTooltip();
        }
        ImGui::PopID();
        if (popCount) ImGui::PopStyleColor(popCount);
        return clicked;
    };

    // Taken from Google Material Design Icons
    static const char* ICON_CAMERA_3D   = "\xEE\xB4\xB8";   // 3d
    static const char* ICON_CAMERA_2D   = "\xEE\xBC\xB7";   // 2d
    static const char* ICON_MOVE        = "\xEE\xA2\x9F";   // open_with
    static const char* ICON_ROTATE      = "\xEE\x90\x9D";   // rotate_right
    static const char* ICON_SCALE       = "\xEE\x8F\x82";   // crop_free
    static const char* ICON_LOCAL       = "\xEE\x95\x9C";   // my_location
    static const char* ICON_WORLD       = "\xEE\xA0\x8B";   // public
    static const char* ICON_OVERLAYS    = "\xEE\x94\xBB";   // layers
    static const char* ICON_DEBUG       = "\xEE\x90\xA9";   // tune
    static const char* ICON_LAYOUT_1    = "\xEE\x8F\x86";   // crop_square
    static const char* ICON_LAYOUT_2    = "\xEE\xA3\xB2";   // view_column
    static const char* ICON_LAYOUT_4    = "\xEE\xA6\xA9";   // grid_view
    static const char* ICON_MAX         = "\xEE\x97\x90";   // fullscreen
    static const char* ICON_RESTORE     = "\xEE\x97\x91";   // fullscreen_exit

    // Per-group tints to visually separate control clusters.
    const ImVec4 cameraTint = ImVec4(0.20f, 0.38f, 0.66f, 1.0f);
    const ImVec4 gizmoTint = ImVec4(0.20f, 0.62f, 0.44f, 1.0f);
    const ImVec4 spaceTint = ImVec4(0.18f, 0.58f, 0.64f, 1.0f);
    const ImVec4 overlayTint = ImVec4(0.78f, 0.56f, 0.20f, 1.0f);
    const ImVec4 debugTint = ImVec4(0.30f, 0.56f, 0.78f, 1.0f);
    const ImVec4 layoutTint = EditorStyle::SecondaryButton;
    const ImVec4 snapTint = ImVec4(0.46f, 0.48f, 0.56f, 1.0f);
    const ImVec4 maximizeTint = ImVec4(0.34f, 0.34f, 0.48f, 1.0f);

    // Camera mode
    bool isPerspective = true;
    if (m_editorCamera && m_editorCamera->GetCamera()) {
        isPerspective = m_editorCamera->GetCamera()->UsePerspective;
    }
    ImGui::PushID("CameraMode");
    if (iconButtonTinted("Ortho", ICON_CAMERA_2D, "Orthographic", !isPerspective, cameraTint, true)) {
        if (m_editorCamera) m_editorCamera->ResetTo2D();
    }
    ImGui::SameLine();
    if (iconButtonTinted("Persp", ICON_CAMERA_3D, "Perspective", isPerspective, cameraTint, true)) {
        if (m_editorCamera) m_editorCamera->ResetTo3D();
    }
    ImGui::PopID();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Gizmo operation and space mode
    auto curOp = m_interactionMgr.GetGizmoOperation();
    auto curMode = m_interactionMgr.GetGizmoMode();
    ImGui::PushID("GizmoOps");
    if (iconButtonTinted("Move", ICON_MOVE, "Move (W)", curOp == Editor::GizmoRenderer::Operation::Translate, gizmoTint, true)) {
        m_interactionMgr.SetGizmoOperation(Editor::GizmoRenderer::Operation::Translate);
    }
    ImGui::SameLine();
    if (iconButtonTinted("Rotate", ICON_ROTATE, "Rotate (E)", curOp == Editor::GizmoRenderer::Operation::Rotate, gizmoTint, true)) {
        m_interactionMgr.SetGizmoOperation(Editor::GizmoRenderer::Operation::Rotate);
    }
    ImGui::SameLine();
    if (iconButtonTinted("Scale", ICON_SCALE, "Scale (R)", curOp == Editor::GizmoRenderer::Operation::Scale, gizmoTint, true)) {
        m_interactionMgr.SetGizmoOperation(Editor::GizmoRenderer::Operation::Scale);
    }
    ImGui::PopID();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    bool isLocal = (curMode == Editor::GizmoRenderer::Mode::Local);
    if (iconButtonTinted("Local", ICON_LOCAL, "Local", isLocal, spaceTint, true)) {
        m_interactionMgr.SetGizmoMode(Editor::GizmoRenderer::Mode::Local);
    }
    ImGui::SameLine();
    if (iconButtonTinted("World", ICON_WORLD, "World", !isLocal, spaceTint, true)) {
        m_interactionMgr.SetGizmoMode(Editor::GizmoRenderer::Mode::World);
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Snap toggle with right-click settings popup.
    ImGui::PushID("Snap");
    if (iconButtonTinted("Toggle", "S", "Snap (Right-click for settings)", m_snapEnabled, snapTint, false)) {
        m_snapEnabled = !m_snapEnabled;
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("SceneSnapSettings");
    }
    if (ImGui::BeginPopup("SceneSnapSettings")) {
        ImGui::Checkbox("Enable Snap", &m_snapEnabled);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("Move", &m_snapTranslate, 0.05f, 0.01f, 50.0f, "%.2f");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("Rotate", &m_snapRotate, 0.5f, 0.1f, 90.0f, "%.1f deg");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("Scale", &m_snapScale, 0.01f, 0.001f, 10.0f, "%.3f");
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Overlays menu for editor helpers and diagnostics.
    ImGui::PushID("Overlays");
    const bool overlaysActive = m_showGrid || m_showAxes || m_showBounds || m_showColliders ||
        m_showLights || m_showCameraFrustum || m_showSceneFpsOverlay;
    if (iconButtonTinted("Button", ICON_OVERLAYS, "Overlays", overlaysActive, overlayTint, true)) {
        ImGui::OpenPopup("SceneOverlays");
    }
    if (ImGui::BeginPopup("SceneOverlays")) {
        ImGui::Checkbox("Grid", &m_showGrid);
        ImGui::Checkbox("Axes", &m_showAxes);
        ImGui::Checkbox("Bounds", &m_showBounds);
        ImGui::Checkbox("Colliders", &m_showColliders);
        ImGui::Checkbox("Lights", &m_showLights);
        ImGui::Separator();
        ImGui::Checkbox("Camera Frustum", &m_showCameraFrustum); // TODO: (currently not working)
        ImGui::Checkbox("FPS Overlay", &m_showSceneFpsOverlay);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::SameLine();
    // Debug render target selector.
    ImGui::PushID("DebugView");
    ImGui::SetNextItemWidth(110.0f);
    static const char* kDebugViews[] = { "Final", "HDR", "Bloom" };
    ImGui::Combo("##SceneDebugView", &m_debugViewIndex, kDebugViews, IM_ARRAYSIZE(kDebugViews));
    ImGui::PopID();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Layout preset buttons for dock rebuilding.
    ImGui::PushID("ViewportLayout");
    if (iconButtonTinted("Layout1", ICON_LAYOUT_1, "Layout: Single View", m_layoutPreset == 1, layoutTint, true)) {
        if (m_layoutPreset != 1) {
            m_layoutPreset = 1;
            Messaging::MessageSystem::Broadcast(Messaging::EditorViewportLayoutRequested(1));
        }
    }
    ImGui::SameLine();
    if (iconButtonTinted("Layout2", ICON_LAYOUT_2, "Layout: Split View", m_layoutPreset == 2, layoutTint, true)) {
        if (m_layoutPreset != 2) {
            m_layoutPreset = 2;
            Messaging::MessageSystem::Broadcast(Messaging::EditorViewportLayoutRequested(2));
        }
    }
    ImGui::SameLine();
    if (iconButtonTinted("Layout4", ICON_LAYOUT_4, "Layout: Quad View", m_layoutPreset == 4, layoutTint, true)) {
        if (m_layoutPreset != 4) {
            m_layoutPreset = 4;
            Messaging::MessageSystem::Broadcast(Messaging::EditorViewportLayoutRequested(4));
        }
    }
    ImGui::PopID();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Toggle maximize/restoration of the scene viewport.
    if (iconButtonTinted("Maximize", m_maximizeViewport ? ICON_RESTORE : ICON_MAX,
        m_maximizeViewport ? "Restore Viewport (F11)" : "Maximize Viewport (F11)",
        m_maximizeViewport, maximizeTint, true)) {
        if (!m_maximizeViewport) {
            // Cache docking state for restoring later.
            m_restoreDockId = ImGui::GetWindowDockID();
            m_restoreDockValid = true;
            m_restorePos = ImGui::GetWindowPos();
            m_restoreSize = ImGui::GetWindowSize();
            m_requestRestore = false;
        } else {
            m_requestRestore = true;
        }
        m_maximizeViewport = !m_maximizeViewport;
    }

    // Push snap settings into the gizmo system each frame.
    m_interactionMgr.SetGizmoSnap(m_snapEnabled, m_snapTranslate, m_snapRotate, m_snapScale);

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::Separator();

    // Get viewport size and position (remaining area after toolbar)
    const auto size = ImGui::GetContentRegionAvail();
    const auto pos = ImGui::GetCursorScreenPos();
    m_sceneDrawPos = pos;
    m_sceneDrawSize = size;

    if (size.x > 1.0f && size.y > 1.0f) {
        Graphics::ViewportManager::Resize(kSceneViewportName, static_cast<int>(size.x), static_cast<int>(size.y));
        if (m_editorCamera) {
            m_editorCamera->SetViewportSize(size.x, size.y);
        }
    }
    if (m_editorCamera) {
        Graphics::ViewportManager::SetCamera(kSceneViewportName, m_editorCamera->GetCamera());
    }

    // Broadcast viewport resize event for camera aspect ratio updates
    Messaging::MessageSystem::Broadcast(Messaging::ViewportResized(size.x, size.y));

    if (rendererSystem) {
        // Configure renderer to use EDITOR camera for Scene viewport
        rendererSystem->SetCamera(m_editorCamera->GetCamera());

        if (!rendererSystem->GetViewport(kSceneViewportName)) {
            const int vpWidth = std::max(1, static_cast<int>(size.x));
            const int vpHeight = std::max(1, static_cast<int>(size.y));
            Graphics::ViewportManager::Create(kSceneViewportName, m_editorCamera ? m_editorCamera->GetCamera() : nullptr, vpWidth, vpHeight);
        }

        // Render camera frustum overlay if enabled
        if (m_world && m_showCameraFrustum) {
            _renderCameraFrustum();
        }

        uint32_t textureId = 0;
        if (auto* vp = rendererSystem->GetViewport(kSceneViewportName)) {
            if (m_debugViewIndex == 1 && vp->HDR) {
                textureId = vp->HDR->GetColorTexture(0);
            } else if (m_debugViewIndex == 2 && vp->BloomExtract) {
                textureId = vp->BloomExtract->GetColorTexture(0);
            } else if (vp->LDR) {
                textureId = vp->LDR->GetColorTexture(0);
            }
        } else if (auto* rg = rendererSystem->GetRenderGraph()) {
            ResourceAccessor acc(rg);
            // Select debug texture based on the header dropdown.
            const char* debugTextureName = "LDR";
            if (m_debugViewIndex == 1) debugTextureName = "HDR";
            if (m_debugViewIndex == 2) debugTextureName = "BloomExtract";

            textureId = acc.GetTexture(debugTextureName);
            if (textureId == 0) {
                textureId = acc.GetTexture("LDR");
            }
        }

        if (textureId > 0) {
            ImGui::Image(textureId, size, ImVec2(0, 1), ImVec2(1, 0));

            // Check if image is hovered AFTER drawing it
            bool isSceneImageHovered = ImGui::IsItemHovered();

            // Accept asset drops on the scene viewport to set the active tileset.
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
                    const char* data = static_cast<const char*>(payload->Data);
                    if (data && payload->DataSize > 0) {
                        const std::string assetPath(data); // Payload is null-terminated list, first entry is enough.
                        if (m_tilePalettePanel) {
                            m_tilePalettePanel->HandleAssetDrop(assetPath);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Update m_isViewportHovered only when image is hovered
            m_isViewportHovered = isSceneImageHovered;

            // Update editor camera when image is hovered
            if (m_editorCamera) {
                m_editorCamera->SetViewportFocused(isSceneImageHovered);
                m_editorCamera->Update(static_cast<float>(TimeSystem::Instance().GetDeltaTime()));
            }

            // Get the drawing position of the rendered image
            ImVec2 viewportScreenPos = ImGui::GetItemRectMin();
            if (auto* renderer = ECS::RendererSystem::GetInstance()) {
                // GUI renders into the full LDR target; the scene image is a scaled blit of that target.
                // Using the ImGui rect as a GUI viewport offsets/clips GUI relative to the image.
                renderer->ResetGUIViewport();
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

                    // Handle mouse click picking on the viewport image
                    // Don't pick if gizmo is being used or hovered
                    // Let tile palette consume clicks when active.
                    bool tilePaletteHandledClick = false;
                    if (m_tilePalettePanel && isSceneImageHovered) {
                        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                            m_tilePalettePanel->SetPaintMode(false);
                        }
                    }
                    const bool canUseTilePalette = m_tilePalettePanel && m_tilePalettePanel->CanHandleViewportInput();
                    if (isSceneImageHovered && canUseTilePalette) {
                        double mx = 0, my = 0;
                        Input::GetMousePosition(mx, my);
                        const ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;

                        // Convert mouse position to world position
                        glm::vec2 vpMin = { viewportScreenPos.x * fbScale.x, viewportScreenPos.y * fbScale.y };
                        glm::vec2 vpSize = { size.x * fbScale.x, size.y * fbScale.y };
                        glm::vec2 localPos = glm::vec2(mx, my) - vpMin;
                        glm::vec4 ndc;

                        // NDC coordinates
                        ndc.x = (2.0f * localPos.x) / vpSize.x - 1.0f;
                        ndc.y = 1.0f - (2.0f * localPos.y) / vpSize.y;
                        ndc.z = 0.0f;
                        ndc.w = 1.0f;

                        // Unproject to world space
                        glm::mat4 invViewProj = glm::inverse(proj * view);
                        glm::vec4 world4 = invViewProj * ndc;
                        glm::vec2 worldPos = { world4.x, world4.y };

                        // Let tile palette panel handle hover and clicks
                        m_tilePalettePanel->OnViewportHover(worldPos);

                        // Draw tilemap bounds/grid when tile palette is active.
                        if (rendererSystem) {
                            const auto& map = m_tilePalettePanel->GetTileMap();
                            const glm::vec2 mapOrigin = m_tilePalettePanel->GetTileMapOrigin(); // Tilemap origin in world space.
                            if (map && map->LayerCount() > 0) {
                                const auto& layer = map->GetLayer(0);
                                const float tileSize = map->TileSize();

                                // Compute camera-aligned extents for an "infinite" grid overlay.
                                float halfHeight = 10.0f;
                                float halfWidth = 10.0f;
                                if (camera->UsePerspective) {
                                    const float dist = std::max(1.0f, std::abs(camera->Position.z));
                                    const float halfHeightView = std::tan(glm::radians(camera->FOV * 0.5f)) * dist;
                                    halfHeight = std::max(1.0f, halfHeightView);
                                    halfWidth = halfHeight * camera->AspectRatio;
                                } else {
                                    halfHeight = std::max(1.0f, camera->OrthoSize);
                                    halfWidth = halfHeight * camera->AspectRatio;
                                }

                                const glm::vec2 camPos(camera->Position.x, camera->Position.y);
                                const glm::vec2 camLocal = camPos - mapOrigin; // Align grid to the tilemap origin.
                                const float startX = std::floor((camLocal.x - halfWidth) / tileSize) * tileSize + mapOrigin.x;
                                const float endX = camPos.x + halfWidth;
                                const float startY = std::floor((camLocal.y - halfHeight) / tileSize) * tileSize + mapOrigin.y;
                                const float endY = camPos.y + halfHeight;

                                // Draw infinite-ish grid within the camera view.
                                const glm::vec4 gridColor(0.6f, 0.8f, 0.9f, 0.12f);
                                const uint32_t maxLines = 256;
                                uint32_t lineCount = 0;
                                for (float x = startX; x <= endX && lineCount < maxLines; x += tileSize, ++lineCount) {
                                    rendererSystem->SubmitWireframeLine(glm::vec2(x, startY), glm::vec2(x, endY), gridColor, 0.02f);
                                }
                                lineCount = 0;
                                for (float y = startY; y <= endY && lineCount < maxLines; y += tileSize, ++lineCount) {
                                    rendererSystem->SubmitWireframeLine(glm::vec2(startX, y), glm::vec2(endX, y), gridColor, 0.02f);
                                }

                                // Draw current tilemap bounds as a separate outline.
                                const glm::vec2 min(mapOrigin.x + map->TileToWorldSigned(map->OriginX()),
                                                    mapOrigin.y + map->TileToWorldSigned(map->OriginY())); // Bounds min in world space.
                                const glm::vec2 max(mapOrigin.x + map->TileToWorldSigned(map->OriginX() + static_cast<int32_t>(layer.Width())),
                                                    mapOrigin.y + map->TileToWorldSigned(map->OriginY() + static_cast<int32_t>(layer.Height()))); // Bounds max in world space.
                                const glm::vec4 outlineColor(0.2f, 0.9f, 0.9f, 0.45f);
                                rendererSystem->SubmitWireframeQuad(min, max, outlineColor, 0.05f);
                            }
                        }
                        bool left = Input::IsMousePressed(MOUSE_LEFT);
                        bool right = Input::IsMousePressed(MOUSE_RIGHT);
                        if (left || right) {
                            // Always treat clicks as handled when tile palette is active to avoid deselecting entities.
                            m_tilePalettePanel->OnViewportClick(worldPos, right);
                            tilePaletteHandledClick = true;
                        }
                    }
                    if (isSceneImageHovered && Input::IsMousePressed(MOUSE_LEFT) && !tilePaletteHandledClick) {
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

                    // Draw grid/axes overlays anchored to camera view.
                    if (m_showGrid || m_showAxes) {
                        const glm::vec2 camPos(camera->Position.x, camera->Position.y);
                        float halfWidth = 10.0f;
                        float halfHeight = 10.0f;

                        if (camera->UsePerspective) {
                            const float dist = std::max(1.0f, std::abs(camera->Position.z));
                            const float halfHeightView = std::tan(glm::radians(camera->FOV * 0.5f)) * dist;
                            halfHeight = std::max(1.0f, halfHeightView);
                            halfWidth = halfHeight * camera->AspectRatio;
                        } else {
                            halfHeight = std::max(1.0f, camera->OrthoSize);
                            halfWidth = halfHeight * camera->AspectRatio;
                        }

                        float step = 1.0f;
                        if (m_tilePalettePanel) {
                            const auto& map = m_tilePalettePanel->GetTileMap();
                            if (map && map->LayerCount() > 0) {
                                step = std::max(0.001f, map->TileSize());
                            }
                        }

                        // Calculate grid start and end positions aligned to step size
                        const float startX = std::floor((camPos.x - halfWidth) / step) * step;
                        const float endX = camPos.x + halfWidth;
                        const float startY = std::floor((camPos.y - halfHeight) / step) * step;
                        const float endY = camPos.y + halfHeight;

                        // Draw grid lines and axes
                        const glm::vec4 gridColor{ 0.35f, 0.40f, 0.45f, 0.35f };
                        const glm::vec4 axisX{ 0.90f, 0.25f, 0.25f, 0.8f };
                        const glm::vec4 axisY{ 0.25f, 0.85f, 0.35f, 0.8f };
                        const float gridThickness = 0.02f;
                        const float axisThickness = 0.02f;

                        // Draw grid lines, if enabled
                        if (m_showGrid) {
                            const uint32_t maxLines = 256;
                            uint32_t lineCount = 0;
                            for (float x = startX; x <= endX && lineCount < maxLines; x += step, ++lineCount) {
                                rendererSystem->SubmitWireframeLine(glm::vec2(x, camPos.y - halfHeight),
                                    glm::vec2(x, camPos.y + halfHeight), gridColor, gridThickness);
                            }
                            lineCount = 0;
                            for (float y = startY; y <= endY && lineCount < maxLines; y += step, ++lineCount) {
                                rendererSystem->SubmitWireframeLine(glm::vec2(camPos.x - halfWidth, y),
                                    glm::vec2(camPos.x + halfWidth, y), gridColor, gridThickness);
                            }
                        }

                        // Draw axes, if enabled
                        if (m_showAxes) {
                            rendererSystem->SubmitWireframeLine(glm::vec2(camPos.x - halfWidth, 0.0f),
                                glm::vec2(camPos.x + halfWidth, 0.0f), axisX, axisThickness);
                            rendererSystem->SubmitWireframeLine(glm::vec2(0.0f, camPos.y - halfHeight),
                                glm::vec2(0.0f, camPos.y + halfHeight), axisY, axisThickness);
                        }
                    }

                    // Draw selection outline around selected entity
                    // Optional selection bounds overlay.
                    if (m_showBounds && !m_selectedEntity.IsNull()) {
                        Editor::SelectionOutlineRenderer::RenderOutline(
                            *m_world,
                            m_selectedEntity.Index,
                            rendererSystem,
                            camera->OrthoSize,
                            size.x,
                            size.y
                        );
                    }

                    // Submit collider debug visualization for selected entity
                    // Optional collider debug overlay for selection.
                    if (m_showColliders && !m_selectedEntity.IsNull() && m_world) {
                        const glm::vec4 colliderColor{ 1.0f, 0.64f, 0.0f, 0.45f }; // Orange with some transparency
                        rendererSystem->SubmitColliderDebugDraw(*m_world, m_selectedEntity.Index, colliderColor);
                    }

                    // Optional light debug overlays
                    if (m_showLights && m_world) {
                        // Determine arrow length based on camera distance/ortho size
                        // Engine::Camera* camera = m_editorCamera ? m_editorCamera->GetCamera() : nullptr;
                        float arrowLength = 1.0f;
                        if (camera) {
                            // Scale arrow length based on camera parameters
                            if (camera->UsePerspective) {
                                arrowLength = std::max(1.0f, std::abs(camera->Position.z) * 0.05f);
                            } else {
                                arrowLength = std::max(1.0f, camera->OrthoSize * 0.05f);
                            }
                        }
                        // Precompute constants for arrow and circle rendering
                        const float arrowHead = arrowLength * 0.25f;
                        const float arrowWing = arrowHead * 0.6f;
                        const float lineThickness = 0.08f;
                        const float circleThickness = 0.08f;

                        // Iterate through all Light2D components in the world
                        auto* world = m_world;
                        world->Each<ECS::Components::LocalTransform, ECS::Components::Light2D>(
                            [&](ECS::Entity e, const ECS::Components::LocalTransform& lt, const ECS::Components::Light2D& light) {
                                // Skip inactive lights
                                if (const auto* active = world->TryGet<ECS::Components::Active>(e); active && !active->Enabled) {
                                    return;
                                }

                                // Determine world position of the light
                                Vector3D worldPos = lt.Position;
                                if (const auto* wt = world->TryGet<ECS::Components::WorldTransform>(e)) {
                                    worldPos = { wt->Matrix.m03, wt->Matrix.m13, wt->Matrix.m23 };
                                }

                                // Render directional light as arrow, point light as circle
                                if (light.LightType == ECS::Components::Light2D::Type::Directional) {
                                    glm::vec2 dir(light.Direction.X, light.Direction.Y);
                                    const float len2 = dir.x * dir.x + dir.y * dir.y;
                                    if (len2 < 1e-6f) {
                                        dir = glm::vec2(0.0f, -1.0f);
                                    } else {
                                        dir *= 1.0f / std::sqrt(len2);
                                    }

                                    // Draw arrow representing light direction
                                    const glm::vec2 start(worldPos.X, worldPos.Y);
                                    const glm::vec2 end = start + dir * arrowLength;
                                    const glm::vec4 tint(light.Color.R, light.Color.G, light.Color.B, 0.9f);
                                    rendererSystem->SubmitWireframeLine(start, end, tint, lineThickness);

                                    // Draw arrow head
                                    const glm::vec2 perp(-dir.y, dir.x);
                                    const glm::vec2 headBase = end - dir * arrowHead;
                                    rendererSystem->SubmitWireframeLine(headBase + perp * arrowWing, end, tint, lineThickness);
                                    rendererSystem->SubmitWireframeLine(headBase - perp * arrowWing, end, tint, lineThickness);
                                } else {
                                    // Draw circle representing point light range
                                    glm::vec2 center(worldPos.X + light.Position.X, worldPos.Y + light.Position.Y);
                                    const float range = std::max(0.01f, light.Range);
                                    const glm::vec4 tint(light.Color.R, light.Color.G, light.Color.B, 0.55f);
                                    rendererSystem->SubmitWireframeCircle(center, range, tint, circleThickness);
                                }
                            });
                    }

                    // Render gizmo via interaction manager
                    if (m_world && m_selectedEntity.Index != ECS::Entity::NPOS32) {
                        m_interactionMgr.RenderGizmo(*m_world, m_selectedEntity.Index);
                    }
                }
            }

            // Optional FPS overlay.
            if (m_showSceneFpsOverlay) {
                _drawFpsOverlay(viewportScreenPos, size);
            }
        } else {
            ImGui::TextDisabled("Viewport texture unavailable");
            m_isViewportHovered = false;
            if (m_editorCamera) {
                m_editorCamera->SetViewportFocused(false);
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
