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
#include "scripting/ScriptManager.h"
#include "core/Logger.h"

// Forward declare the component deserialize callback registration function
extern "C" void RegisterComponentDeserializeCallback(void(*callback)(uint32_t, void*, int, const char*));

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
    
    // Register hot reload callback so systems get reinitialized when scripts are reloaded
    if (scriptManager && scriptManager->IsInitialized()) {
        scriptManager->SetHotReloadCallback([&engine, &emptyWorld](const std::string& assemblyPath) {
            LOG_INFO("[EditorMain] Hot reload callback triggered for: " << assemblyPath);
            
            // Unregister old C# systems (calls OnDestroy)
            engine.GetSystemManager().UnregisterScriptedSystems(emptyWorld);
            LOG_INFO("[EditorMain] Unregistered old scripted systems");
            
            // Re-discover and re-register C# systems
            // Pass emptyWorld so OnCreate is called immediately during registration
            int systemCount = engine.GetScriptManager()->RegisterScriptedSystems(engine.GetSystemManager(), &emptyWorld);
            
            if (systemCount > 0) {
                LOG_INFO("[EditorMain] Hot reload: re-discovered and initialized " << systemCount << " C# systems");
            }

            // Rebuild the editor's component registry to reflect any changes to C# component structures
            // This ensures the inspector shows the correct properties for updated components
            ComponentRegistryUI::RebuildFromNativeRegistry();
            LOG_INFO("[EditorMain] Rebuilt editor component registry after hot reload");
        });
        LOG_INFO("[EditorMain] Hot reload callback registered with ScriptManager");

        // Register the component deserialization callback so C# components can be edited in the editor
        auto deserializeCallback = scriptManager->GetDeserializeComponentFromJson();
        if (deserializeCallback) {
            RegisterComponentDeserializeCallback(deserializeCallback);
            LOG_INFO("[EditorMain] Registered component deserialize callback with editor");
        }
    }

    // Background threads for compilation/progress (declared outer so we can join on shutdown)
    std::thread bgCompileThread;
    std::thread bgProgressThread;
    // Temp output root for compiled scripts (if created) and cleanup flag
    std::filesystem::path tempRoot;
    bool shouldCleanTemp = false;
    if (scriptManager && scriptManager->IsInitialized()) {
        // Use ProjectPaths to get the current project root and watch all directories
        if (Engine::ProjectPaths::IsInitialized()) {
            std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();

            // Write compiled script assemblies to an OS temp area so they're
            // kept out of the user's project source tree and are easy to clean.
            // Use a per-project subfolder under the system temp directory.
            std::string projName = projectRoot.filename().string();
            tempRoot = std::filesystem::temp_directory_path() / "GrapeEngine" / projName;
            std::filesystem::create_directories(tempRoot);
            shouldCleanTemp = true;
            std::filesystem::path scriptsOutput = tempRoot / "GameScripts.dll";
            
            // Set the standardized output path for hot reload watcher so compilations use consistent location
            auto setOutputPath = scriptManager->GetSetOutputAssemblyPath();
            if (setOutputPath) {
                setOutputPath(scriptsOutput.string().c_str());
                LOG_INFO("[EditorMain] Set output assembly path for hot reload: " << scriptsOutput.string());
            }
            
            // Compile scripts after the editor has loaded, on a background thread,
            // so the UI becomes responsive immediately. A progress poller logs
            // compile status until compilation finishes. The file watcher is
            // started after the initial compilation completes.
            LOG_INFO("[EditorMain] Scheduling background C# script compilation...");

            std::atomic<bool> compileDone{false};
            std::thread progressThread([&]() {
                int status = 0;
                int progress = -1;
                std::string message;

                while (!compileDone.load()) {
                    scriptManager->GetCompileStatus(status, progress, message);
                    if (status == 3 || status == 4) break; // success or failure
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                }
                
                // Final read to capture terminal state
                scriptManager->GetCompileStatus(status, progress, message);
                LOG_INFO("[EditorMain] Script compile finished status=" << status << " progress=" << progress << " msg=" << message);
            });

            // Launch compilation on a worker thread so main loop can run immediately
            std::thread compileThread([scriptManager, scriptsOutput, projectRoot, &engine, &emptyWorld, &compileDone]() mutable {
                std::string diagnostics;
                try {
                    scriptManager->SetCompileStatus(1, 0, "Starting compilation");
                    LOG_INFO("[EditorMain] Background script compilation started");

                    bool compileSuccess = scriptManager->CompileScriptsWithDiagnostics(projectRoot.string(), scriptsOutput.string(), diagnostics);

                    if (compileSuccess) {
                        scriptManager->SetCompileStatus(3, 100, "Compilation successful");
                        LOG_INFO("[EditorMain] Initial script compilation succeeded");
                        
                        // Load the compiled assembly into the managed runtime so components are discovered
                        if (scriptManager->LoadAssembly(scriptsOutput.string())) {
                            LOG_INFO("[EditorMain] Loaded compiled script assembly");
                            
                            // Register discovered C# systems with the SystemManager
                            // Pass emptyWorld so OnCreate is called immediately during registration
                            int systemCount = scriptManager->RegisterScriptedSystems(engine.GetSystemManager(), &emptyWorld);
                            LOG_INFO("[EditorMain] Registered " << systemCount << " C# systems with SystemManager");
                            
                            // Rebuild the editor's component registry now that C# components are registered
                            ComponentRegistryUI::RebuildFromNativeRegistry();
                            LOG_INFO("[EditorMain] Rebuilt editor component registry with C# components");
                        }
                        else {
                            LOG_ERROR("[EditorMain] Failed to load compiled script assembly");
                        }
                    }
                    else {
                        // Set compile status/message; detailed diagnostics are logged by ScriptManager
                        scriptManager->SetCompileStatus(4, 100, diagnostics.c_str());
                    }

                    // Start file watcher for hot reload - it will handle compilation on changes
                    // Watch the entire project root so users can organize scripts however they want
                    if (scriptManager->StartScriptWatching(projectRoot.string())) {
                        LOG_INFO("[EditorMain] C# script hot reload watcher started at: " << projectRoot.string());
                    }
                    else {
                        LOG_WARNING("[EditorMain] Failed to start C# script hot reload watcher (scripts will not auto-reload on changes)");
                    }
                }
                catch (const std::exception& e) {
                    scriptManager->SetCompileStatus(4, 100, e.what());
                    LOG_ERROR("[EditorMain] Exception during background script compilation: " << e.what());
                }
                compileDone.store(true);
            });

            // Move threads into outer-scope variables so we can join them on shutdown
            bgCompileThread = std::move(compileThread);
            bgProgressThread = std::move(progressThread);
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
            engine.UpdateSystemsByMode(systemModes, world);
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

    // Ensure background compile/poller threads are finished before exiting
    try { if (bgCompileThread.joinable()) bgCompileThread.join(); } catch (...) {}
    try { if (bgProgressThread.joinable()) bgProgressThread.join(); } catch (...) {}

    // Clean up temp compiled scripts folder if we created one
    if (shouldCleanTemp) {
        try {
            if (!tempRoot.empty() && std::filesystem::exists(tempRoot)) {
                std::filesystem::remove_all(tempRoot);
                LOG_INFO("[EditorMain] Removed temp script output: " << tempRoot.string());
            }
        }
        catch (const std::exception& e) {
            LOG_WARNING("[EditorMain] Failed to remove temp script output: " << e.what());
        }
    }

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
