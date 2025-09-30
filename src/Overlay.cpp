#include "systems/Overlay.h"
#ifdef USE_IMGUI
#include "DebugUI.h"
#include "Input.h"
#include <systems/Window.h>
#include <systems/WindowManager.h>
#include <iostream>

void Overlay::OnCreate() {
    // Don't initialize ImGUI here cause the window might not exist yet
    // due to system initialization order (Overlay runs before WindowManager)
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
    // Start a new ImGUI frame
    DebugUI::NewFrame();
    // Draw all the ImGUI windows and widgets
    DebugUI::Render();
}

// Prevent memory leaks
Overlay::~Overlay() {
    DebugUI::DetachAudio();
    DebugUI::Shutdown();
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
