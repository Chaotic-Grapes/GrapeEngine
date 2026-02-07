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
#include "physics/Physics.h"
#include "platform/IPlatformContext.h"
#include "platform/IWindow.h"
#include "scripting/ScriptManager.h"
#include <filesystem>

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

    // TODO: Remove hardcoded "EchoesBelow" when editor is separated from engine
    // Initialize project paths to point to game project folder
    Engine::ProjectPaths::Initialize("EchoesBelow");

    // Load project-specific settings
    m_engine->LoadProjectSettings("EchoesBelow");

    // Apply physics settings from project configuration if available
    if (m_engine->HasProjectSettings()) {
        const auto& projectSettings = m_engine->GetProjectSettings();
        Engine::Physics::SetGravity(Vector2D(0.0f, projectSettings.Physics.Gravity));
        LOG_INFO("Applied physics gravity from ProjectSettings: " << projectSettings.Physics.Gravity);
    }

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
}

void EditorApplication::_saveEditorSettings() {
    // Update settings from current window state
    auto* platformContext = m_engine->GetPlatformContext();
    if (platformContext) {
        auto* window = platformContext->GetMainWindow();
        if (window) {
            m_editorSettings.WindowSettings.Width = window->GetWidth();
            m_editorSettings.WindowSettings.Height = window->GetHeight();
            m_editorSettings.WindowSettings.Maximized = window->IsMaximized();
            m_editorSettings.WindowSettings.VSync = window->IsVSync();
            if (window->HasMode(Platform::WindowMode::Fullscreen) &&
                window->HasMode(Platform::WindowMode::Borderless)) {
                m_editorSettings.WindowSettings.Mode = "BorderlessFullscreen";
            } else if (window->HasMode(Platform::WindowMode::Fullscreen)) {
                m_editorSettings.WindowSettings.Mode = "Fullscreen";
            } else if (window->HasMode(Platform::WindowMode::Borderless)) {
                m_editorSettings.WindowSettings.Mode = "Borderless";
            } else {
                m_editorSettings.WindowSettings.Mode = "Windowed";
            }
        }
    }

    Editor::EditorConfiguration::SaveConfig("config.json", m_editorSettings);
    LOG_INFO("Saved editor configuration");
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
    } else if (m_editorSettings.WindowSettings.Mode == "BorderlessFullscreen") {
        windowInfo.Mode = Platform::WindowMode::Borderless | Platform::WindowMode::Fullscreen;
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

    if (m_editorService) {
        try {
            LOG_INFO("Calling EditorService::Initialize()");
            m_editorService->Initialize();
            LOG_INFO("EditorService::Initialize returned");
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in EditorService::Initialize: " << e.what());
        }

        m_editorService->SetEnabled(true);
        LOG_INFO("EditorService created and initialized");
    } else {
        LOG_ERROR("EditorService pointer is null after allocation");
    }

    // Enable level editor without a scene (scene-less editor mode)
    m_editorService->EnableLevelEditorForScene(nullptr);
    LOG_INFO("Level Editor initialized successfully");
}

EditorState EditorApplication::GetEditorState() const {
    if (m_editorService) {
        return m_editorService->GetPlaybackState();
    }
    return EditorState::Edit;
}
