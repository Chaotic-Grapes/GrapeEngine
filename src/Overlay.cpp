#include "systems/Overlay.h"

#ifdef USE_IMGUI
#include "DebugUI.h"
#include "Input.h"
#include <systems/Window.h>
#include <systems/WindowManager.h>
#include <iostream>
#include "ecs/World.h"

// Define constructor here where DebugUI is complete
Overlay::Overlay() = default;

void Overlay::OnCreate() {
    // Don't create DebugUI here since we might not have world yet
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
            m_initialized = true;
        }
        else return;
    }

    // Update UI every frame
    m_debugUI->NewFrame();
    m_debugUI->Render();
}

// Destructor
Overlay::~Overlay() = default;

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