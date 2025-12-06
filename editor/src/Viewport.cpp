/* Start Header *****************************************************************/
/*!
\file   ViewportPanel.cpp
\author Samantha Leong (50%)
        Foo Rui Qin    (45%)
        Muhammad Nur Fadzly Bin Zulkifli (5%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   3rd November 2025
\brief
Implements the Viewport and Game classes for core editor functionality and entity management.
Handles the main menu, viewport rendering, and entity selection with event callbacks.
*/
/* End Header *******************************************************************/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include "Viewport.h"
#include "CameraFrustumRenderer.h"
#include "EditorGizmo.h"
#include "SelectionOutlineRenderer.h"
#include "EditorCamera.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "services/Input.h"
#include <imgui.h>
#include <algorithm>
#include "services/EditorService.h"
#include "core/Application.h"
#include "scene/Scene.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include "ecs/systems/RendererSystem.h"  
#include "graphics/RenderGraph.hpp"
#include "services/Time.h"
#include "services/WindowManager.h"

// Messaging for editor warnings
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include <unordered_map>

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
void Viewport::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world, Scenes::SceneManager* sceneManager) {
    (void)sceneManager;

    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;

    // Create editor camera (viewport owns it)
    if (!m_editorCamera) {
        m_editorCamera = std::make_unique<Editor::EditorCamera>();
    }
    
    // Subscribe to engine events for editor integration
    if (m_undoSystem) {
        m_transformChangedSubscription = Messaging::MessageSystem::Subscribe<Messaging::EntityTransformChanged>(
            [this](const Messaging::EntityTransformChanged& e) {
                m_undoSystem->RecordTransformChange(e.EntityId, 
                    e.OldPosition, e.OldRotation, e.OldScale,
                    e.NewPosition, e.NewRotation, e.NewScale);
            }
        );
    }

    if (m_fileMenu) {
        m_sceneModifiedSubscription = Messaging::MessageSystem::Subscribe<Messaging::SceneModified>(
            [this](const Messaging::SceneModified& e) {
                (void)e; // May use e.Reason for logging
				m_fileMenu->MarkSceneDirty();
            }
        );
    }
}

void Viewport::SetWorld(ECS::World* world) {
    m_world = world;

    // Create editor camera if needed
    if (!m_editorCamera) {
        m_editorCamera = std::make_unique<Editor::EditorCamera>();
    }
}

// -------------------------------------------------------------------------
// Event Registration
// -------------------------------------------------------------------------
void Viewport::OnSelectionChanged(std::function<void(EntityId)> callback) {
    m_onSelectionChanged = callback;
}

// -------------------------------------------------------------------------
// Update
// -------------------------------------------------------------------------
void Viewport::HandleInWorldInteraction() {
    if (!HasValidWorld()) return;

    if (m_undoSystem) {
        m_undoSystem->Update();
    }

    // Toggle FPS overlay in the Scene viewport (editor-only)
    if (Input::IsKeyPressed(KEY_F)) {
        m_showSceneFpsOverlay = !m_showSceneFpsOverlay;
    }

    // Editor camera input is controlled via SetAllowInput() above
    // Selection is handled by ViewportPicking utility in editor
    // Viewport maintains its own m_selectedEntityId state
    
    // Handle editor-specific entity manipulation (drag-to-move)
    if (m_isViewportHovered && m_activeTab == 0) { // Only in Scene tab
        _handleEntityDragToMove();
    }
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------
void Viewport::ShowEditorWindows() {
    _renderViewport();
}

// -------------------------------------------------------------------------
// Viewport
// -------------------------------------------------------------------------
void Viewport::_renderViewport() {
    // Render Viewport window (editor camera)
    ImGui::Begin("Scene");

    // Check if viewport window is hovered AND focused (not blocked by other windows)
    m_isViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)
        && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // Update editor camera (processes input if viewport is hovered)
    if (m_editorCamera) {
        m_editorCamera->SetAllowInput(m_isViewportHovered);
        m_editorCamera->Update(Time::DeltaTime());
    }

    auto* rendererSystem = _getRendererSystem();
    if (rendererSystem) {
        const auto size = ImGui::GetContentRegionAvail();
        const auto pos = ImGui::GetCursorScreenPos();

        m_sceneDrawPos = pos;
        m_sceneDrawSize = size;

        // Broadcast viewport resize event for camera aspect ratio updates
        Messaging::MessageSystem::Broadcast(Messaging::ViewportResized(size.x, size.y));

        // Configure renderer for Scene viewport (uses editor camera)
        rendererSystem->SetCamera(m_editorCamera->GetCamera());
        rendererSystem->SetForceSceneCamera(false);

        // Note: RendererSystem::Update() is called by Application's main loop
        // We just configure camera settings here

        // Render camera frustum overlay if enabled
        if (m_world && m_showCameraFrustum) {
            _renderCameraFrustum();
        }

        auto* rg = rendererSystem->GetRenderGraph();
        if (rg) {
            ResourceAccessor acc(rg);
            // Previously, the editor viewport was sampling from the "HDR" texture,
            // which meant ImGui was displaying raw FP16 linear data with no tone mapping
            // or gamma. Resulting in very dark, incorrect colors.
            //
            // We now use a dedicated ToneMap pass that converts HDR to LDR. The Composite
            // pass then blits LDR to the real backbuffer. Sampling "LDR" here ensures the
            // Editor viewport shows the same tone-mapped, gamma-correct image the game
            // actually outputs.
            const uint32_t textureId = acc.GetTexture("LDR");
            if (textureId > 0) {
                ImGui::Image(textureId, size, ImVec2(0, 1), ImVec2(1, 0));
            }

            // 1. Get the drawing position of the image we just rendered
            ImVec2 gizmoPos = ImGui::GetItemRectMin(); // Get the top-left corner of the image item

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
    }

    ImGui::End();

    // Render Game window (scene camera)
    ImGui::Begin("Game");

    // Aspect ratio selector panel
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        
        const char* aspectRatios[] = {
            "Free Aspect",
            "16:9",
            "16:10",
            "4:3",
            "5:4",
            "21:9",
            "1:1"
        };
        
        ImGui::SetNextItemWidth(150);
        if (ImGui::Combo("##AspectRatio", &m_selectedAspectRatio, aspectRatios, IM_ARRAYSIZE(aspectRatios))) {
            m_freeAspect = (m_selectedAspectRatio == 0);
        }
        
        ImGui::PopStyleVar(2);
        ImGui::Separator();
    }

    // Check if any Camera3D components exist in the world
    bool hasCameraComponent = false;
    bool hasActiveCamera = false;
    int cameraCount = 0;
    if (m_world) {
        m_world->Each<ECS::Components::Camera3D>([&](const ECS::Entity e, const ECS::Components::Camera3D& cam) {
            (void)e;

            hasCameraComponent = true;
            cameraCount++;
            if (cam.Active) {
                hasActiveCamera = true;
            }
        });
    }

    if (!rendererSystem) {
        ImGui::TextDisabled("No game renderer not initialized");
    }
    else if (!hasCameraComponent) {
        ImGui::TextDisabled("No camera found");
        ImGui::TextDisabled("Add a Camera3D component to an entity");
    }
    else if (!hasActiveCamera) {
        ImGui::Text("Found %d camera(s) but none are active", cameraCount);
        ImGui::TextDisabled("Set Camera3D.Active to true in the inspector");
    }
    else {
        auto availableSize = ImGui::GetContentRegionAvail();
        
        // Calculate display size based on aspect ratio
        ImVec2 displaySize = availableSize;
        float targetRatio = availableSize.x / availableSize.y;
        
        if (!m_freeAspect) {
            switch (m_selectedAspectRatio) {
                case 1: targetRatio = 16.0f / 9.0f; break;   // 16:9
                case 2: targetRatio = 16.0f / 10.0f; break;  // 16:10
                case 3: targetRatio = 4.0f / 3.0f; break;    // 4:3
                case 4: targetRatio = 5.0f / 4.0f; break;    // 5:4
                case 5: targetRatio = 21.0f / 9.0f; break;   // 21:9
                case 6: targetRatio = 1.0f; break;           // 1:1
            }
            
            const float availableRatio = availableSize.x / availableSize.y;
            
            if (availableRatio > targetRatio) {
                // Available space is wider - constrain width
                displaySize.x = availableSize.y * targetRatio;
                displaySize.y = availableSize.y;
            }
            else {
                // Available space is taller - constrain height
                displaySize.x = availableSize.x;
                displaySize.y = availableSize.x / targetRatio;
            }
            
            // Center the viewport
            const float offsetX = (availableSize.x - displaySize.x) * 0.5f;
            const float offsetY = (availableSize.y - displaySize.y) * 0.5f;
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY));
        }
        
        // Update camera aspect ratio to match display size (prevents squishing)
        if (m_world) {
            m_world->Each<ECS::Components::Camera3D>([targetRatio](ECS::Entity e, ECS::Components::Camera3D& cam) {
                (void)e;

                if (cam.Active) {
                    cam.AspectRatio = targetRatio;
                }
            });
        }

        // Configure renderer for Game viewport (uses scene camera)
        if (rendererSystem) {
            LOG_DEBUG("[Viewport] Game renderer update - forceSceneCamera should be true");
            rendererSystem->SetCamera(nullptr);  // Use ECS camera
            rendererSystem->SetForceSceneCamera(true);
        }

        auto* rg = rendererSystem ? rendererSystem->GetRenderGraph() : nullptr;
        if (rg) {
            ResourceAccessor acc(rg);
            const uint32_t textureId = acc.GetTexture("LDR");
            if (textureId > 0) {
                ImGui::Image(textureId, displaySize, ImVec2(0, 1), ImVec2(1, 0));
            }
            else {
                ImGui::TextDisabled("Texture ID is 0 - render graph issue");
            }
        }
        else {
            ImGui::TextDisabled("No render graph available");
        }
    }

    ImGui::End();
}

void Viewport::FocusOnEntity(const EntityId entityId) {
    auto* rendererSystem = _getRendererSystem();
    if (!m_world || !rendererSystem) return;

    const ECS::Entity entity = m_world->Resolve(entityId);
    if (!m_world->IsAlive(entity)) return;

    // Get entity position (use WorldTransform if available, else LocalTransform)
    Vector3D position;
    if (m_world->Has<ECS::Components::WorldTransform>(entity)) {
        const auto& [Matrix, Dirty] = m_world->Get<ECS::Components::WorldTransform>(entity);
        // Extract position from the translation column of the matrix
        // In a standard 4x4 transformation matrix, translation is in the last column
        position.X = Matrix.m03;
        position.Y = Matrix.m13;
        position.Z = Matrix.m23;
    }
    else if (m_world->Has<ECS::Components::LocalTransform>(entity)) {
        const auto& lt = m_world->Get<ECS::Components::LocalTransform>(entity);
        position = lt.Position;
    }
    else {
        // No transform component, can't focus
        LOG_WARNING("Cannot focus on entity " << entityId << ": no transform component");
        return;
    }

    // Focus editor camera on entity
    if (m_editorCamera) {
        m_editorCamera->Focus(glm::vec3(position.X, position.Y, position.Z));
    }
    else {
        LOG_WARNING("Cannot focus: editor camera not available");
    }
}

// -------------------------------------------------------------------------
// Accessors
// -------------------------------------------------------------------------
EntityId Viewport::GetSelectedEntityId() const {
    return m_selectedEntityId;
}

bool Viewport::IsViewportHovered() const {
    return m_isViewportHovered;
}

void Viewport::SetFileMenu(EditorFileMenu* fileMenu) {
    m_fileMenu = fileMenu;

    // Subscribe to scene modification events
    if (fileMenu) {
        m_sceneModifiedSubscription = Messaging::MessageSystem::Subscribe<Messaging::SceneModified>(
            [this](const Messaging::SceneModified& e) {
                (void)e; // May use e.Reason for logging
                if (m_fileMenu) {
                    m_fileMenu->MarkSceneDirty();
                }
            }
        );
    }
}

void Viewport::SetSelectedEntity(const EntityId id) {
    // Keep local state in sync
    m_selectedEntityId = id;
    
    // Selection is now managed entirely by editor
    // No need to sync with renderer system
}

void Viewport::_renderCameraFrustum() {
    auto* rendererSystem = _getRendererSystem();
    if (!rendererSystem || !m_world || !m_editorCamera) {
        return;
    }

    auto* rg = rendererSystem->GetRenderGraph();
    if (!rg) {
        return;
    }

    // Get HDR framebuffer to draw on
    ResourceAccessor acc(rg);
    auto* hdrFbo = acc.GetFramebuffer("HDR");
    if (!hdrFbo) {
        return;
    }

    // Bind HDR framebuffer (don't clear, we're overlaying)
    hdrFbo->Bind();

    // Get editor camera info for view projection
    auto* cam = m_editorCamera->GetCamera();
    if (!cam) {
        Framebuffer::Unbind();
        return;
    }

    const glm::mat4 view = cam->GetViewMatrix();
    const glm::mat4 projection = cam->GetProjectionMatrix();
    const glm::mat4 viewProj = projection * view;

    // Get window dimensions
    const auto& win = WindowManager::GetMainWindow();
    const float windowHeight = static_cast<float>(win->GetHeight());

    // Get renderer and shader from renderer system
    Renderer* renderer = rendererSystem->GetRenderer();
    Shader* shader = rendererSystem->GetShader();
    
    if (renderer && shader) {
        Editor::CameraFrustumRenderer::RenderFrustum(
            *m_world,
            *renderer,
            *shader,
            viewProj,
            cam->OrthoSize,
            windowHeight,
            0  // No entity to exclude (editor camera is not an ECS entity)
        );
    }

    Framebuffer::Unbind();
}

void Viewport::_drawFpsOverlay(const ImVec2& viewportPos, const ImVec2& viewportSize) {
    (void)viewportSize;
    
    auto* rendererSystem = _getRendererSystem();
    if (!rendererSystem) return;

    // Get FPS data from Profiler
    const float currentFps = Profiler::GetFPS();
    const float frameTimeMs = Profiler::GetFrameTimeMs();
    const int flushCount = rendererSystem->GetFlushCount();

    // Position overlay in top-left corner of viewport with padding
    constexpr float padding = 10.0f;
    const ImVec2 overlayPos(viewportPos.x + padding, viewportPos.y + padding);

    // Setup overlay window
    ImGui::SetNextWindowPos(overlayPos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f); // Semi-transparent background

    ImGuiWindowFlags overlayFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("##SceneFpsOverlay", nullptr, overlayFlags)) {
        // Determine FPS color
        ImVec4 fpsColor;
        if (currentFps >= 60.0f) {
            fpsColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
        }
        else if (currentFps >= 30.0f) {
            fpsColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
        }
        else {
            fpsColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
        }

        // Current FPS (bold font, colored)
        if (m_boldFont) ImGui::PushFont(m_boldFont);
        ImGui::TextColored(fpsColor, "%.1f FPS", currentFps);
        if (m_boldFont) ImGui::PopFont();

        // Frame time in milliseconds
        ImGui::Text("%.2f ms", frameTimeMs);

        // Batch flush count (optional performance metric)
        if (flushCount >= 0) {
            ImGui::Separator();
            ImGui::Text("Batches: %d", flushCount);
        }

        // Toggle hint
        ImGui::Separator();
        ImGui::TextDisabled("Press F to toggle");
    }
    ImGui::End();
}

ECS::RendererSystem* Viewport::_getRendererSystem() {
    // Get the global RendererSystem from Application's SystemManager
    if (Engine::CORE) {
        return Engine::CORE->GetSystemManager().GetSystem<ECS::RendererSystem>();
    }
    return nullptr;
}

void Viewport::_handleEntityDragToMove() {
    if (!m_world || m_selectedEntityId == 0) {
        m_isDragging = false;
        m_wasMouseDownLastFrame = false;
        return;
    }

    auto* rendererSystem = _getRendererSystem();
    if (!rendererSystem) return;

    // Get camera matrices for screen-to-world conversion
    const Engine::Camera* camera = m_editorCamera->GetCamera();
    if (!camera) return;

    const glm::mat4 view = camera->GetViewMatrix();
    const glm::mat4 projection = camera->GetProjectionMatrix();

    // Get mouse state
    glm::dvec2 mousePos;
    Input::GetMousePosition(mousePos.x, mousePos.y);
    
    const bool isMouseDownThisFrame = Input::IsMouseDown(MOUSE_LEFT);
    const bool mouseJustPressed = isMouseDownThisFrame && !m_wasMouseDownLastFrame;

    // Convert screen to world coordinates
    const auto screenToWorld = [&](const glm::dvec2& screenPos) -> glm::vec2 {
        // Convert to viewport-local coordinates
        const auto localPos = glm::vec2(screenPos.x - m_sceneDrawPos.x, screenPos.y - m_sceneDrawPos.y);

        // Normalize device coordinates (-1 to 1)
        glm::vec4 ndc;
        ndc.x = (2.0f * localPos.x) / m_sceneDrawSize.x - 1.0f;
        ndc.y = 1.0f - (2.0f * localPos.y) / m_sceneDrawSize.y;
        ndc.z = 0.0f;
        ndc.w = 1.0f;

        // Inverse projection and view
        const glm::mat4 invViewProj = glm::inverse(projection * view);
        glm::vec4 worldPos = invViewProj * ndc;

        return {worldPos.x, worldPos.y};
    };

    glm::vec2 mouseWorld = screenToWorld(mousePos);

    // Check if selection changed (from hierarchy or inspector)
    if (m_selectedEntityId != m_lastSelectedEntityID) {
        m_isDragging = false;
        m_lastSelectedEntityID = m_selectedEntityId;
    }

    // Start drag on mouse press
    if (mouseJustPressed && !m_isDragging) {
        m_world->Each<ECS::Components::LocalTransform>([&](const ECS::Entity e, const ECS::Components::LocalTransform& lt) {
            if (e.Index == m_selectedEntityId) {
                m_dragStartEntityPos = glm::vec3(lt.Position.X, lt.Position.Y, lt.Position.Z);
                m_dragStartEntityRot = lt.Rotation;
                m_dragStartEntityScale = lt.Scale;
            }
        });
        m_dragStartMouseWorld = mouseWorld;
    }

    // Check for drag threshold (5 pixels in world space)
    if (isMouseDownThisFrame && !m_isDragging) {
        const glm::vec2 dragDelta = mouseWorld - m_dragStartMouseWorld;
        const float dragDistance = glm::length(dragDelta);
        const float dragThreshold = (camera->OrthoSize / m_sceneDrawSize.y) * 5.0f;

        if (dragDistance > dragThreshold) {
            m_isDragging = true;
            m_dragStartMouseWorld = mouseWorld; // Reset to current position
        }
    }

    // Update entity position while dragging
    if (m_isDragging && isMouseDownThisFrame) {
        const glm::vec2 dragDelta = mouseWorld - m_dragStartMouseWorld;

        m_world->Each<ECS::Components::LocalTransform>([&](const ECS::Entity e, ECS::Components::LocalTransform& lt) {
            if (e.Index == m_selectedEntityId) {
                lt.Position.X = m_dragStartEntityPos.x + dragDelta.x;
                lt.Position.Y = m_dragStartEntityPos.y + dragDelta.y;

                // Mark scene as dirty
                if (m_fileMenu) {
                    m_fileMenu->MarkSceneDirty();
                }
            }
        });
    }

    // End drag and create undo command
    if (m_isDragging && !isMouseDownThisFrame) {
        m_world->Each<ECS::Components::LocalTransform>([&](const ECS::Entity e, const ECS::Components::LocalTransform& lt) {
            if (e.Index == m_selectedEntityId) {
                const Vector3D oldPos(m_dragStartEntityPos.x, m_dragStartEntityPos.y, m_dragStartEntityPos.z);
                const Vector3D newPos = lt.Position;

                // Only notify if position actually changed
                if (oldPos != newPos) {
                    Messaging::MessageSystem::Notify(
                        Messaging::EntityTransformChanged(
                            e.Index, oldPos, m_dragStartEntityRot, m_dragStartEntityScale,
                            newPos, lt.Rotation, lt.Scale
                        )
                    );
                }
            }
        });
        m_isDragging = false;
    }

    m_wasMouseDownLastFrame = isMouseDownThisFrame;
}
