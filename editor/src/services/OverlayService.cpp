/* Start Header *****************************************************************/
/*!
\file   OverlayService.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the OverlayService which bridges DebugUI and the ImGui-based
LevelEditor overlay. Manages ImGui frame lifecycle, scene targeting,
and world propagation so editor and debug views stay synchronized.

Features:
- DebugUI lifecycle and ImGui backend integration
- LevelEditor creation, update, and render gating per active scene
- World reference updates propagated to both editor and debug views
- Playback state proxies (playing, step request) exposed to systems
- Window resize subscription to keep editor proportions/layout stable
- Scene attach/detach handling and minimal scene-less editor support
*/
/* End Header *******************************************************************/

#include "glad/glad.h"
#include "editor/services/OverlayService.h"
#include "scene/SceneManager.h"
#include "services/WindowManager.h"
#include "services/Input.h"
#include "core/messaging/MessageTypes.h"
#include "core/messaging/MessageSystem.h"
#include "serialization/EntitySerializer.h"
#include "services/UICommon.h"

#ifdef USE_IMGUI
#include "services/DebugUI.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include "LevelEditor.h"
#endif

using namespace Services;

#ifdef USE_IMGUI
OverlayService::~OverlayService() { m_overlay_instance = nullptr; }

void OverlayService::Initialize() {
    if (m_world) {
        m_debugUI = std::make_unique<DebugUI>(m_world);
        UICommon::InitializeDefaultLayouts();
    }
    if (!m_debugUI)
        m_debugUI = std::make_unique<DebugUI>(nullptr);

    if (!m_initialized) {
        Window* mainWindow = WindowManager::GetMainWindow();
        if (mainWindow) {
            if (m_debugUI)
                m_debugUI->Initialize(mainWindow->Handle());
            if (m_audioDevice && m_debugUI)
                m_debugUI->AttachAudio(m_audioDevice);
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = "imgui.ini";
            m_initialized = true;
            Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
                [this](const Messaging::WindowResized& msg) {
                    if (m_levelEditor) { m_levelEditor->OnWindowResized(msg.Width, msg.Height); }
                }
            );
        }
    }
}

bool OverlayService::IsGamePlaying() const { return (m_levelEditor) && m_levelEditor->IsPlaying(); }
bool OverlayService::IsStepRequested() const { return (m_levelEditor) && m_levelEditor->IsStepRequested(); }
void OverlayService::ClearStepRequest() const { if (m_levelEditor) m_levelEditor->ClearStepRequest(); }

void OverlayService::SetWorld(ECS::World* world) {
    if (m_world == world) return;
    m_world = world;
    if (m_levelEditor) m_levelEditor->SetWorld(m_world);
    if (m_debugUI) m_debugUI->SetWorld(m_world);
}

void OverlayService::Update() {
    if (m_pendingLevelEditorRebuild && m_levelEditor) {
        LevelEditorConfig config;
        m_levelEditor.reset();
        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : m_sceneManager.GetActive();
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);
        Window* mainWindow = WindowManager::GetMainWindow();
        if (mainWindow && m_initialized) m_levelEditor->Initialize(mainWindow->Handle());
        m_pendingLevelEditorRebuild = false;
    }

    if (Input::IsKeyPressed(KEY_F1) && m_debugUI) m_debugUI->SetEnabled(!m_debugUI->IsEnabled());

    auto* activeScene = m_scene_manager.GetActive();

    if (m_showLevelEditor && m_levelEditorForScene && activeScene != m_levelEditorForScene) DisableLevelEditor();

    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    if (shouldShowLevelEditor && !m_levelEditor) {
        LevelEditorConfig config;
        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : activeScene;
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);
        Window* mainWindow = WindowManager::GetMainWindow();
        if (mainWindow && m_initialized) m_levelEditor->Initialize(mainWindow->Handle());
    }

    if (!m_debugUI && m_world) m_debugUI = std::make_unique<DebugUI>(m_world);
    if (!m_debugUI) return;

    if (!m_initialized) {
        Window* mainWindow = WindowManager::GetMainWindow();
        if (mainWindow) {
            if (m_debugUI) m_debugUI->Initialize(mainWindow->Handle());
            if (m_levelEditor && shouldShowLevelEditor) m_levelEditor->Initialize(mainWindow->Handle());
            if (m_audioDevice && m_debugUI) m_debugUI->AttachAudio(m_audioDevice);
            m_initialized = true;
        } else return;
    }

    if (m_audioDevice && m_debugUI && !m_debugUI->HasValidAudio()) m_debugUI->AttachAudio(m_audioDevice);
    if (activeScene && m_debugUI && !m_debugUI->HasValidScene(activeScene)) m_debugUI->AttachScene(activeScene);
    else if (m_debugUI && m_debugUI->HasValidScene() && !activeScene) m_debugUI->DetachScene();
}

void OverlayService::Render() {
    auto* activeScene = m_scene_manager.GetActive();
    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));
    if (m_debugUI) { m_debugUI->NewFrame(); }
    if (m_levelEditor && shouldShowLevelEditor && m_initialized) { m_levelEditor->Update(); m_levelEditor->Render(); }
    else if (m_debugUI && m_debugUI->IsEnabled()) { m_debugUI->Render(); }
    ImGui::Render();
    auto* drawData = ImGui::GetDrawData();
    if (drawData) ImGui_ImplOpenGL3_RenderDrawData(drawData);
}

void OverlayService::Terminate() {
    if (m_debugUI) { m_debugUI->Shutdown(); m_debugUI->DetachAudio(); }
    if (m_levelEditor) m_levelEditor.reset();
}

void OverlayService::EnableLevelEditorForScene(Scenes::Scene* scene) {
#ifdef USE_IMGUI
    m_showLevelEditor = true;
    m_levelEditorForScene = scene;
    LOG_DEBUG(scene ? "LevelEditor enabled for scene" : "LevelEditor enabled (scene-less)");
#endif
}

void OverlayService::DisableLevelEditor() {
    m_showLevelEditor = false;
    m_levelEditorForScene = nullptr;
    if (m_levelEditor) m_levelEditor.reset();
    LOG_DEBUG("LevelEditor disabled");
}

#else
void OverlayService::Update() {}
void OverlayService::Render() {}
void OverlayService::Terminate() {}
void OverlayService::EnableLevelEditorForScene(Scenes::Scene* /*scene*/) {}
void OverlayService::DisableLevelEditor() {}
bool OverlayService::IsGamePlaying() const { return false; }
bool OverlayService::IsStepRequested() const { return false; }
void OverlayService::ClearStepRequest() const {}
void OverlayService::SetWorld(ECS::World* world) { (void)world; }
#endif
