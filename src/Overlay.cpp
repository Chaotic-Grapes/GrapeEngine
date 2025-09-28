#include "systems/Overlay.h"
#ifdef USE_IMGUI
#include "DebugUI.h"
#include "Input.h"
#include <systems/Window.h>
#include <systems/WindowManager.h>
#include <iostream>
#include "ecs/World.h"

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

            // Set the world reference for DebugUI
            DebugUI::SetWorld(m_world);

            // Mark as initialized so we don't do this again
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
