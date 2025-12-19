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
#include <thread>
#include <chrono>
#include <atomic>
#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "EditorApplication.h"
#include "EditorState.h"
#include "services/TimeSystem.h"
#include "platform/IPlatformContext.h"
#include "scripting/ScriptManager.h"
#include "core/Logger.h"

/**
 * @brief Main entry point for the Grape Engine Level Editor
 */
int main() {
    // Enable memory leak detection in debug builds
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // Initialize engine in Editor mode
    Engine::Application engine;
#ifdef _DEBUG
    engine.Initialize(Engine::EngineMode::Editor, true);
#else
    engine.Initialize(Engine::EngineMode::Editor, false);
#endif

    // Get platform context for editor use
    auto* platformContext = engine.GetPlatformContext();
    if (!platformContext) {
        return -1;
    }

    // Create editor application
    EditorApplication editor(&engine);
    editor.Initialize();

    // Initialize systems after window is created
    ECS::World emptyWorld;
    engine.GetSystemManager().CreateAll(emptyWorld);

    // Initialize C# script compilation and hot reload watcher
    auto* scriptManager = engine.GetScriptManager();
    if (scriptManager && scriptManager->IsInitialized()) {
        // Use ProjectPaths to get the current project's Scripts folder
        if (Engine::ProjectPaths::IsInitialized()) {
            std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
            std::filesystem::path scriptsDir = projectRoot / "Scripts";
            std::filesystem::path scriptsOutput = projectRoot / "GameScripts.dll"; // TODO: Move this to somewhere more discrete and temporary
            
            // Create scripts directory if it doesn't exist
            if (!std::filesystem::exists(scriptsDir)) {
                std::filesystem::create_directories(scriptsDir);
                LOG_INFO("[EditorMain] Created Scripts directory: " << scriptsDir.string());
            }
            
            // Compile scripts on startup with progress logging
            LOG_INFO("[EditorMain] Compiling C# scripts on startup...");
            std::string diagnostics;

            // Set initial compile status and start a background poller to log progress
            scriptManager->SetCompileStatus(1, 0, "Starting compilation");
            std::atomic<bool> compileDone{false};
            std::thread progressThread([&]() {
                int status = 0;
                int progress = -1;
                std::string message;
                while (!compileDone.load()) {
                    scriptManager->GetCompileStatus(status, progress, message);
                    LOG_INFO("[EditorMain] Script compile status=" << status << " progress=" << progress << " msg=" << message);
                    if (status == 3 || status == 4) break; // success or failure
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                }
                // Final read to capture terminal state
                scriptManager->GetCompileStatus(status, progress, message);
                LOG_INFO("[EditorMain] Script compile finished status=" << status << " progress=" << progress << " msg=" << message);
            });

            bool compileSuccess = scriptManager->CompileScriptsWithDiagnostics(scriptsDir.string(), scriptsOutput.string(), diagnostics);

            // Ensure final status is set if managed side didn't update it
            if (compileSuccess) {
                scriptManager->SetCompileStatus(3, 100, "Compilation successful");
                LOG_INFO("[EditorMain] Initial script compilation succeeded");
            }
            else {
                scriptManager->SetCompileStatus(4, 100, diagnostics.c_str());
                LOG_ERROR("Failed to compile scripts:\n" << diagnostics);
            }

            // Signal the poller to exit and join
            compileDone.store(true);
            if (progressThread.joinable()) progressThread.join();

            // Start file watcher for hot reload - it will handle compilation on changes
            if (scriptManager->StartScriptWatching(scriptsDir.string())) {
                LOG_INFO("[EditorMain] C# script hot reload watcher started at: " << scriptsDir.string());
            }
            else {
                LOG_WARNING("[EditorMain] Failed to start C# script hot reload watcher (scripts will not auto-reload on changes)");
            }
        }
        else {
            LOG_WARNING("[EditorMain] ProjectPaths not initialized, cannot initialize script compilation");
        }
    }

    // Track previous state to detect transitions
    EditorState previousState = EditorState::Edit;

    // Editor main loop
    while (engine.IsRunning()) {
        // Update editor state and input
        editor.Update();
        
        // Engine update (input, services, UI events - but NOT systems in editor mode)
        engine.Update();
        
        // Editor controls which systems execute based on playback state
        // Get the current scene
        auto* currentScene = engine.GetSceneManager().GetActive();
        if (currentScene) {
            ECS::World& world = currentScene->GetWorld();
            float deltaTime = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
            
            // Determine which system modes to run based on editor playback state
            uint32_t systemModes = 0;
            
            // Always run these systems (render, transform hierarchy)
            systemModes |= (1 << static_cast<int>(ECS::SystemRunMode::Always));
            
            // Get current editor state
            EditorState state = editor.GetEditorState();
            
            // Only process transitions if state actually changed
            if (previousState != state) {
                // Only stop/start when transitioning between Edit and active play states
                // Paused state keeps audio initialized but systems don't run
                bool wasInEdit = (previousState == EditorState::Edit);
                bool isInEdit = (state == EditorState::Edit);
                
                if (!wasInEdit && isInEdit) {
                    // Transitioning to Edit: stop PlayOnly systems
                    auto& systemManager = engine.GetSystemManager();
                    systemManager.OnSceneStop(world);
                }
                else if (wasInEdit && !isInEdit) {
                    // Transitioning from Edit to any active state: start PlayOnly systems
                    auto& systemManager = engine.GetSystemManager();
                    systemManager.OnSceneStart(world);
                }
                
                // Update previous state after processing transition
                previousState = state;
            }
            
            // Run gameplay systems based on playback state
            switch (state) {
                case EditorState::Play:
                    // Play mode: run all gameplay systems
                    systemModes |= (1 << static_cast<int>(ECS::SystemRunMode::PlayOnly));
                    break;
                    
                case EditorState::Paused:
                    // Paused: run editor-only systems (gizmos, debug rendering)
                    systemModes |= (1 << static_cast<int>(ECS::SystemRunMode::EditOnly));
                    break;
                    
                case EditorState::Step:
                    // Step: run gameplay systems once
                    systemModes |= (1 << static_cast<int>(ECS::SystemRunMode::PlayOnly));
                    break;
                    
                case EditorState::Edit:
                default:
                    // Edit mode: run editor-only systems (gizmos, debug rendering)
                    systemModes |= (1 << static_cast<int>(ECS::SystemRunMode::EditOnly));
                    break;
            }
            
            // Execute the filtered systems
            engine.UpdateSystemsByMode(systemModes, world, deltaTime);
        }
        
        // Render editor UI
        editor.Render();

        // Swap buffers using platform abstraction
        for (auto* win : platformContext->GetAllWindows()) {
            win->SwapBuffers();
        }
    }

    // Shutdown
    editor.Shutdown();
    engine.Shutdown();

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
