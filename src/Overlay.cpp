#include "systems/Overlay.h"

#ifdef USE_IMGUI
#include "DebugUI.h"
#include "Input.h"
#include <systems/Window.h>
#include <systems/WindowManager.h>
#include <iostream>
#include "ecs/World.h"

void Overlay::OnCreate() {
    if (m_world) {
        m_debugUI = std::make_unique<DebugUI>(m_world);
    }
}

void Overlay::OnUpdate() {
    // If we don't have a DebugUI instance yet, try to create it
    if (!m_debugUI && m_world) {
        m_debugUI = std::make_unique<DebugUI>(m_world);  // Just pass the raw pointer directly
    }

    // If still no instance, then return
    if (!m_debugUI) return;

    // Try to initialize ImGui if not done yet
    if (!m_initialized) {
        // Get main window handle and set up ImGUI
        Window* mainWindow = WindowManager::GetMainWindow();
        if (mainWindow) {
            m_debugUI->Initialize(mainWindow->Handle());
            if (m_audio) DebugUI::AttachAudio(m_audio);
            m_debugUI->SyncWithWorld();
            m_initialized = true;
        }
        else return;
    }

    // Update UI every frame
    m_debugUI->NewFrame();
    m_debugUI->Render();
}

// Prevent memory leaks
Overlay::~Overlay() {
    if (m_debugUI) m_debugUI->Shutdown();
    DebugUI::DetachAudio();
}

#else
// Non-ImGui implementations
Overlay::Overlay() = default;

void Overlay::OnCreate() {
    // Empty implementation when ImGui is not available
}

void Overlay::OnUpdate() {
    // Empty implementation when ImGui is not available
}

#endif