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
#include "graphics/EditorCamera.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "services/Input.h"
#include <imgui.h>
#include <algorithm>
#include "services/OverlayService.h"
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
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;

    // Create and initialize renderer system
    // RendererSystem creates and manages its own EditorCamera internally
    if (m_world && !m_rendererSystem) {
        m_rendererSystem = std::make_shared<ECS::RendererSystem>();
        m_rendererSystem->Initialize(*m_world);
        m_rendererSystem->BindWorld(*m_world);
        m_rendererSystem->SetEditorInputEnabled(true);

        if (m_undoSystem) {
            m_rendererSystem->SetUndoSystem(m_undoSystem);
        }

        // Wire up file menu if available
        if (m_fileMenu) {
            m_rendererSystem->SetFileMenu(m_fileMenu);
        }
    }
    
    // Create game renderer (always uses scene camera)
    if (m_world && !m_gameRendererSystem) {
        m_gameRendererSystem = std::make_shared<ECS::RendererSystem>();
        m_gameRendererSystem->Initialize(*m_world);
        m_gameRendererSystem->BindWorld(*m_world);
        m_gameRendererSystem->SetForceSceneCamera(true);
        m_gameRendererSystem->SetEditorInputEnabled(false);
        LOG_INFO("[Viewport] Game renderer initialized with force scene camera");
    }
}

Engine::EditorCamera* Viewport::GetEditorCamera() const {
    return m_rendererSystem ? m_rendererSystem->GetEditorCamera() : nullptr;
}

void Viewport::SetWorld(ECS::World* world) {
    m_world = world;

    // Create renderer if it doesn't exist yet (handles File > Open Scene case)
    if (!m_rendererSystem && world) {
        m_rendererSystem = std::make_shared<ECS::RendererSystem>();
        m_rendererSystem->Initialize(*world);
        m_rendererSystem->BindWorld(*world);
        m_rendererSystem->SetEditorInputEnabled(true);
        m_rendererSystem->SetUndoSystem(m_undoSystem);

        // Wire up file menu if available
        if (m_fileMenu) {
            m_rendererSystem->SetFileMenu(m_fileMenu);
        }
    }
    // Rebind existing renderer to new world
    else if (m_rendererSystem && world) {
        m_rendererSystem->BindWorld(*world);

        if (m_undoSystem) {
            m_rendererSystem->SetUndoSystem(m_undoSystem);
        }

        // Reset file menu when world changes
        if (m_fileMenu) {
            m_rendererSystem->SetFileMenu(m_fileMenu);
        }
    }
    
    // Create game renderer if it doesn't exist yet
    if (!m_gameRendererSystem && world) {
        m_gameRendererSystem = std::make_shared<ECS::RendererSystem>();
        m_gameRendererSystem->Initialize(*world);
        m_gameRendererSystem->BindWorld(*world);
        m_gameRendererSystem->SetForceSceneCamera(true);
        m_gameRendererSystem->SetEditorInputEnabled(false);
        LOG_INFO("[Viewport] Game renderer created in SetWorld");
    }
    // Rebind existing game renderer to new world
    else if (m_gameRendererSystem && world) {
        m_gameRendererSystem->BindWorld(*world);
        m_gameRendererSystem->SetForceSceneCamera(true);
        m_gameRendererSystem->SetEditorInputEnabled(false);
        LOG_INFO("[Viewport] Game renderer rebound to new world");
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

    // Control editor camera input based on viewport hover state
    // RendererSystem will call EditorCamera::Update() in its own Update()
    if (m_rendererSystem) {
        m_rendererSystem->SetEditorInputEnabled(m_isViewportHovered);
    }

    // Keep selection in sync with renderer picking only when hovering the viewport
    if (m_rendererSystem && m_isViewportHovered) {
        EntityId newSelection = m_rendererSystem->GetSelectedEntityID();

        // Only notify if selection actually changed
        if (newSelection != m_selectedEntityId) {
            m_selectedEntityId = newSelection;

            // Emit selection change event
            if (m_onSelectionChanged) {
                m_onSelectionChanged(m_selectedEntityId);
            }
        }
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

    if (m_rendererSystem) {
        auto size = ImGui::GetContentRegionAvail();
        auto pos = ImGui::GetCursorScreenPos();

        m_sceneDrawPos = pos;
        m_sceneDrawSize = size;

        // Broadcast viewport resize event for camera aspect ratio updates
        Messaging::MessageSystem::Broadcast(Messaging::ViewportResized(size.x, size.y));

        if (m_world) {
            m_rendererSystem->Update(*m_world, Time::DeltaTime());
        }

        auto* rg = m_rendererSystem->GetRenderGraph();
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
            uint32_t textureId = static_cast<uint32_t>(acc.GetTexture("LDR"));
            if (textureId > 0) {
                ImGui::Image((void*)(intptr_t)textureId, size, ImVec2(0, 1), ImVec2(1, 0));
            }

            // 1. Get the drawing position of the image we just rendered
            ImVec2 gizmoPos = ImGui::GetItemRectMin(); // Get the top-left corner of the image item

            // Draw FPS overlay if enabled (before gizmo so it appears behind)
            if (m_showSceneFpsOverlay) {
                _drawFpsOverlay(gizmoPos, size);
            }

            // 2. Call the RendererSystem method to draw the Gizmo overlay
            // Draw the Gizmo overlay (only in Scene tab)
            m_rendererSystem->DrawEditorGizmo(
                *m_world,
                gizmoPos.x, gizmoPos.y,
                size.x, size.y
            );
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
        m_world->Each<ECS::Components::Camera3D>([&](ECS::Entity e, ECS::Components::Camera3D& cam) {
            hasCameraComponent = true;
            cameraCount++;
            if (cam.Active) {
                hasActiveCamera = true;
            }
        });
    }

    if (!m_gameRendererSystem) {
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
            
            float availableRatio = availableSize.x / availableSize.y;
            
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
            float offsetX = (availableSize.x - displaySize.x) * 0.5f;
            float offsetY = (availableSize.y - displaySize.y) * 0.5f;
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY));
        }
        
        // Update camera aspect ratio to match display size (prevents squishing)
        if (m_world) {
            m_world->Each<ECS::Components::Camera3D>([targetRatio](ECS::Entity e, ECS::Components::Camera3D& cam) {
                if (cam.Active) {
                    cam.AspectRatio = targetRatio;
                }
            });
        }

        if (m_world) {
            m_gameRendererSystem->Update(*m_world, Time::DeltaTime());
        }

        auto* rg = m_gameRendererSystem->GetRenderGraph();
        if (rg) {
            ResourceAccessor acc(rg);
            uint32_t textureId = static_cast<uint32_t>(acc.GetTexture("LDR"));
            if (textureId > 0) {
                ImGui::Image((void*)(intptr_t)textureId, displaySize, ImVec2(0, 1), ImVec2(1, 0));
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

void Viewport::FocusOnEntity(EntityId entityId) {
    if (!m_world || !m_rendererSystem) return;

    ECS::Entity entity = m_world->Resolve(entityId);
    if (!m_world->IsAlive(entity)) return;

    // Get entity position (use WorldTransform if available, else LocalTransform)
    Vector3D position;
    if (m_world->Has<ECS::Components::WorldTransform>(entity)) {
        const auto& wt = m_world->Get<ECS::Components::WorldTransform>(entity);
        // Extract position from the translation column of the matrix
        // In a standard 4x4 transformation matrix, translation is in the last column
        position.X = wt.Matrix.m03;
        position.Y = wt.Matrix.m13;
        position.Z = wt.Matrix.m23;
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

    // Access editor camera through renderer system and focus on entity
    auto* editorCam = m_rendererSystem->GetEditorCamera();
    if (editorCam) {
        editorCam->Focus(glm::vec3(position.X, position.Y, position.Z));
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

    // Propagate to renderer if it exists
    if (m_rendererSystem) {
        m_rendererSystem->SetFileMenu(fileMenu);
    }
}

void Viewport::SetSelectedEntity(EntityId id) {
    // Keep local state in sync
    m_selectedEntityId = id;

    // Forward to renderer system so gizmo and selection outline update
    if (m_rendererSystem) {
        m_rendererSystem->SetSelectedEntityID(id);
    }
}

void Viewport::_drawFpsOverlay(const ImVec2& viewportPos, const ImVec2& viewportSize) {
    if (!m_rendererSystem) return;

    // Get FPS data from Profiler
    float currentFps = Profiler::GetFPS();
    float frameTimeMs = Profiler::GetFrameTimeMs();
    int flushCount = m_rendererSystem->GetFlushCount();

    // Position overlay in top-left corner of viewport with padding
    const float padding = 10.0f;
    ImVec2 overlayPos(viewportPos.x + padding, viewportPos.y + padding);

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
