/* Start Header *****************************************************************/
/*!
\file   BaseViewport.cpp
\author Samantha Leong (50%)
        Foo Rui Qin    (50%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implementation of BaseViewport class with shared functionality for Scene and Game viewports.

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

#include "BaseViewport.h"
#include "CameraFrustumRenderer.h"
#include "SelectionOutlineRenderer.h"
#include "EditorCamera.hpp"
#include "EditorECSUtils.h"
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
#include "core/Application.h"
#include "platform/IPlatformContext.h"
#include "serialization/EntitySerializer.h"
#include "ecs/systems/RendererSystem.h"  
#include "graphics/RenderGraph.hpp"
#include "services/TimeSystem.h"

// Messaging for editor warnings
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include <unordered_map>

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

// Unsubscribe from all message system subscriptions
BaseViewport::~BaseViewport() {
    Messaging::MessageSystem::Unsubscribe<Messaging::EntityTransformChanged>(m_transformChangedSubscription);
    Messaging::MessageSystem::Unsubscribe<Messaging::SceneModified>(m_sceneModifiedSubscription);
}

// Store fonts, create the editor camera, and subscribe to engine events
void BaseViewport::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
    ECS::World* world, Scenes::SceneManager* sceneManager) {
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

// Replace the active ECS world and clear any stale entity selection
void BaseViewport::SetWorld(ECS::World* world) {
    if (m_world == world) {
        return;
    }

    m_world = world;

    // Clear stale selection when switching scenes to avoid rendering invalid entities.
    SetSelectedEntity(ECS::Entity::NPOS32);

    // Create editor camera if needed
    if (!m_editorCamera) {
        m_editorCamera = std::make_unique<Editor::EditorCamera>();
    }
}


// -------------------------------------------------------------------------
// Event Registration
// -------------------------------------------------------------------------

// Register the callback to be invoked whenever the selected entity changes
void BaseViewport::OnSelectionChanged(std::function<void(EntityId)> callback) {
    m_onSelectionChanged = callback;
}

// Store the file menu reference and subscribe to scene modification events
void BaseViewport::SetFileMenu(EditorFileMenu* fileMenu) {
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

// -------------------------------------------------------------------------
// Accessors
// -------------------------------------------------------------------------

// Return the index portion of the currently selected entity
EntityId BaseViewport::GetSelectedEntityId() const {
    return m_selectedEntity.Index;
}

// Return whether the cursor is currently over the viewport window
bool BaseViewport::IsViewportHovered() const {
    return m_isViewportHovered;
}

// Resolve the entity from the world, update internal state, and fire selection callbacks
void BaseViewport::SetSelectedEntity(const EntityId id) {
    // Convert EntityId to full Entity by looking it up in the world
    // This ensures we have the correct generation for IsAlive() validation
    if (m_world && id != ECS::Entity::NPOS32) {
        m_selectedEntity = m_world->Resolve(id);
    }
    else {
        m_selectedEntity = ECS::NULL_ENTITY;
    }
    
    // Notify any registered callbacks so the rest of the editor can react
    if (m_onSelectionChanged) {
        m_onSelectionChanged(id);
    }

    // Broadcast a selection request message so other systems can observe
    if (m_world) {
        Messaging::MessageSystem::Broadcast(Messaging::EntitySelectionRequested(id, false));
    }
}

// Move the editor camera to frame the given entity in the viewport
void BaseViewport::FocusOnEntity(const EntityId entityId) {
    auto* rendererSystem = _getRendererSystem();
    if (!m_world || !rendererSystem) return;

    const ECS::Entity entity = m_world->Resolve(entityId);
    if (!m_world->IsAlive(entity)) return;

    // Get entity position (use WorldTransform if available, else LocalTransform)
    Vector3D position;
    if (Editor::ECSUtils::HasComponent(m_world, entity, "WorldTransform")) {
        const auto* wt = Editor::ECSUtils::GetComponentPtr<ECS::Components::WorldTransform>(m_world, entity, "WorldTransform");
        if (!wt) {
            return;
        }
        // Extract position from the translation column of the matrix
        // In a standard 4x4 transformation matrix, translation is in the last column
        position.X = wt->Matrix.m03;
        position.Y = wt->Matrix.m13;
        position.Z = wt->Matrix.m23;
    }
    else if (Editor::ECSUtils::HasComponent(m_world, entity, "LocalTransform")) {
        const auto* lt = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(m_world, entity, "LocalTransform");
        if (!lt) {
            return;
        }
        position = lt->Position;
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
// Protected Helper Methods
// -------------------------------------------------------------------------

// Draw world-space frustum lines for all scene cameras using the renderer system
void BaseViewport::_renderCameraFrustum() {
    auto* rendererSystem = _getRendererSystem();
    if (!rendererSystem || !m_world || !m_editorCamera) {
        return;
    }
    auto* cam = m_editorCamera->GetCamera();
    if (!cam) {
        return;
    }

    // Get window dimensions
    auto* context = Engine::CORE->GetPlatformContext();
    auto* win = context ? context->GetMainWindow() : nullptr;
    if (!win) return;
    const float windowHeight = static_cast<float>(win->GetHeight());

    Editor::CameraFrustumRenderer::RenderFrustum(
        *m_world,
        rendererSystem,
        cam->OrthoSize,
        windowHeight,
        0  // No entity to exclude (editor camera is not an ECS entity)
    );
}

// Render a semi-transparent FPS/frame-time overlay in the top-left corner of the viewport
void BaseViewport::_drawFpsOverlay(const ImVec2& viewportPos, const ImVec2& viewportSize) {
    (void)viewportSize;
    
    auto* rendererSystem = _getRendererSystem();
    if (!rendererSystem) return;

    // Get FPS data from TimeSystem
    const float currentFps = TimeSystem::Instance().GetFPS();
    const float frameTimeMs = TimeSystem::Instance().GetFrameTimeMs();
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

    }
    ImGui::End();
}

// Retrieve the global RendererSystem from the Application's SystemManager
ECS::RendererSystem* BaseViewport::_getRendererSystem() {
    // Get the global RendererSystem from Application's SystemManager
    if (Engine::CORE) {
        return Engine::CORE->GetSystemManager().GetSystem<ECS::RendererSystem>();
    }
    return nullptr;
}
