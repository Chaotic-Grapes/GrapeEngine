/**
 * @file Overlay.cpp
 * @author Foo Rui Qin
 * @date 2024
 * @brief Implementation of the Overlay system for debug UI management
 * 
 * This file implements the OverlayService class which serves as a system-level wrapper
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
 * The OverlayService system acts as a bridge between the engine's core systems
 * and the debug interface, ensuring proper initialization order and
 * resource management while maintaining clean separation of concerns.
 */

#include "services/OverlayService.h"

#ifdef USE_IMGUI
#include "services/DebugUI.h"
#include "services/Input.h"
#include "services/Window.h"
#include "services/WindowManager.h"
#include "scene/SceneManager.h"
#include <iostream>

namespace Services {
    void OverlayService::Initialize() {
        if (!m_debugUI)
            m_debugUI = std::make_unique<DebugUI>();

        if (!m_initialized) {
            // Get main window handle and set up ImGUI
            Window* mainWindow = WindowManager::GetMainWindow();
            if (mainWindow) {
                m_debugUI->Initialize(mainWindow->Handle());
                if (m_audioDevice)
                    m_debugUI->AttachAudio(m_audioDevice);
                m_initialized = true;
            }
        }
    }

    void OverlayService::Update() {
        // TODO: Still using GLFW KEYS so replace it later
        if (Input::IsKeyDown(GLFW_KEY_F1))
            SetEnabled(!IsEnabled());
        if (!IsEnabled()) return;

		// Ensure DebugUI instance exists
        if (!m_debugUI)
            return;

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

        if (m_audioDevice && !m_debugUI->HasValidAudio())
			m_debugUI->AttachAudio(m_audioDevice);

        // Attach/detach scene as needed
        auto* scene = Scenes::SceneManager::GetActive();
        if (scene && !m_debugUI->HasValidScene(scene))
            m_debugUI->AttachScene(scene);
        else if (m_debugUI->HasValidScene() && !scene)
            m_debugUI->DetachScene();

        // Update UI every frame
        m_debugUI->NewFrame();
        m_debugUI->Render();
    }

    // Prevent memory leaks
    void OverlayService::Terminate() {
        if (m_debugUI)
            m_debugUI->Shutdown();
    }

#else
	 // Non-ImGui implementations
	void OverlayService::Update() {
	    // Empty implementation when ImGui is not available
	}

	void OverlayService::Terminate() {
	    // Empty implementation when ImGui is not available
	}
#endif
}