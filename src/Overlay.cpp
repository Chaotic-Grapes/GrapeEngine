#include "systems/Overlay.h"

#ifdef USE_IMGUI
#include "DebugUI.h"
#include "Input.h"
#include <systems/Window.h>
#include <systems/WindowManager.h>
#include <iostream>
#include "ecs/World.h"

void Overlay::OnCreate() {
    // Don't create DebugUI here since we might not have world yet
}

void Overlay::OnUpdate() {
    // Use static bool to track if ImGUI has been initialized
    static bool initialized = false;
    // If not, initialize it now
    if (!initialized) {
        // Get the main window from WindowManager
        Window* mainWindow = WindowManager::GetMainWindow();

        // If window exists
        if (mainWindow) {
            // We can safely initialize ImGUI
            // ImGUI needs a valid OpenGL context (provided by the window)
            DebugUI::Initialize(mainWindow->Handle());
            // Mark as initialized so we don't do this again
            if (m_audio) { DebugUI::AttachAudio(m_audio); } // attach the real instance
            initialized = true;
        }
        // Window doesn't exist yet
        else {
            // Skip rendering this frame and try again next frame
            return;
        }
    }

    // If still no instance, then return
    if (!m_debugUI) return;

    // Try to initialize ImGui if not done yet
    if (!m_initialized) {
        // Get main window handle and set up ImGUI
        Window* mainWindow = WindowManager::GetMainWindow();
        if (mainWindow) {
            m_debugUI->Initialize(mainWindow->Handle());
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
    DebugUI::DetachAudio();
    DebugUI::Shutdown();
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