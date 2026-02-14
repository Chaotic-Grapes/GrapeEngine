/* Start Header *****************************************************************/
/*!
\file   EditorApplication.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implements the EditorApplication class which manages editor-specific
functionality separate from the engine core.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "EditorApplication.h"
#include "services/EditorService.h"
#include "EditorConfiguration.h"
#include "EditorState.h"
#include "core/Logger.h"
#include "core/ProjectPaths.h"
#include "EditorStyle.h"
#include "physics/Physics.h"
#include "services/TimeSystem.h"
#include "platform/IPlatformContext.h"
#include "platform/IWindow.h"
#include "scripting/ScriptManager.h"
#include <filesystem>
#include <algorithm>

// Initialize static members
Engine::Application* EditorApplication::s_editorApplication = nullptr;
ECS::World* EditorApplication::s_editorWorld = nullptr;

EditorApplication::EditorApplication(Engine::Application* engine) : m_engine(engine), m_editorService(nullptr), m_initialized(false) { }

EditorApplication::~EditorApplication() {
    if (m_initialized) {
        Shutdown();
    }
}

void EditorApplication::Initialize() {
    if (m_initialized) {
        LOG_WARNING("EditorApplication already initialized");
        return;
    }

    LOG_INFO("Initializing Editor Application...");

    // Load editor configuration
    _loadEditorSettings();

    // Create main window based on editor config
    _createMainWindow();

    // Initialize editor service
    _initializeEditorService();

    m_initialized = true;
    LOG_INFO("Editor Application initialized successfully");
}

void EditorApplication::BeginFrame() {
    if (!m_initialized) return;

    // Begin frame processing in editor service
    if (m_editorService) {
        m_editorService->BeginFrame();
    }
}

void EditorApplication::Update() {
    if (!m_initialized) return;

    // Handle script compile status during startup to determine when to show scene selection
    if (m_waitForScripts) {
        auto* scriptManager = m_engine ? m_engine->GetScriptManager() : nullptr;
        if (!scriptManager || !scriptManager->IsInitialized()) {
            return;
        }

        // Query compile status from script manager
        int status = 0;
        int progress = -1;
        std::string message;
        scriptManager->GetCompileStatus(status, progress, message);

        // Status codes:
        // 0 = Idle (no compile in progress)
        // 1 = Compile started
        // 2 = Compiling (progress updates)
        // 3 = Compile finished successfully
        // 4 = Compile finished with errors

        // If we see compile start or progress, reset idle frame counter
        if (status == 1 || status == 2) {
            m_sawCompileStart = true;
            m_compileIdleFrames = 0;
            return;
        }

        // If compile finished (success or error), proceed to scene selection
        if (status == 3 || status == 4) {
            // If compile failed, we could show an error message here.
            // For now, we proceed to scene selection regardless of compile success to allow user to fix issues.
            m_waitForScripts = false;
            m_startupStage = EditorStartupStage::SelectScene;
            m_compileIdleFrames = 0;
            return;
        }

        // If we haven't seen a compile start but status is idle, count idle frames to avoid race condition where we miss the compile start event
        if (!m_sawCompileStart && status == 0) {
            // If we've been idle for a while without seeing compile start, assume scripts are ready and move on
            ++m_compileIdleFrames;
            if (m_compileIdleFrames < 120) {
                return;
            }

            m_waitForScripts = false;
            m_startupStage = EditorStartupStage::SelectScene;
            m_compileIdleFrames = 0;
        }
    }

    // Update editor service
    if (m_editorService) {
        m_editorService->Update();
    }
}

void EditorApplication::Render() {
    if (!m_initialized) return;

    // Render editor UI
    if (m_editorService) {
        m_editorService->Render();
    }
}

void EditorApplication::EndFrame() {
    if (!m_initialized) return;

    // End frame processing in editor service
    if (m_editorService) {
        m_editorService->EndFrame();
    }
}

void EditorApplication::InitializeScriptCallbacks(ECS::ScriptManager* scriptManager, ECS::World* world) {
    if (!scriptManager) {
        LOG_WARNING("[EditorApplication] Cannot initialize script callbacks: scriptManager is null");
        return;
    }
    
    // Store global pointers for callback access (world may be updated during hot reload)
    s_editorApplication = m_engine;
    s_editorWorld = world;
    
    // Register the component removal callback for hot reload
    auto registerRemoveCallback = scriptManager->GetRegisterRemoveComponentCallback();
    if (registerRemoveCallback) {
        // Create static lambda that captures globals for callback
        static auto removalCallback = [](uint32_t componentTypeHash) {
            try {
                if (!s_editorApplication) {
                    LOG_WARNING("[EditorApplication::RemoveComponentCallback] Application not initialized");
                    return;
                }
                
                // Get the current active scene's world (or use stored world as fallback)
                ECS::World* targetWorld = s_editorWorld;
                if (auto* sceneManager = &s_editorApplication->GetSceneManager()) {
                    if (auto* activeScene = sceneManager->GetActive()) {
                        targetWorld = &activeScene->GetWorld();
                    }
                }
                
                if (!targetWorld) {
                    LOG_WARNING("[EditorApplication::RemoveComponentCallback] No target world available");
                    return;
                }
                
                LOG_INFO("[EditorApplication::RemoveComponentCallback] Removing component type 0x" << std::hex << componentTypeHash << std::dec << " from all entities");
                
                auto* scriptMgr = s_editorApplication->GetScriptManager();
                if (!scriptMgr) {
                    LOG_WARNING("[EditorApplication::RemoveComponentCallback] No script manager available");
                    return;
                }
                
                scriptMgr->RemoveComponentFromAllEntitiesByHash(*targetWorld, componentTypeHash);
            }
            catch (const std::exception& e) {
                LOG_ERROR("[EditorApplication::RemoveComponentCallback] Exception: " << e.what());
            }
            catch (...) {
                LOG_ERROR("[EditorApplication::RemoveComponentCallback] Unknown exception");
            }
        };
        
        // Wrap the lambda in a function pointer
        static void (*callbackFuncPtr)(uint32_t) = 
            [](uint32_t hash) { removalCallback(hash); };
        
        registerRemoveCallback(reinterpret_cast<void*>(callbackFuncPtr));
        LOG_INFO("[EditorApplication] Registered remove component callback with managed runtime");
    }
}

void EditorApplication::Shutdown() {
    if (!m_initialized) return;

    LOG_INFO("Shutting down Editor Application...");

    // Clean up editor service first while GL context is still active
    // This ensures ImGui and all editor resources are properly destroyed
    if (m_editorService) {
        m_editorService->Terminate();
        delete m_editorService;
        m_editorService = nullptr;
        LOG_INFO("EditorService terminated");
    }

    // Save editor settings after editor is cleaned up
    _saveEditorSettings();

    m_initialized = false;
    LOG_INFO("Editor Application shutdown complete");
}

void EditorApplication::_loadEditorSettings() {
    const std::string settingsPath = (std::filesystem::path(Engine::ProjectPaths::GetEditorDocumentsRoot())
        / "EditorSettings.json").string();

    if (!Editor::EditorConfiguration::LoadConfig(settingsPath, m_editorSettings)) {
        // Migration fallback: load legacy config.json and write to Documents.
        bool migrated = false;
        if (Editor::EditorConfiguration::LoadConfig("config.json", m_editorSettings)) {
            LOG_INFO("Loaded legacy editor configuration: config.json");
            migrated = true;
        } else if (Editor::EditorConfiguration::LoadConfig("../config.json", m_editorSettings)) {
            LOG_INFO("Loaded legacy editor configuration: ../config.json");
            migrated = true;
        }

        if (migrated) {
            Editor::EditorConfiguration::SaveConfig(settingsPath, m_editorSettings);
        }
    } else {
        LOG_INFO("Loaded editor configuration: " << settingsPath);
    }
}

void EditorApplication::_saveEditorSettings() {
    // Update settings from current window state
    auto* platformContext = m_engine->GetPlatformContext();
    if (platformContext) {
        auto* window = platformContext->GetMainWindow();

        if (window) {
            // We only update the window settings if we have a valid window, otherwise we keep the existing settings which may be defaults or from config.
            // This prevents us from overwriting valid config settings with zeros if the window was not created successfully.
            m_editorSettings.WindowSettings.Width = window->GetWidth();
            m_editorSettings.WindowSettings.Height = window->GetHeight();
            m_editorSettings.WindowSettings.Maximized = window->IsMaximized();
            m_editorSettings.WindowSettings.VSync = window->IsVSync();

            if (window->HasMode(Platform::WindowMode::Fullscreen)) {
                m_editorSettings.WindowSettings.Mode = "Fullscreen";
            }
            else if (window->HasMode(Platform::WindowMode::Borderless)) {
                m_editorSettings.WindowSettings.Mode = "Borderless";
            }
            else {
                m_editorSettings.WindowSettings.Mode = "Windowed";
            }
        }
    }

    m_editorSettings.UiScale = EditorStyle::FontScale;

    const std::string settingsPath = (std::filesystem::path(Engine::ProjectPaths::GetEditorDocumentsRoot())
        / "EditorSettings.json").string();
    Editor::EditorConfiguration::SaveConfig(settingsPath, m_editorSettings);
    LOG_INFO("Saved editor configuration: " << settingsPath);
}

void EditorApplication::_createMainWindow() {
    LOG_INFO("Creating main window from EditorSettings");

    // Get platform context from engine
    auto* platformContext = m_engine->GetPlatformContext();
    if (!platformContext) {
        LOG_ERROR("Platform context not available");
        return;
    }

    // Create window using platform abstraction
    Platform::WindowCreateInfo windowInfo;
    windowInfo.Title = "Grape Engine Editor";
    windowInfo.Width = m_editorSettings.WindowSettings.Width;
    windowInfo.Height = m_editorSettings.WindowSettings.Height;
    windowInfo.VSync = m_editorSettings.WindowSettings.VSync;
    windowInfo.Mode = Platform::WindowMode::Windowed;
    windowInfo.Resizable = true;
    windowInfo.Decorated = true;

    if (m_editorSettings.WindowSettings.Mode == "Fullscreen") {
        windowInfo.Mode = Platform::WindowMode::Fullscreen;
    } else if (m_editorSettings.WindowSettings.Mode == "Borderless") {
        windowInfo.Mode = Platform::WindowMode::Borderless;
    }

    auto* window = platformContext->CreatePlatformWindow(windowInfo);
    if (!window) {
        LOG_ERROR("Failed to create main window for editor");
        return;
    }

    window->SetMaximized(m_editorSettings.WindowSettings.Maximized);
    LOG_INFO("Main window created successfully via platform abstraction");
}

void EditorApplication::_initializeEditorService() {
    if (m_editorService) {
        LOG_WARNING("EditorService already initialized");
        return;
    }

    // Create and initialize editor service
    LOG_INFO("About to create EditorService (sceneManager=" << reinterpret_cast<void*>(&m_engine->GetSceneManager()) << ")");
    try {
        m_editorService = new Services::EditorService(m_engine->GetSceneManager());
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception creating EditorService: " << e.what());
        m_editorService = nullptr;
    }

    LOG_INFO("EditorService pointer after new: " << reinterpret_cast<void*>(m_editorService));

    // Set up editor service with settings and callbacks before enabling it
    if (m_editorService) {
        try {
            // Set global font scale before initializing editor service so it's applied to all UI elements from the start
            LOG_INFO("Calling EditorService::Initialize()");
            EditorStyle::FontScale = m_editorSettings.UiScale;
            m_editorService->Initialize();
            LOG_INFO("EditorService::Initialize returned");
        } 
        catch (const std::exception& e) {
            LOG_ERROR("Exception in EditorService::Initialize: " << e.what());
        }

        // Pass editor settings and callbacks to the editor service so it can manage the startup UI and other editor features
        m_editorService->SetEditorSettings(&m_editorSettings);
        m_editorService->SetStartupStageGetter([this]() { return m_startupStage; });
        Editor::ProjectStartupCallbacks callbacks;
        callbacks.OnProjectSelected = [this](const std::string& path) { HandleProjectSelected(path); };
        callbacks.OnSceneSelected = [this](const std::string& path) { HandleSceneSelected(path); };
        callbacks.OnContinueWithoutScene = [this]() { HandleContinueWithoutScene(); };
        m_editorService->SetProjectStartupCallbacks(callbacks);

        // Enable the editor service after initialization and callback setup is complete
        m_editorService->SetEnabled(true);
        LOG_INFO("EditorService created and initialized");
    } else {
        LOG_ERROR("EditorService pointer is null after allocation");
    }

    // Delay enabling the level editor until a project/scene is selected.
    m_editorService->DisableLevelEditor();
    LOG_INFO("Level Editor waiting for project selection");
}

// Helper to get current editor playback state, defaults to Edit mode if editor service is not available for some reason
EditorState EditorApplication::GetEditorState() const {
    if (m_editorService) {
        return m_editorService->GetPlaybackState();
    }
    return EditorState::Edit;
}

// Project and scene selection handlers called by the ProjectStartupUI callbacks to manage the startup flow of the editor
void EditorApplication::RequestProjectSelection() {
    if (!m_editorService) {
        return;
    }

    // Reset to initial state to show project selection UI
    m_startupStage = EditorStartupStage::SelectProject;
    m_projectInitialized = false;
    m_projectRoot.clear();
    m_waitForScripts = false;
    m_sawCompileStart = false;
    m_compileIdleFrames = 0;
    m_projectLoadInProgress = false;
    _clearScenes();
    m_editorService->DisableLevelEditor();
}

// Called when the user selects a project in the startup UI, initializes project paths, loads settings, and transitions to scene selection
void EditorApplication::HandleProjectSelected(const std::string& projectRoot) {
    if (projectRoot.empty()) {
        LOG_WARNING("Project selection ignored: empty path");
        return;
    }
    if (m_projectLoadInProgress) {
        LOG_WARNING("Project selection ignored: load already in progress");
        return;
    }
    m_projectLoadInProgress = true;

    // Clear any existing scenes and editor state before loading the new project
    _clearScenes();

    // Initialize project paths and settings for the selected project
    m_projectRoot = projectRoot;
    Engine::ProjectPaths::Initialize(projectRoot);
    if (m_editorService) {
        m_editorService->RequestLevelEditorRebuild();
        m_editorService->EnableLevelEditorForScene(nullptr);
    }

    if (!m_engine->LoadProjectSettings(projectRoot)) {
        LOG_WARNING("Project settings missing; creating defaults");
        m_engine->SaveProjectSettings(projectRoot);
        m_engine->LoadProjectSettings(projectRoot);
    }

    _applyProjectSettings();

    // Mark project as initialized to allow editor to proceed to scene selection
    m_projectInitialized = true;
    m_startupStage = EditorStartupStage::Booting;
    m_waitForScripts = true;
    m_sawCompileStart = false;
    m_compileIdleFrames = 0;

    // Update editor recents
    auto& recents = m_editorSettings.RecentProjects;
    recents.erase(std::remove(recents.begin(), recents.end(), projectRoot), recents.end());
    recents.insert(recents.begin(), projectRoot);
    if (recents.size() > 10) {
        recents.resize(10);
    }
    m_editorSettings.LastProject = projectRoot;
    _saveEditorSettings();
}

// Called when the user selects a scene in the startup UI, attempts to load the scene and enable the level editor for it
void EditorApplication::HandleSceneSelected(const std::string& scenePath) {
    if (!_loadSceneFromPath(scenePath)) {
        LOG_WARNING("Failed to load scene: " << scenePath);
        return;
    }

    // Enable level editor for the loaded scene so user can start editing immediately
    if (m_editorService) {
        auto* activeScene = m_engine->GetSceneManager().GetActive();
        m_editorService->EnableLevelEditorForScene(activeScene);
    }

    // Transition to ready state after scene is loaded and level editor is enabled
    m_startupStage = EditorStartupStage::Ready;
    m_projectLoadInProgress = false;
}

// Called when the user chooses to continue without selecting a scene
// Enables the level editor with no active scene (user can create a new scene or open one later)
void EditorApplication::HandleContinueWithoutScene() {
    if (m_editorService) {
        m_editorService->EnableLevelEditorForScene(nullptr);
    }
    m_startupStage = EditorStartupStage::Ready;
    m_projectLoadInProgress = false;
}

void EditorApplication::_applyProjectSettings() {
    if (!m_engine->HasProjectSettings()) {
        return;
    }

    const auto& projectSettings = m_engine->GetProjectSettings();
    Engine::Physics::SetGravity(Vector2D(0.0f, projectSettings.Physics.Gravity));
    TimeSystem::Instance().SetFixedTimeStep(projectSettings.Physics.TimeStep);
    LOG_INFO("Applied project settings: Gravity=" << projectSettings.Physics.Gravity
        << " TimeStep=" << projectSettings.Physics.TimeStep);
}

void EditorApplication::_clearScenes() {
    auto& sceneManager = m_engine->GetSceneManager();
    const size_t count = sceneManager.GetSceneCount();
    
    for (size_t idx = count; idx > 0; --idx) {
        sceneManager.RemoveScene(idx - 1);
    }
}

bool EditorApplication::_loadSceneFromPath(const std::string& scenePath) {
    if (scenePath.empty()) {
        return false;
    }

    // Create a new scene and attempt to load the selected scene file into it.
    // If loading fails, we clean up the new scene and return false to indicate failure.
    auto& sceneManager = m_engine->GetSceneManager();
    auto newScene = std::make_unique<Scenes::Scene>();
    size_t idx = sceneManager.AddScene(newScene.release());
    std::vector<uint32_t> entityOrder;

    if (sceneManager.LoadScene(idx, scenePath, &entityOrder)) {
        sceneManager.SetActive(idx);
        sceneManager.Update();
        return true;
    }

    sceneManager.RemoveScene(idx);
    return false;
}
