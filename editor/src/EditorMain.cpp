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
#include "EditorComponentRegistry.h"
#include "EditorState.h"
#include "services/TimeSystem.h"
#include "platform/IPlatformContext.h"
#include "platform/IWindow.h"
#include "scripting/ScriptManager.h"
#include "core/Logger.h"
#include "scripting/ScriptsCompiler.h"
#include "ecs/events/EventDispatcher.h"

extern "C" {
    // Forward declare the component deserialize callback registration function
    void RegisterComponentDeserializeCallback(void(*callback)(uint32_t, void*, int, const char*));

    // Request high-performance GPU
    __declspec(dllexport) unsigned long NvOptimusEnablement         = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance  = 1;
}

/**
 * @brief Main entry point for the Grape Engine Level Editor
 */
int main() {
    // Enable memory leak detection in debug builds
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // Delete temporary compiled script assemblies from previous runs
    {
        std::filesystem::path tempScriptsPath = Engine::ProjectPaths::GetTempScriptsPath();
        if (std::filesystem::exists(tempScriptsPath)) {
            try {
                std::filesystem::remove_all(tempScriptsPath);
                LOG_INFO("[EditorMain] Cleaned up temporary script assemblies at: " << tempScriptsPath.string());
            } catch (const std::exception& e) {
                LOG_WARNING("[EditorMain] Failed to clean up temporary script assemblies: " << e.what());
            }
        } else {
            LOG_INFO("[EditorMain] No temporary script assemblies to clean up at: " << tempScriptsPath.string());
        }
    }

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

    // Build the initial component registry immediately so the editor UI has metadata
    // This ensures components are available for rendering in the inspector on startup
    ComponentRegistryUI::RebuildFromNativeRegistry();
    LOG_INFO("[EditorMain] Initial component registry built");

    // Initialize C# script compilation and hot reload watcher
    ECS::ScriptManager* scriptManager = engine.GetScriptManager();

    // Initialize script callbacks for hot reload
    editor.InitializeScriptCallbacks(scriptManager, &emptyWorld);

    // Copy GrapeEngine.Scripting.dll to temp directory so it can be found at runtime
    {
        std::filesystem::path tempScriptsPath = Engine::ProjectPaths::GetTempScriptsPath();
        if (!tempScriptsPath.empty() && std::filesystem::exists(tempScriptsPath)) {
            if (scriptManager) {
                scriptManager->CopyScriptingAssemblyToDirectory(tempScriptsPath.string());
            }
        }
    }

    // Create the ScriptsCompiler to manage compilation and hot-reload
    ScriptsCompiler scriptsCompiler(&engine, &emptyWorld);
    if (scriptManager && scriptManager->IsInitialized()) {
        scriptsCompiler.Initialize(scriptManager);
        scriptsCompiler.Start();
        LOG_INFO("[EditorMain] ScriptsCompiler initialized and started");
    }

    // Track previous state to detect transitions
    EditorState previousState = EditorState::Edit;

    LOG_INFO("Using GPU: " << glGetString(GL_RENDERER));

    // Editor main loop
    while (engine.IsRunning()) {
        // Let ScriptsCompiler handle compilation/hot-reload and deferred registry rebuild
        scriptsCompiler.Update();
        

        // If the editor window is minimized or unfocused, avoid burning CPU in a tight loop.
        // This does not affect FPS cap behavior when focused.
        if (platformContext) {
            auto* mainWindow = platformContext->GetMainWindow();
            if (mainWindow && (mainWindow->IsMinimized() || !mainWindow->IsFocused() || !mainWindow->IsVisible())) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // ============================================================
        // EDITOR BEGIN FRAME - Handle input and request picking
        // ============================================================
        editor.BeginFrame();
        
        // ============================================================
        // EDITOR UPDATE - Update editor state and playback controls
        // ============================================================
        editor.Update();
        
        // ============================================================
        // ENGINE UPDATE - Process input, time, and services
        // ============================================================
        engine.Update();
        
        // Editor controls which systems execute based on playback state
        // Get the current scene
        auto* currentScene = engine.GetSceneManager().GetActive();
        if (currentScene) {
            ECS::World& world = currentScene->GetWorld();
            // float deltaTime = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
            
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
            engine.UpdateSystemsByMode(systemModes, world);
            ECS::Events::ClearFrameEventComponents(world);
        }
        
        // ============================================================
        // EDITOR RENDER - Render editor UI and viewports
        // ============================================================
        editor.Render();

        // ============================================================
        // EDITOR END FRAME - Resolve picking and update selection
        // ============================================================
        editor.EndFrame();

        // Swap buffers using platform abstraction
        for (auto* win : platformContext->GetAllWindows()) {
            if (!win) continue;
            if (!win->IsVisible() || win->IsMinimized()) continue;
            win->SwapBuffers();
        }
    }

    // Shutdown
    editor.Shutdown();
    engine.Shutdown();

    // Make sure script compiler background work is finished before exiting
    scriptsCompiler.Shutdown();

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
