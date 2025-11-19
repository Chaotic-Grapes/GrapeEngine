/* Start Header *****************************************************************/
/*!
\file   ViewportPanel.cpp
\author Samantha Leong (80%)
        Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implements the ViewportPanel class for core editor functionality and entity management.
Handles the main menu, viewport rendering, and entity selection with event callbacks.
*/
/* End Header *******************************************************************/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include "../editor/Viewport.h"
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

        // Wire up file menu if available
        if (m_fileMenu) {
            m_rendererSystem->SetFileMenu(m_fileMenu);
        }
    }
}

void Viewport::SetWorld(ECS::World* world) {
    m_world = world;

    // Create renderer if it doesn't exist yet (handles File > Open Scene case)
    if (!m_rendererSystem && world) {
        m_rendererSystem = std::make_shared<ECS::RendererSystem>();
        m_rendererSystem->Initialize(*world);
        m_rendererSystem->BindWorld(*world);
        m_rendererSystem->SetEditorInputEnabled(true);

        // Wire up file menu if available
        if (m_fileMenu) {
            m_rendererSystem->SetFileMenu(m_fileMenu);
        }
    }
    // Rebind existing renderer to new world
    else if (m_rendererSystem && world) {
        m_rendererSystem->BindWorld(*world);

        // Reset file menu when world changes
        if (m_fileMenu) {
            m_rendererSystem->SetFileMenu(m_fileMenu);
        }
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
    ImGui::Begin("Viewport");

    m_isViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

    if (m_rendererSystem) {
        auto size = ImGui::GetContentRegionAvail();
        auto pos = ImGui::GetCursorScreenPos();

        if (m_world) {
            m_rendererSystem->Update(*m_world, Time::DeltaTime());
        }

        auto* rg = m_rendererSystem->GetRenderGraph();
        if (rg) {
            ResourceAccessor acc(rg);
            uint32_t textureId = static_cast<uint32_t>(acc.GetTexture("HDR"));
            if (textureId > 0) {
                ImGui::Image((void*)(intptr_t)textureId, size, ImVec2(0, 1), ImVec2(1, 0));
            }
        }
    }
    else {
        ImGui::TextDisabled("No renderer available");
    }

    ImGui::End();
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