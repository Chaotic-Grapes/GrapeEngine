#include "services/OverlayService.h"
#include "scene/SceneManager.h"
#include "services/WindowManager.h"
#include "services/DebugUI.h"
#include "services/Input.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include "core/messaging/MessageTypes.h"
#include "core/messaging/MessageSystem.h"
#include "../../include/editor/LevelEditor.h"
#include "serialization/EntitySerializer.h"

#ifdef USE_IMGUI

namespace Services {
    #ifdef USE_IMGUI
    OverlayService::~OverlayService() { m_overlayInstance = nullptr; }
    #endif
    void OverlayService::Initialize() {
        // Initialize DebugUI (always available)
        if (m_world) {
            m_debugUI = std::make_unique<DebugUI>(m_world);
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
                io.IniFilename = "imgui.ini"; // Persist dock layouts and window positions to disk
                m_initialized = true;

                // Subscribe to window resize to keep editor proportions stable
                Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
                    [this](const Messaging::WindowResized& msg) {
                        if (m_levelEditor) {
                            m_levelEditor->OnWindowResized(msg.Width, msg.Height);
                            // Force a rebuild of the docking layout
                            static_cast<void>(0); // You might need to add a public method to LevelEditor to force layout rebuild
                        }
                    }
                );
                // Dump component TypeId registry once for copying into prefabs
                static bool s_dumped = false;
                if (!s_dumped) {
                    auto& reg = Serialization::EntitySerializer::Registry();
                    for (const auto& [tid, info] : reg) {
                        LOG_DEBUG(std::string("TypeId dump: ") + info.Name + " -> " + std::to_string(tid));
                    }
                    s_dumped = true;
                }
            }
        }
    }

    bool OverlayService::IsGamePlaying() const {
        return m_levelEditor && m_levelEditor->IsPlaying();
    }

    bool OverlayService::IsStepRequested() const {
        return m_levelEditor && m_levelEditor->IsStepRequested();
    }

    void OverlayService::ClearStepRequest() const {
        if (m_levelEditor) m_levelEditor->ClearStepRequest();
    }

    void OverlayService::SetWorld(ECS::World* world) {
        if (m_world == world) return;
        m_world = world;
        // Propagate world update directly to avoid stale references
        if (m_levelEditor != nullptr) {
            m_levelEditor->SetWorld(m_world);
        }
        if (m_debugUI != nullptr) {
            m_debugUI->SetWorld(m_world);
        }
    }

    void OverlayService::Update() {
        // Handle deferred LevelEditor rebuild at the start of the frame
        if (m_pendingLevelEditorRebuild && m_levelEditor) {
            LevelEditorConfig config;
            m_levelEditor.reset();
            m_levelEditor = std::make_unique<LevelEditor>(m_world, config);
            Window* mainWindow = WindowManager::GetMainWindow();
            if (mainWindow && m_initialized) {
                m_levelEditor->Initialize(mainWindow->Handle());
            }
            m_pendingLevelEditorRebuild = false;
        }

        if (Input::IsKeyPressed(KEY_F1) && m_debugUI)
            m_debugUI->SetEnabled(!m_debugUI->IsEnabled());

        auto* activeScene = m_sceneManager.GetActive();

        // Disable LevelEditor if scene changes (only when targeted to a specific scene)
        if (m_showLevelEditor && m_levelEditorForScene && activeScene != m_levelEditorForScene) {
            DisableLevelEditor();
        }

        // Show LevelEditor when enabled either for a specific scene or globally (scene-less)
        bool shouldShowLevelEditor = m_showLevelEditor && (
            m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene)
            );

        // Initialize LevelEditor only when needed (even without a world)
        if (shouldShowLevelEditor && !m_levelEditor) {
            LevelEditorConfig config;
            m_levelEditor = std::make_unique<LevelEditor>(m_world, config);

            Window* mainWindow = WindowManager::GetMainWindow();
            if (mainWindow && m_initialized) {
                m_levelEditor->Initialize(mainWindow->Handle());
            }
        }

        if (!m_debugUI && m_world) {
            m_debugUI = std::make_unique<DebugUI>(m_world);
        }

        if (!m_debugUI) return;

        if (!m_initialized) {
            Window* mainWindow = WindowManager::GetMainWindow();
            if (mainWindow) {
                if (m_debugUI)
                    m_debugUI->Initialize(mainWindow->Handle());
                if (m_levelEditor && shouldShowLevelEditor)
                    m_levelEditor->Initialize(mainWindow->Handle());
                if (m_audioDevice && m_debugUI)
                    m_debugUI->AttachAudio(m_audioDevice);
                m_initialized = true;
            }
            else return;
        }

        if (m_audioDevice && m_debugUI && !m_debugUI->HasValidAudio())
            m_debugUI->AttachAudio(m_audioDevice);

        if (activeScene && m_debugUI && !m_debugUI->HasValidScene(activeScene))
            m_debugUI->AttachScene(activeScene);
        else if (m_debugUI && m_debugUI->HasValidScene() && !activeScene)
            m_debugUI->DetachScene();
    }

    void OverlayService::Render() {
        auto* activeScene = m_sceneManager.GetActive();
        bool shouldShowLevelEditor = m_showLevelEditor && (
            m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene)
            );

        // Background clear hack removed; dockspace now draws its own background

        // Start a new ImGui frame via DebugUI (sets up IO and backend state)
        if (m_debugUI) { m_debugUI->NewFrame(); }

        // LevelEditor takes priority
        if (m_levelEditor && shouldShowLevelEditor && m_initialized) {
            m_levelEditor->Update();
            m_levelEditor->Render();
        }
        // DebugUI only when LevelEditor isn't showing
        else if (m_debugUI && m_debugUI->IsEnabled()) {
            m_debugUI->Render();
        }

        ImGui::Render(); // Finalize draw lists for the current frame
        auto* drawData = ImGui::GetDrawData();
        if (drawData) {
            ImGui_ImplOpenGL3_RenderDrawData(drawData); // Submit ImGui draw lists to OpenGL
        }
    }

    void OverlayService::Terminate() {
        if (m_debugUI) {
            m_debugUI->Shutdown();
            m_debugUI->DetachAudio();
        }
        if (m_levelEditor) {
            m_levelEditor.reset();
        }
    }

    void OverlayService::EnableLevelEditorForScene(Scenes::Scene* scene) {
#ifdef USE_IMGUI
        // Enable for the specified scene; nullptr enables scene-less mode
        m_showLevelEditor = true;
        m_levelEditorForScene = scene; // nullptr means scene-less mode
        LOG_DEBUG(scene ? "LevelEditor enabled for scene" : "LevelEditor enabled (scene-less)");
#endif
    }

    void OverlayService::DisableLevelEditor() {
        m_showLevelEditor = false;
        m_levelEditorForScene = nullptr;

        if (m_levelEditor) {
            m_levelEditor.reset();
        }

        LOG_DEBUG("LevelEditor disabled");
    }

#else
void OverlayService::Update() {}
void OverlayService::Render() {}
void OverlayService::Terminate() {}
void OverlayService::EnableLevelEditorForScene(Scenes::Scene* scene) {}
void OverlayService::DisableLevelEditor() {}
bool OverlayService::IsGamePlaying() const { return false; }
bool OverlayService::IsStepRequested() const { return false; }
void OverlayService::ClearStepRequest() const {}
void OverlayService::SetWorld(ECS::World* world) { m_world = world; }
#endif
}
