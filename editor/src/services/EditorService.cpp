/* Start Header *****************************************************************/
/*!
\file   EditorService.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the EditorService which manages the ImGui-based LevelEditor.
Handles ImGui frame lifecycle, scene targeting, and world propagation
to keep the editor synchronized with the active scene.

Features:
- LevelEditor creation, update, and render gating per active scene
- World reference updates propagated to editor views
- Playback state proxies (playing, step request) exposed to systems
- Window resize subscription to keep editor proportions/layout stable
- Scene attach/detach handling and minimal scene-less editor support
*/
/* End Header *******************************************************************/

#include "glad/glad.h"
#include "services/EditorService.h"
#include "scene/SceneManager.h"
#include "services/WindowManager.h"
#include "services/Input.h"
#include "core/messaging/MessageTypes.h"
#include "core/messaging/MessageSystem.h"
#include "serialization/EntitySerializer.h"
#include "services/UICommon.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>
#include "LevelEditor.h"

// Custom allocator functions for ImGui to share across DLL boundary
static void* ImGuiMallocWrapper(size_t size, void* user_data) {
    (void)user_data;
    return malloc(size);
}

static void ImGuiFreeWrapper(void* ptr, void* user_data) {
    (void)user_data;
    free(ptr);
}
#endif

using namespace Services;

#ifdef USE_IMGUI
EditorService::~EditorService() { m_editorInstance = nullptr; }

void EditorService::Initialize() {
    if (m_world) {
        UICommon::InitializeDefaultLayouts();
    }

    Window* mainWindow = WindowManager::GetMainWindow();
    if (!mainWindow) {
        LOG_WARNING("EditorService::Initialize() - No main window available");
        return;
    }

    // Create ImGui context only - defer backend initialization until first Update()
    // This ensures glfwPollEvents() has been called at least once, which is required
    // for the Win32 window procedure to be properly initialized
    if (ImGui::GetCurrentContext() == nullptr) {
        LOG_INFO("Creating ImGui context...");
        ImGui::CreateContext();
        
        // CRITICAL for DLL boundaries: Set allocator functions to ensure heap consistency
        // across Engine.dll and Editor.exe. Without this, ImGui may crash due to
        // allocating in one module and freeing in another.
        ImGui::SetAllocatorFunctions(ImGuiMallocWrapper, ImGuiFreeWrapper, nullptr);
        
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = "imgui.ini";
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }

    // Subscribe to window resize events
    if (!m_initialized) {
        Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
            [this](const Messaging::WindowResized& msg) {
                if (m_levelEditor) { m_levelEditor->OnWindowResized(msg.Width, msg.Height); }
            }
        );
        m_initialized = true;
    }
}

bool EditorService::IsGamePlaying() const { return (m_levelEditor) && m_levelEditor->IsPlaying(); }
bool EditorService::IsStepRequested() const { return (m_levelEditor) && m_levelEditor->IsStepRequested(); }
void EditorService::ClearStepRequest() const { if (m_levelEditor) m_levelEditor->ClearStepRequest(); }

void EditorService::SetWorld(ECS::World* world) {
    if (m_world == world) return;
    m_world = world;
    if (m_levelEditor) m_levelEditor->SetWorld(m_world);
}

void EditorService::Update() {
    if (!m_initialized) return;

    // Initialize ImGui backends on first Update() call (after glfwPollEvents has run)
    if (!m_backendInitialized) {
        Window* mainWindow = WindowManager::GetMainWindow();
        if (!mainWindow) return;
        
        // Ensure ImGui context is set for this DLL boundary
        // This is critical when Engine.dll and Editor.exe both use ImGui
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if (ctx) {
            ImGui::SetCurrentContext(ctx);
        }
        
        LOG_INFO("Initializing ImGui backends (deferred to first frame)...");
        ImGui_ImplGlfw_InitForOpenGL(mainWindow->Handle(), true);
        ImGui_ImplOpenGL3_Init("#version 130");
        LOG_INFO("ImGui backends initialized successfully");
        
        m_backendInitialized = true;
    }

    if (m_pendingLevelEditorRebuild && m_levelEditor) {
        LevelEditorConfig config;
        m_levelEditor.reset();
        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : m_sceneManager.GetActive();
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);
        Window* mainWindow = WindowManager::GetMainWindow();
        if (mainWindow) m_levelEditor->Initialize(mainWindow->Handle());
        m_pendingLevelEditorRebuild = false;
    }

    auto* activeScene = m_sceneManager.GetActive();

    if (m_showLevelEditor && m_levelEditorForScene && activeScene != m_levelEditorForScene) {
        DisableLevelEditor();
    }

    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    if (shouldShowLevelEditor && !m_levelEditor) {
        LevelEditorConfig config;
        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : activeScene;
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);
        Window* mainWindow = WindowManager::GetMainWindow();
        if (mainWindow) m_levelEditor->Initialize(mainWindow->Handle());
    }
}

void EditorService::Render() {
    auto* activeScene = m_sceneManager.GetActive();
    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));
    
    if (m_levelEditor && shouldShowLevelEditor && m_initialized && m_backendInitialized) {
        // Ensure correct ImGui context for this DLL boundary before every frame
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if (ctx) {
            ImGui::SetCurrentContext(ctx);
        }
        
        // Start new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        m_levelEditor->Update(); 
        m_levelEditor->Render(); 
        
        ImGui::Render();
        auto* drawData = ImGui::GetDrawData();
        if (drawData) ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
}

void EditorService::Terminate() {
    if (m_levelEditor) m_levelEditor.reset();

    if (ImGui::GetCurrentContext() != nullptr) {
        if (m_backendInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            m_backendInitialized = false;
        }
        ImGui::DestroyContext();
    }
}

void EditorService::EnableLevelEditorForScene(Scenes::Scene* scene) {
#ifdef USE_IMGUI
    m_showLevelEditor = true;
    m_levelEditorForScene = scene;
    LOG_DEBUG(scene ? "LevelEditor enabled for scene" : "LevelEditor enabled (scene-less)");
#endif
}

void EditorService::DisableLevelEditor() {
    m_showLevelEditor = false;
    m_levelEditorForScene = nullptr;
    if (m_levelEditor) m_levelEditor.reset();
    LOG_DEBUG("LevelEditor disabled");
}

#else
void EditorService::Update() {}
void EditorService::Render() {}
void EditorService::Terminate() {}
void EditorService::EnableLevelEditorForScene(Scenes::Scene* /*scene*/) {}
void EditorService::DisableLevelEditor() {}
bool EditorService::IsGamePlaying() const { return false; }
bool EditorService::IsStepRequested() const { return false; }
void EditorService::ClearStepRequest() const {}
void EditorService::SetWorld(ECS::World* world) { (void)world; }
#endif
