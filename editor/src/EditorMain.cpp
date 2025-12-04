/* Start Header *****************************************************************/
/*!
\file   EditorMain.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
Main entry point for the Grape Engine Level Editor.
Launches the application in editor mode with the level editor interface.
*/
/* End Header *******************************************************************/

#include <crtdbg.h>
#include <filesystem>
#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "Game.h"
#include "services/WindowManager.h"
#include "editor/services/OverlayService.h"
#include "editor/EditorConfiguration.h"
#include "core/Logger.h"
#include "physics/Physics.h"

/**
 * @brief Editor-focused game class that launches directly into the level editor
 */
class EditorGame : public Game {
private:
    EditorSettings m_editorSettings;

public:
    void OnStart(Scenes::SceneManager& sceneManager) override {
        (void)sceneManager; // Editor starts scene-less
        
        LOG_INFO("Starting Grape Engine Level Editor...");
        
        // Load editor configuration
        bool configLoaded = Editor::EditorConfiguration::LoadConfig("config.json", m_editorSettings);
        if (!configLoaded) {
            // Fallback: common scenario when running from build directory
            if (Editor::EditorConfiguration::LoadConfig("../config.json", m_editorSettings)) {
                LOG_INFO("Loaded editor configuration from parent directory: ../config.json");
            }
        }
        else {
            LOG_INFO("Loaded editor configuration: " << std::filesystem::current_path().string() + "/config.json");
        }
        
        // TODO: Remove hardcoded "EchoesBelow" when editor is separated from engine
        // Initialize project paths to point to game project folder
        Engine::ProjectPaths::Initialize("EchoesBelow");
        
        // Load project-specific settings
        Engine::CORE->LoadProjectSettings("EchoesBelow");
        
        // Get window dimensions from editor config (editor always uses its own settings)
        int width = m_editorSettings.WindowSettings.Width;
        int height = m_editorSettings.WindowSettings.Height;
        
        // Apply physics settings from project configuration if available
        if (Engine::CORE->HasProjectSettings()) {
            const auto& projectSettings = Engine::CORE->GetProjectSettings();
            Engine::Physics::SetGravity(Vector2D(0.0f, projectSettings.Physics.Gravity));
            LOG_INFO("Applied physics gravity from ProjectSettings: " << projectSettings.Physics.Gravity);
        }

        // Create main window based on editor config
        LOG_INFO("Creating main window from EditorSettings");
        auto* window = CREATE_WINDOW_EX("Grape Engine Editor", width, height, 
            m_editorSettings.WindowSettings.VSync,
            WindowMode::Windowed
        );

        if (!window) {
            LOG_ERROR("Failed to create main window for editor");
            return;
        }
        
        window->SetMaximized(m_editorSettings.WindowSettings.Maximized);
        
        // Enable level editor without a scene (scene-less editor mode)
        if (auto* overlay = Services::OverlayService::Get()) {
            overlay->EnableLevelEditorForScene(nullptr);
            LOG_INFO("Level Editor initialized successfully");
        }
        else {
            LOG_ERROR("Failed to initialize Level Editor: OverlayService not available");
        }
    }

    void OnUpdate(Scenes::SceneManager& sceneManager) override {
        (void)sceneManager;
        // Editor updates are handled by the overlay service

    }

    void OnShutdown(Scenes::SceneManager& sceneManager) override {
        (void)sceneManager;
        
        // Save editor settings on shutdown
        if (auto* window = WindowManager::GetMainWindow()) {
            m_editorSettings.WindowSettings.Width = window->GetWidth();
            m_editorSettings.WindowSettings.Height = window->GetHeight();
            m_editorSettings.WindowSettings.Maximized = window->IsMaximized();
            m_editorSettings.WindowSettings.VSync = window->IsVSync();
            
            Editor::EditorConfiguration::SaveConfig("config.json", m_editorSettings);
            LOG_INFO("Saved editor configuration");
        }
        
        DESTROY_ALL_WINDOWS();
        LOG_INFO("Grape Engine Editor shutdown complete");
    }
};

/**
 * @brief Main entry point for the Grape Engine Level Editor
 */
int main() {
    // Enable memory leak detection in debug builds
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // Create and run the editor
    Engine::Application engine;
    EditorGame editor;
    
#ifdef _DEBUG
    engine.Run(editor, true);
#else
    engine.Run(editor, false);
#endif

    return 0;
}

#ifdef _WIN32
#include <windows.h>
// WinMain shim for GUI/Windows subsystem: forward to main()
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return main();
}
#endif
