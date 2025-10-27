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