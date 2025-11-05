namespace Services {
    void OverlayService::Initialize() {
        if (m_world) {
            m_debugUI = std::make_unique<DebugUI>(m_world);
            m_levelEditor = std::make_unique<LevelEditor>(m_world);
            UICommon::InitializeDefaultLayouts();
        }
        
        // Fallback if no world but still need debugUI
        if (!m_debugUI)
            m_debugUI = std::make_unique<DebugUI>();

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
        }
    }

    void OverlayService::Update() {
        if (Input::IsKeyDown(GLFW_KEY_F1))
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
        m_debugUI->NewFrame();

        // Create a full-screen DockSpace and initialize layout once
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            // Offset dockspace host below the global main menu bar to avoid overlapping tabs
            const float topOffset = ImGui::GetFrameHeight();
            ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + topOffset));
            ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - topOffset));
            ImGui::SetNextWindowViewport(vp->ID);
            ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                ImGuiWindowFlags_NoBackground; // Keep scene visible under dockspace host
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("MainDockSpaceHost", nullptr, hostFlags);
            ImGui::PopStyleVar(3);

            ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
            ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode; // Show scene through central node
            ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);

            // Build initial docking layout once
            if (!m_dockLayoutBuilt) {
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

                // Split center top section: 90% viewport, 10% controls at the bottom (relative to the 65% height)
                ImGuiID centerControlsNode, centerViewportNode;
                // 10% of total height / 65% of total height = ~0.154
                ImGui::DockBuilderSplitNode(centerTopSection, ImGuiDir_Down, 0.154f, &centerControlsNode, &centerViewportNode);
                // Keep the tab bar visible for Game Controls so users can access the tab

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
        
        // Update and render both UIs
        m_levelEditor->Update();
        m_levelEditor->Render();
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
        if (m_levelEditor)
            m_levelEditor->Shutdown(); // Assuming LevelEditor has Shutdown method
        DebugUI::DetachAudio();
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
