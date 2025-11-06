#include "services/OverlayService.h"
#include "services/WindowManager.h"
#include "services/DebugUI.h"
#include "services/Input.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include "core/messaging/MessageTypes.h"
#include "core/messaging/MessageSystem.h"

#ifdef USE_IMGUI

namespace Services {
    void OverlayService::_onWindowResize(int width, int height) {
        m_dockLayoutBuilt = false; // Force layout to rebuild
        m_lastWindowSize = Vector2D(static_cast<float>(width), static_cast<float>(height));
    }

    void OverlayService::Initialize() {
        if (m_world) {
            m_debugUI = std::make_unique<DebugUI>(m_world);
            m_levelEditor = std::make_unique<LevelEditor>(m_world);
            UICommon::InitializeDefaultLayouts();
        }
        
        // Fallback if no world but still need debugUI
        if (!m_debugUI)
            m_debugUI = std::make_unique<DebugUI>(nullptr); // Pass nullptr

        if (!m_initialized) {
            // Get main window handle and set up ImGUI
            Window* mainWindow = WindowManager::GetMainWindow();
            if (mainWindow) {
                if (m_debugUI) 
                    m_debugUI->Initialize(mainWindow->Handle());
                if (m_levelEditor)
                    m_levelEditor->Initialize(mainWindow->Handle());
                if (m_audioDevice && m_debugUI)
                    m_debugUI->AttachAudio(m_audioDevice);

                // Save layout to file (docking purposes)
                ImGuiIO& io = ImGui::GetIO();
                io.IniFilename = "imgui.ini";
                m_initialized = true;
            }
        }
        // Subscribe to window resize events
        Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
            [this](const Messaging::WindowResized& msg) {
                _onWindowResize(msg.Width, msg.Height);
            });
    }

    void OverlayService::Update() {
        if (Input::IsKeyDown(KEY_F1))
            SetEnabled(!IsEnabled());

        if (!IsEnabled()) return;

        // If we don't have instances yet, try to create them
        if (!m_debugUI && m_world) {
            m_debugUI = std::make_unique<DebugUI>(m_world);
        }
        if (!m_levelEditor && m_world) {
            m_levelEditor = std::make_unique<LevelEditor>(m_world);
        }

        // If still no instances, then return
        if (!m_debugUI) return;

        // Try to initialize ImGui if not done yet
        if (!m_initialized) {
            // Get main window handle and set up ImGUI
            Window* mainWindow = WindowManager::GetMainWindow();
            if (mainWindow) {
                if (m_debugUI)
                    m_debugUI->Initialize(mainWindow->Handle());
                if (m_levelEditor)
                    m_levelEditor->Initialize(mainWindow->Handle());
                if (m_audioDevice && m_debugUI)
                    m_debugUI->AttachAudio(m_audioDevice);
                m_initialized = true;
            }
            else return;
        }

        // Audio and scene attachment
        if (m_audioDevice && m_debugUI && !m_debugUI->HasValidAudio())
            m_debugUI->AttachAudio(m_audioDevice);

        // Attach/detach scene as needed
        auto* scene = m_sceneManager.GetActive();
        if (scene && m_debugUI && !m_debugUI->HasValidScene(scene))
            m_debugUI->AttachScene(scene);
        else if (m_debugUI && m_debugUI->HasValidScene() && !scene)
            m_debugUI->DetachScene();
    }

    void OverlayService::Render() {
        if (!IsEnabled() || !m_debugUI || !m_levelEditor) return;

        // Update UI every frame
        if (m_debugUI) { m_debugUI->NewFrame(); }

        // LevelEditor handles its own docking
        if (m_levelEditor) {
            m_levelEditor->Update();
            m_levelEditor->Render();  // This now includes docking
        }

        // DebugUI renders freely (no docking)
        if (m_debugUI) { m_debugUI->Render(); }

        // Finalize and draw everything at once
        ImGui::Render();
        auto* drawData = ImGui::GetDrawData();
        if (drawData) {
            // Submit to OpenGL for GPU execution
            ImGui_ImplOpenGL3_RenderDrawData(drawData);
        }
    }

    // Prevent memory leaks
    void OverlayService::Terminate() {
        if (m_debugUI) 
            m_debugUI->Shutdown();
            m_debugUI->DetachAudio();
    }

    void OverlayService::EnableLevelEditorForScene(Scenes::Scene* scene) {
#ifdef USE_IMGUI
        if (scene && scene == m_sceneManager.GetActive()) {
            m_showLevelEditor = true;

            // Create LevelEditor if it doesn't exist
            if (!m_levelEditor && m_world) {
                m_levelEditor = std::make_unique<LevelEditor>(m_world);

                // Initialize if we have a window
                Window* mainWindow = WindowManager::GetMainWindow();
                if (mainWindow && m_initialized) {
                    m_levelEditor->Initialize(mainWindow->Handle());
                }
            }

            LOG_DEBUG("LevelEditor enabled for scene");
        }
#endif
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
