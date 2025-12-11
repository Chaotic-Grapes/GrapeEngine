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
#include "EditorState.h"
#include "scene/SceneManager.h"
#include "core/Application.h"
#include "platform/IPlatformContext.h"
#include "platform/IWindow.h"
#include "services/Input.h"
#include "services/TimeSystem.h"
#include "core/messaging/MessageTypes.h"
#include "core/messaging/MessageSystem.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include "services/UICommon.h"
#include <sstream>
#include <iomanip>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <Windows.h>
#endif

// Undefine common Windows macros that conflict with our logging enums/macros
#ifdef ERROR
#undef ERROR
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef INFO
#undef INFO
#endif

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>
#include "LevelEditor.h"
#include "EditorStyle.h"
#endif

using namespace Services;

#ifdef USE_IMGUI
EditorService::~EditorService() { m_editorInstance = nullptr; }

void EditorService::Initialize() {
    LOG_INFO("EditorService::Initialize ENTRY - this=" << reinterpret_cast<void*>(this) << ", m_world=" << reinterpret_cast<void*>(m_world));
    try {
        if (m_world) {
            LOG_INFO("EditorService::Initialize - calling UICommon::InitializeDefaultLayouts");
            UICommon::InitializeDefaultLayouts();
        }

    if (!Engine::CORE) {
        LOG_ERROR("EditorService::Initialize() - Engine CORE not available");
        return;
    }

    auto* platformContext = Engine::CORE->GetPlatformContext();
    if (!platformContext) {
        LOG_ERROR("EditorService::Initialize() - Platform context not available");
        return;
    }

    auto* mainWindow = platformContext->GetMainWindow();
    if (!mainWindow) {
        LOG_WARNING("EditorService::Initialize() - No main window available");
        return;
    }

    LOG_INFO("EditorService::Initialize - platformContext=" << reinterpret_cast<void*>(platformContext)
             << ", mainWindow=" << reinterpret_cast<void*>(mainWindow)
             << ", nativeHandle=" << reinterpret_cast<void*>(mainWindow->GetNativeHandle()));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    // Enable docking, but disable multi-viewport to avoid the ImGui GLFW
    // backend attempting to install Win32 WndProc hooks (engine already
    // manages GLFW callbacks and that caused assertion failures).
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
    io.FontGlobalScale = EditorStyle::FontScale;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.TabBarBorderSize = 0.0f;

    GLFWwindow* glfwHandle = static_cast<GLFWwindow*>(mainWindow->GetNativeHandle());
    if (!glfwHandle) {
        LOG_ERROR("EditorService::Initialize() - GLFW native handle is null; cannot initialize ImGui");
        return;
    }

    // Make sure the window's context is current before initializing GL loader/backends
    glfwMakeContextCurrent(glfwHandle);

    // Ensure GL functions are available (GLAD). Window creation normally initializes GLAD,
    // but re-check here to be safe in case of ordering differences during refactor.
    if (!gladLoadGL()) {
        LOG_ERROR("EditorService::Initialize() - gladLoadGL() failed; OpenGL functions unavailable");
        return;
    }

    // We avoid using ImGui's GLFW platform backend (it installs Win32 hooks
    // which conflict with our input system). Instead we initialize only the
    // OpenGL renderer backend and provide minimal platform data ourselves.
    ImGui_ImplOpenGL3_Init("#version 460");
    ImGuiIO& io2 = ImGui::GetIO();
    io2.BackendPlatformName = "GrapeEngine_Custom";
    io2.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // we can honor cursors
    m_backendInitialized = true;

    if (!m_initialized) {
        Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
            [this](const Messaging::WindowResized& msg) {
                if (m_levelEditor) { m_levelEditor->OnWindowResized(msg.Width, msg.Height); }
            }
        );
        m_initialized = true;
    }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in EditorService::Initialize(): " << e.what());
    } catch (...) {
        LOG_ERROR("Unknown exception in EditorService::Initialize()");
    }
}

bool EditorService::IsGamePlaying() const { return (m_levelEditor) && m_levelEditor->IsPlaying(); }
bool EditorService::IsStepRequested() const { return (m_levelEditor) && m_levelEditor->IsStepRequested(); }
void EditorService::ClearStepRequest() const { if (m_levelEditor) m_levelEditor->ClearStepRequest(); }

EditorState EditorService::GetPlaybackState() const { 
    return (m_levelEditor) ? m_levelEditor->GetEditorState() : EditorState::Edit; 
}

void EditorService::SetWorld(ECS::World* world) {
    if (m_world == world)
        return;

    m_world = world;

    if (m_levelEditor) {
        // Also propagate the active Scenes::Scene so EntityActions is updated
        Scenes::Scene* activeScene = m_sceneManager.GetActive();
        m_levelEditor->SetScene(activeScene);
    }
}

void EditorService::Update() {
    if (!m_initialized) return;

    // Ensure the SceneManager processes any pending transitions so the editor
    // immediately sees newly created scenes (e.g. File->New Scene).
    // Calling Update here is idempotent and cheap (it only processes pending
    // transitions) and guarantees the editor's world is set correctly.
    m_sceneManager.Update();

    if (m_pendingLevelEditorRebuild && m_levelEditor) {
        LevelEditorConfig config;
        m_levelEditor.reset();

        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : m_sceneManager.GetActive();
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);

        if (Engine::CORE) {
            auto* platformContext = Engine::CORE->GetPlatformContext();
            if (platformContext) {
                auto* mainWindow = platformContext->GetMainWindow();
                if (mainWindow) {
                    m_levelEditor->Initialize(static_cast<GLFWwindow*>(mainWindow->GetNativeHandle()));
                }
            }
        }

        m_pendingLevelEditorRebuild = false;
    }

    auto* activeScene = m_sceneManager.GetActive();

    // Propagate active scene's world to editor panels so UI updates immediately
    // when a new scene is created or activated.
    ECS::World* activeWorld = activeScene ? &activeScene->GetWorld() : nullptr;
    SetWorld(activeWorld);

    if (m_showLevelEditor && m_levelEditorForScene && activeScene != m_levelEditorForScene) {
        DisableLevelEditor();
    }

    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    if (shouldShowLevelEditor && !m_levelEditor) {
        LevelEditorConfig config;
        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : activeScene;
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);

        if (Engine::CORE) {
            auto* platformContext = Engine::CORE->GetPlatformContext();
            if (platformContext) {
                auto* mainWindow = platformContext->GetMainWindow();
                if (mainWindow) {
                    m_levelEditor->Initialize(static_cast<GLFWwindow*>(mainWindow->GetNativeHandle()));
                }
            }
        }
    }
}

void EditorService::Render() {
    auto* activeScene = m_sceneManager.GetActive();
    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));
    
    if (m_levelEditor && shouldShowLevelEditor && m_initialized && m_backendInitialized) {
        // Minimal platform glue: update ImGui IO from our Input and Time systems
        ImGuiIO& io = ImGui::GetIO();
        auto* platformContext = Engine::CORE ? Engine::CORE->GetPlatformContext() : nullptr;
        if (platformContext) {
            if (auto* w = platformContext->GetMainWindow()) {
                io.DisplaySize = ImVec2(static_cast<float>(w->GetWidth()), static_cast<float>(w->GetHeight()));
            }
        }
        io.DeltaTime = static_cast<float>(TimeSystem::Instance().GetUnscaledDeltaTime());

        double mx, my; Input::GetMousePosition(mx, my);
        io.MousePos = ImVec2(static_cast<float>(mx), static_cast<float>(my));
        io.MouseDown[0] = Input::IsMouseDown(MOUSE_LEFT);
        io.MouseDown[1] = Input::IsMouseDown(MOUSE_RIGHT);
        io.MouseDown[2] = Input::IsMouseDown(MOUSE_MIDDLE);

        // Feed keyboard state to ImGui
        io.KeyCtrl = Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL);
        io.KeyShift = Input::IsKeyDown(KEY_LEFT_SHIFT) || Input::IsKeyDown(KEY_RIGHT_SHIFT);
        io.KeyAlt = Input::IsKeyDown(KEY_LEFT_ALT) || Input::IsKeyDown(KEY_RIGHT_ALT);
        io.KeySuper = Input::IsKeyDown(KEY_LEFT_SUPER) || Input::IsKeyDown(KEY_RIGHT_SUPER);

        // Feed key presses to ImGui for input handling
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Tab, Input::IsKeyDown(KEY_TAB));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftArrow, Input::IsKeyDown(KEY_LEFT));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, Input::IsKeyDown(KEY_RIGHT));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_UpArrow, Input::IsKeyDown(KEY_UP));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_DownArrow, Input::IsKeyDown(KEY_DOWN));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_PageUp, Input::IsKeyDown(KEY_PAGE_UP));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_PageDown, Input::IsKeyDown(KEY_PAGE_DOWN));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Home, Input::IsKeyDown(KEY_HOME));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_End, Input::IsKeyDown(KEY_END));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Insert, Input::IsKeyDown(KEY_INSERT));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Delete, Input::IsKeyDown(KEY_DELETE));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Backspace, Input::IsKeyDown(KEY_BACKSPACE));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, Input::IsKeyDown(KEY_ENTER));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, Input::IsKeyDown(KEY_ESCAPE));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_A, Input::IsKeyDown(KEY_A));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_C, Input::IsKeyDown(KEY_C));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_V, Input::IsKeyDown(KEY_V));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_X, Input::IsKeyDown(KEY_X));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Y, Input::IsKeyDown(KEY_Y));
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Z, Input::IsKeyDown(KEY_Z));

        // Feed character input to ImGui
        const std::string& charInput = Input::GetCharInput();
        for (unsigned char c : charInput) {
            io.AddInputCharacter(c);
        }
        Input::ClearCharInput();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
        
        m_levelEditor->Update(); 
        m_levelEditor->Render(); 
        
        ImGui::Render();
        ImGui::EndFrame();
        auto* drawData = ImGui::GetDrawData();
        if (drawData)
            ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
}

void EditorService::Terminate() {
    if (m_levelEditor) m_levelEditor.reset();

    if (ImGui::GetCurrentContext() != nullptr) {
        if (m_backendInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
            // We only initialize the OpenGL renderer backend (see Initialize()).
            // Do not call ImGui_ImplGlfw_Shutdown() because we do not use
            // ImGui's GLFW platform backend (it would attempt to shutdown
            // a backend that was never initialized and trigger assertions).
            m_backendInitialized = false;
        }
        ImGui::DestroyContext();
    }
}

void EditorService::EnableLevelEditorForScene(Scenes::Scene* scene) {
    m_showLevelEditor = true;
    m_levelEditorForScene = scene;
    LOG_DEBUG(scene ? "LevelEditor enabled for scene" : "LevelEditor enabled (scene-less)");
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
EditorState EditorService::GetPlaybackState() const { return EditorState::Edit; }
void EditorService::SetWorld(ECS::World* world) { (void)world; }
#endif
