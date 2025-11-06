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
        // Toggle Level Editor visibility with F2 (press)
        if (Input::IsKeyPressed(GLFW_KEY_F2))
            ToggleLevelEditor();
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
        m_debugUI->NewFrame();

        // Create a full-screen DockSpace and initialize layout once
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();

            // Safety check to prevent negative sizes
            if (vp->Size.x <= 0 || vp->Size.y <= 0) return;

            // Offset dockspace host below the global main menu bar to avoid overlapping tabs
            const float topOffset = ImGui::GetFrameHeight();

            // Calculate safe sizes that aren't negative
            ImVec2 safePos(vp->Pos.x, vp->Pos.y + topOffset);
            ImVec2 safeSize(vp->Size.x, std::max(1.0f, vp->Size.y - topOffset)); // Ensure at least 1px height

            ImGui::SetNextWindowPos(safePos);
            ImGui::SetNextWindowSize(safeSize);
            ImGui::SetNextWindowViewport(vp->ID);

            ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent black
            ImGui::Begin("MainDockSpaceHost", nullptr, hostFlags);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(3);

            ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
            ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode; // Show scene through central node
            ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);

            // Build initial docking layout once
            if (!m_dockLayoutBuilt) {
                m_dockLayoutBuilt = true;

                // Another safety check
                if (vp->Size.x <= 0 || vp->Size.y <= 0) {
                    // Skip layout building this frame, try again next frame
                    ImGui::End();
                    return;
                }

                m_dockLayoutBuilt = true;

                // Reset and rebuild
                ImGui::DockBuilderRemoveNode(dockspaceId);
                ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
                ImGui::DockBuilderSetNodeSize(dockspaceId, vp->Size);

                // First: split off right column (25% instead of 33%)
                ImGuiID leftCenterNode, rightNode;
                ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.25f, &rightNode, &leftCenterNode);

                // Split left + center vertically: top 65%, bottom 35% (for Asset Browser)
                ImGuiID topSection, assetBrowserNode;
                ImGui::DockBuilderSplitNode(leftCenterNode, ImGuiDir_Up, 0.65f, &topSection, &assetBrowserNode);

                // Split top section into left (33.3% of 75% = 25% total) and center (66.6% of 75% = 50% total)
                ImGuiID leftTopNode, centerTopSection;
                ImGui::DockBuilderSplitNode(topSection, ImGuiDir_Left, 0.333f, &leftTopNode, &centerTopSection);

                // Split center top section so Game Controls sits at TOP (~15%) and Viewport below (~85%)
                ImGuiID centerControlsNode, centerViewportNode;
                // Using ImGuiDir_Up: out_node_at_dir = TOP (Controls), remainder = BOTTOM (Viewport)
                ImGui::DockBuilderSplitNode(centerTopSection, ImGuiDir_Up, 0.154f, &centerControlsNode, &centerViewportNode);
                // Keep the tab bar visible for Game Controls so users can access the tab

                // The remainder from the last split (centerViewportNode) stays the central node
                // We intentionally rely on DockBuilderSplitNode semantics to avoid multiple central nodes

                // Dock windows
                ImGui::DockBuilderDockWindow("Hierarchy", leftTopNode);
                ImGui::DockBuilderDockWindow("Game Controls", centerControlsNode);
                ImGui::DockBuilderDockWindow("Viewport", centerViewportNode);
                ImGui::DockBuilderDockWindow("Asset Browser", assetBrowserNode); // Spans left + center bottom
                ImGui::DockBuilderDockWindow("Prefab Editor", rightNode);
                ImGui::DockBuilderDockWindow("Property Editor", rightNode);

                ImGui::DockBuilderFinish(dockspaceId);
            }
            ImGui::End();
        }
        
        // Update and render UIs (LevelEditor is conditional)
        if (m_levelEditor && IsLevelEditorEnabled()) {
            m_levelEditor->Update();
            m_levelEditor->Render();
        }
        m_debugUI->Render();

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
