/* Start Header *****************************************************************/
/*!
\file   Overlay.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the Overlay class which serves as a system-level wrapper for managing
debug UI and level editor functionality within the engine's ECS architecture.

Features:
- ImGui initialization and integration with GLFW/OpenGL backends
- DebugUI and LevelEditor lifecycle management (creation, updates, cleanup)
- Audio system integration for real-time debug monitoring
- Window management integration for UI rendering context
- Conditional compilation support for ImGui features
- UI layout configuration and management
*/
/* End Header *******************************************************************/

#include "services/Overlay.h"

#ifdef USE_IMGUI
#include "services/DebugUI.h"
#include "../editor/LevelEditor.h"
#include "services/Input.h"
#include "services/Window.h"
#include "services/WindowManager.h"
#include "services/UICommon.h"
#include <iostream>
#include "ecs/World.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
// DockBuilder APIs
#include <imgui_internal.h>

// Initialize UI instances and default layouts
void Overlay::OnCreate() {
    if (m_world) {
        m_debugUI = std::make_unique<DebugUI>(m_world);
        m_levelEditor = std::make_unique<LevelEditor>(m_world);
        UICommon::InitializeDefaultLayouts();
    }
}

// Update UI every frame (handle initialization, rendering, and finalization)
void Overlay::OnUpdate() {
    // If we don't have a DebugUI instance yet, try to create it
    if (!m_debugUI && m_world) {
        m_debugUI = std::make_unique<DebugUI>(m_world);  // Just pass the raw pointer directly
    }

    if (!m_levelEditor && m_world) {
        m_levelEditor = std::make_unique<LevelEditor>(m_world);
    }

    // If still no instance, then return
    if (!m_debugUI || !m_levelEditor) return;

    // Try to initialize ImGui if not done yet
    if (!m_initialized) {
        // Get main window handle and set up ImGUI
        Window* mainWindow = WindowManager::GetMainWindow();
        if (mainWindow) {
            m_debugUI->Initialize(mainWindow->Handle());
            m_levelEditor->Initialize(mainWindow->Handle());
            if (m_audioDevice)
                DebugUI::AttachAudio(m_audioDevice);
            m_initialized = true;
        }
        else return;
    }

    // Update UI every frame
    m_debugUI->NewFrame();

    // Create a full-screen DockSpace and initialize layout once
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::SetNextWindowViewport(vp->ID);
        ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground; // Keep scene visible under dockspace host
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("MainDockSpaceHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
        ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode; // Show scene through central node
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);

        // Build initial docking layout once
        if (!m_dockLayoutBuilt) {
            m_dockLayoutBuilt = true;
            // Reset and rebuild
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::DockBuilderSetNodeSize(dockspaceId, vp->Size);

            // Split right for Prefab Editor (full height)
            ImGuiID rightNode;
            ImGuiID leftNode = dockspaceId;
            ImGui::DockBuilderSplitNode(leftNode, ImGuiDir_Right, 0.35f, &rightNode, &leftNode);

            // Split left vertically: top (playback), bottom (asset browser)
            ImGuiID topNode;
            ImGuiID bottomNode;
            ImGui::DockBuilderSplitNode(leftNode, ImGuiDir_Up, 0.25f, &topNode, &bottomNode);

            // Further split top area into left/center/right; dock controls in center
            ImGuiID topLeft;
            ImGuiID topRest;
            ImGui::DockBuilderSplitNode(topNode, ImGuiDir_Left, 0.33f, &topLeft, &topRest);
            ImGuiID topRight;
            ImGuiID topCenter;
            ImGui::DockBuilderSplitNode(topRest, ImGuiDir_Right, 0.33f, &topRight, &topCenter);

            // Split bottom area into left/right; dock asset browser to bottom-left
            ImGuiID bottomLeft;
            ImGuiID bottomRight;
            ImGui::DockBuilderSplitNode(bottomNode, ImGuiDir_Left, 0.60f, &bottomLeft, &bottomRight);

            // Dock windows by title
            ImGui::DockBuilderDockWindow("Prefab Editor", rightNode);
            ImGui::DockBuilderDockWindow("Asset Browser", bottomLeft);
            ImGui::DockBuilderDockWindow("Game Controls", topCenter);

            ImGui::DockBuilderFinish(dockspaceId);
        }

        ImGui::End();
    }

    m_levelEditor->Update();

    // Draw debugUI and level editor
    m_debugUI->Render();
    m_levelEditor->Render();

    // Finalize and draw everything at once
    ImGui::Render();
    auto* drawData = ImGui::GetDrawData();
    if (drawData) {
        // Submit to OpenGL for GPU execution
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
}

// Prevent memory leaks
Overlay::~Overlay() {
    if (m_debugUI) m_debugUI->Shutdown();
    DebugUI::DetachAudio();
}

#else
// Non-ImGui implementations
void Overlay::OnCreate() {
    // Empty implementation when ImGui is not available
}

void Overlay::OnUpdate() {
    // Empty implementation when ImGui is not available
}

#endif