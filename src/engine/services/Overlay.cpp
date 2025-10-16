/**
 * @file Overlay.cpp
 * @author Foo Rui Qin
 * @date 2024
 * @brief Implementation of the Overlay system for debug UI management
 * 
 * This file implements the Overlay class which serves as a system-level wrapper
 * for managing debug UI functionality within the engine's ECS architecture.
 * The implementation provides:
 * 
 * Core Functionality:
 * - ImGui initialization and integration with GLFW/OpenGL backends
 * - DebugUI instance lifecycle management (creation, updates, cleanup)
 * - Audio system integration for real-time debug monitoring
 * - Window management integration for UI rendering context
 * - Conditional compilation support for ImGui features
 * 
 * System Integration:
 * - ECS system interface implementation (OnCreate, OnUpdate)
 * - World reference management for entity debugging
 * - Window manager integration for main window access
 * - Audio system attachment for debug monitoring
 * - Proper resource cleanup and memory management
 * 
 * Conditional Compilation:
 * - Full ImGui implementation when USE_IMGUI is defined
 * - Empty stub implementations when ImGui is not available
 * - Proper destructor handling for ImGui-specific resources
 * - Compile-time feature toggling for different build configurations
 * 
 * The Overlay system acts as a bridge between the engine's core systems
 * and the debug interface, ensuring proper initialization order and
 * resource management while maintaining clean separation of concerns.
 */

#include "services/Overlay.h"

#ifdef USE_IMGUI
#include "services/DebugUI.h"
#include "services/Input.h"
#include "services/Window.h"
#include "services/WindowManager.h"
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
            if (m_audioDevice)
                DebugUI::AttachAudio(m_audioDevice);
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
void Overlay::OnCreate() {
    // Empty implementation when ImGui is not available
}

void Overlay::OnUpdate() {
    // Empty implementation when ImGui is not available
}

#endif