/* Start Header *****************************************************************/
/*!
\file   EditorService.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   12th March 2026
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
#include "services/ResourceManager.h"
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
namespace {

    // Baseline sizes used to bootstrap UI text and icon fonts before user settings apply
    constexpr float kDefaultTextFontSize = 20.0f;
    constexpr float kDefaultIconFontSize = 22.0f;

    // Loads font bytes through ResourceManager with a direct-file fallback to keep editor startup resilient
    ImFont* LoadFontFromRM(ImFontAtlas& atlas, const std::string& path, float size,
        ImFontConfig* config = nullptr, const ImWchar* ranges = nullptr) {

        // Try packaged/managed asset path first so startup follows project resource pipeline
        auto raw = RM.Get<RawData>(path);

        // Fallback to direct file loading if resource manager cannot provide valid bytes
        if (!raw || !raw->IsValid) {
            LOG_WARNING("Failed to load font via RM, falling back to file: " << path);

            // Clone optional caller config so local edits do not mutate external config state
            ImFontConfig cfg = config ? *config : ImFontConfig();

            // Store source path in config name to make font-atlas debugging easier
            strncpy_s(cfg.Name, path.c_str(), sizeof(cfg.Name) - 1);
            return atlas.AddFontFromFileTTF(path.c_str(), size, &cfg, ranges);
        }

        // Resource manager returned valid bytes, register memory font in atlas
        ImFontConfig cfg = config ? *config : ImFontConfig();

        // Data ownership stays with RawData container, atlas only borrows the pointer
        cfg.FontDataOwnedByAtlas = false;
        strncpy_s(cfg.Name, path.c_str(), sizeof(cfg.Name) - 1);

        return atlas.AddFontFromMemoryTTF(raw->Data.data(), (int)raw->Data.size(), size, &cfg, ranges);
    }

    // Ensures startup atlas has base text and icon fonts so editor UI is usable before settings override
    void EnsureStartupFontsLoaded(ImGuiIO& io) {

        // Skip if atlas already has fonts to avoid duplicate registration on repeated init calls
        if (!io.Fonts || !io.Fonts->Fonts.empty()) {
            return;
        }

        // Main UI text face
        ImFont* mainFont = LoadFontFromRM(*io.Fonts,
            "assets/fonts/Inter/static/Inter_24pt-Medium.ttf",
            kDefaultTextFontSize);
        if (!mainFont) {
            mainFont = io.Fonts->AddFontDefault();
        }

        // Bold variant used by emphasis labels and headers
        ImFont* boldFont = LoadFontFromRM(*io.Fonts,
            "assets/fonts/Inter/static/Inter_24pt-ExtraBold.ttf",
            kDefaultTextFontSize);
        if (!boldFont) {
            boldFont = io.Fonts->AddFontDefault();
        }

        // Material Symbols private-use codepoint range
        static constexpr ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };
        ImFontConfig iconsConfig;
        iconsConfig.MergeMode = false;
        iconsConfig.PixelSnapH = true;
        iconsConfig.OversampleH = 3;
        iconsConfig.OversampleV = 3;

        // Merge icon glyphs into text atlas once so mixed text/icon labels render in one draw pipeline
        static ImFontAtlas* s_mergedAtlas = nullptr;
        if (mainFont && s_mergedAtlas != io.Fonts) {
            ImFontConfig mergeConfig = iconsConfig;
            mergeConfig.MergeMode = true;

            // Merge pass writes icon glyphs into the existing atlas texture
            LoadFontFromRM(*io.Fonts,
                "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
                kDefaultTextFontSize,
                &mergeConfig,
                iconRanges);
            s_mergedAtlas = io.Fonts;
        }

        // Keep a standalone icon face too for explicit icon-only drawing code paths
        ImFont* symbolsFont = LoadFontFromRM(*io.Fonts,
            "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
            kDefaultIconFontSize,
            &iconsConfig,
            iconRanges);
        if (!symbolsFont) {
            symbolsFont = io.Fonts->AddFontDefault();
        }

        (void)mainFont;
        (void)boldFont;
        (void)symbolsFont;

    }
}

// Releases EditorService-owned pointers on destruction
EditorService::~EditorService() { m_editorInstance = nullptr; }

// Injects startup-stage getter used by startup UI to drive render gating logic
void EditorService::SetStartupStageGetter(std::function<EditorStartupStage()> getter) {
    m_projectStartupUI.SetStageGetter(std::move(getter));
}

// Forwards startup callbacks so project startup UI can invoke editor actions
void EditorService::SetProjectStartupCallbacks(const Editor::ProjectStartupCallbacks& callbacks) {
    m_projectStartupUI.SetCallbacks(callbacks);
}

// Updates editor settings references in startup UI and active level editor
void EditorService::SetEditorSettings(EditorSettings* settings) {
    m_editorSettings = settings;
    m_projectStartupUI.SetEditorSettings(settings);

    // Push settings immediately to active level editor so UI scale and prefs update live
    if (m_levelEditor) {
        m_levelEditor->SetEditorSettings(settings);
    }
}

// Requests project browser startup screen
void EditorService::RequestProjectBrowser() {
    m_projectStartupUI.RequestProjectBrowser();
}

// Marks level editor for safe rebuild on next update tick
void EditorService::RequestLevelEditorRebuild() {
    m_pendingLevelEditorRebuild = true;
}

// Initializes ImGui runtime and editor integration backend state
void EditorService::Initialize() {    

    // Prevent double initialization
    if (m_initialized || m_backendInitialized) {
        LOG_WARNING("[EditorService] Initialize() - Already initialized, skipping");
        return;
    }

    try {
        if (m_world) {
            UICommon::InitializeDefaultLayouts();
        }

        if (!Engine::CORE) {
            LOG_ERROR("[EditorService] Initialize() - Engine CORE not available");
            return;
        }

        auto* platformContext = Engine::CORE->GetPlatformContext();
        if (!platformContext) {
            LOG_ERROR("[EditorService] Initialize() - Platform context not available");
            return;
        }

        auto* mainWindow = platformContext->GetMainWindow();
        if (!mainWindow) {
            LOG_WARNING("[EditorService] No main window available");
            return;
        }
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();

        // Enable docking, but disable multi-viewport to avoid the ImGui GLFW
        // backend attempting to install Win32 WndProc hooks (engine already
        // manages GLFW callbacks and that caused assertion failures)
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        io.FontGlobalScale = EditorStyle::FontScale;

        // Load baseline fonts before first frame so startup and editor windows use consistent metrics
        EnsureStartupFontsLoaded(io);

        ImGuiStyle& style = ImGui::GetStyle();

        // Apply shared editor theme once during backend init
        EditorStyle::ApplyModernTheme(style);

        GLFWwindow* glfwHandle = static_cast<GLFWwindow*>(mainWindow->GetNativeHandle());
        if (!glfwHandle) {
            LOG_ERROR("[EditorService] GLFW native handle is null; cannot initialize ImGui");
            ImGui::DestroyContext();
            return;
        }

        // Make sure the window's context is current before initializing GL loader/backends
        glfwMakeContextCurrent(glfwHandle);

        // Ensure GL functions are available (GLAD), window creation normally initializes GLAD
        // but re-check here to be safe in case of ordering differences during refactor
        if (!gladLoadGL()) {
            LOG_ERROR("[EditorService] gladLoadGL() failed");
            ImGui_ImplOpenGL3_Shutdown();
            ImGui::DestroyContext();
            return;
        }

        // We avoid using ImGui's GLFW platform backend (it installs Win32 hooks
        // which conflict with our input system), instead initialize only the
        // OpenGL renderer backend and provide minimal platform data ourselves
        ImGui_ImplOpenGL3_Init("#version 460");
        ImGuiIO& io2 = ImGui::GetIO();
        io2.BackendPlatformName = "GrapeEngine_Custom";

        // Mark cursor support so ImGui can request cursor shape changes when needed
        io2.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

        // Enable GPU timing only when timer queries are supported by this GLAD build/runtime.
        const bool hasGl33 =
    #ifdef GLAD_GL_VERSION_3_3
            (GLAD_GL_VERSION_3_3 != 0);
    #else
            false;
    #endif

        const bool hasArbTimerQuery =
    #ifdef GLAD_GL_ARB_timer_query
            (GLAD_GL_ARB_timer_query != 0);
    #else
            false;
    #endif

        m_gpuTimingSupported = hasGl33 || hasArbTimerQuery;
        m_sceneGpuQuery = 0u;
        m_imguiGpuQuery = 0u;
        m_sceneGpuQueryPending = false;
        m_imguiGpuQueryPending = false;
        m_sceneGpuMs = 0.0f;
        m_imguiGpuMs = 0.0f;
        if (m_gpuTimingSupported) {
            glGenQueries(1, &m_sceneGpuQuery);
            glGenQueries(1, &m_imguiGpuQuery);
            m_gpuTimingSupported = (m_sceneGpuQuery != 0u) && (m_imguiGpuQuery != 0u);

            if (!m_gpuTimingSupported) {
                if (m_sceneGpuQuery != 0u) {
                    glDeleteQueries(1, &m_sceneGpuQuery);
                    m_sceneGpuQuery = 0u;
                }
                if (m_imguiGpuQuery != 0u) {
                    glDeleteQueries(1, &m_imguiGpuQuery);
                    m_imguiGpuQuery = 0u;
                }
            }
        }

        m_backendInitialized = true;

        if (!m_initialized) {
            Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
                [this](const Messaging::WindowResized& msg) {

                    // Forward resize to level editor so docking and panel layout remain in sync
                    if (m_levelEditor) { m_levelEditor->OnWindowResized(msg.Width, msg.Height); }
                }
            );
            m_initialized = true;

            LOG_INFO("Running ImGui version " << IMGUI_VERSION);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception in EditorService::Initialize(): " << e.what());

        // Clean up partially initialized state
        if (m_backendInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
            m_backendInitialized = false;
        }
        if (ImGui::GetCurrentContext() != nullptr) {
            ImGui::DestroyContext();
        }
    }
    catch (...) {
        LOG_ERROR("Unknown exception in EditorService::Initialize()");

        // Clean up partially initialized state
        if (m_backendInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
            m_backendInitialized = false;
        }
        if (ImGui::GetCurrentContext() != nullptr) {
            ImGui::DestroyContext();
        }
    }
}

// Returns true when level editor playback is in Play state
bool EditorService::IsGamePlaying() const { return (m_levelEditor) && m_levelEditor->IsPlaying(); }

// Returns whether one-frame step was requested through playback controls
bool EditorService::IsStepRequested() const { return (m_levelEditor) && m_levelEditor->IsStepRequested(); }

// Clears pending one-frame step request in playback controls
void EditorService::ClearStepRequest() const { if (m_levelEditor) m_levelEditor->ClearStepRequest(); }

// Returns current playback state or Edit when level editor is unavailable
EditorState EditorService::GetPlaybackState() const { 
    return (m_levelEditor) ? m_levelEditor->GetEditorState() : EditorState::Edit; 
}

/**
 * @brief Returns whether GPU timer query instrumentation is active.
 * @return True when scene and ImGui GPU timings are available.
 */
bool EditorService::HasGpuTiming() const {
    return m_gpuTimingSupported;
}

/**
 * @brief Get the latest scene-render GPU elapsed time.
 * @return Scene GPU time in milliseconds.
 */
float EditorService::GetSceneRenderGpuMs() const {
    return m_sceneGpuMs;
}

/**
 * @brief Get the latest ImGui-render GPU elapsed time.
 * @return ImGui GPU time in milliseconds.
 */
float EditorService::GetImGuiRenderGpuMs() const {
    return m_imguiGpuMs;
}

// Updates active world reference and propagates scene target into level editor
void EditorService::SetWorld(ECS::World* world) {
    if (m_world == world)
        return;

    m_world = world;

    if (m_levelEditor) {

        // Update scene binding too so scene-dependent actions follow active context
        Scenes::Scene* activeScene = m_sceneManager.GetActive();
        m_levelEditor->SetScene(activeScene);
    }
}

// Begins editor frame processing for the active level editor when startup flow allows it
void EditorService::BeginFrame() {
    if (!m_initialized) return;

    // Startup UI can take exclusive focus, in that state editor panels should not run begin-frame logic
    const auto& stageGetter = m_projectStartupUI.GetStageGetter();
    const EditorStartupStage stage = stageGetter ? stageGetter() : EditorStartupStage::Ready;
    const bool blockEditor = stageGetter && (m_projectStartupUI.WantsProjectBrowser() || stage == EditorStartupStage::SelectProject);

    if (blockEditor) {
        return;
    }

    // Show level editor only when enabled and when active scene matches optional scene pin
    auto* activeScene = m_sceneManager.GetActive();
    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    if (m_levelEditor && shouldShowLevelEditor) {
        m_levelEditor->BeginFrame();
    }
}

// Updates editor runtime state and creates or rebuilds level editor when required
void EditorService::Update() {
    if (!m_initialized) return;

    // Startup screen ownership means editor update work must pause until startup flow yields control
    const auto& stageGetter = m_projectStartupUI.GetStageGetter();
    const EditorStartupStage stage = stageGetter ? stageGetter() : EditorStartupStage::Ready;
    const bool blockEditor = stageGetter && (m_projectStartupUI.WantsProjectBrowser() || stage == EditorStartupStage::SelectProject);
    if (blockEditor) {
        return;
    }

    // Rebuild path recreates LevelEditor instance to refresh panel graph and scene hookups
    if (m_pendingLevelEditorRebuild && m_levelEditor) {
        LevelEditorConfig config;
        m_levelEditor.reset();

        // Keep previous scene pin when set, otherwise follow current active scene
        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : m_sceneManager.GetActive();
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);
        m_levelEditor->SetEditorSettings(m_editorSettings);

        // Reinitialize with active native window handle so editor can bind viewport/input backend state
        if (Engine::CORE) {
            auto* platformContext = Engine::CORE->GetPlatformContext();
            if (platformContext) {
                auto* mainWindow = platformContext->GetMainWindow();
                if (mainWindow) {
                    m_levelEditor->Initialize(static_cast<GLFWwindow*>(mainWindow->GetNativeHandle()));
                }
            }
        }
        m_levelEditor->SetProjectBrowserRequestCallback([this]() { RequestProjectBrowser(); });

        m_pendingLevelEditorRebuild = false;
    }

    auto* activeScene = m_sceneManager.GetActive();

    // Refresh world pointer whenever active scene changes so all editor panels target current scene data
    ECS::World* activeWorld = activeScene ? &activeScene->GetWorld() : nullptr;
    SetWorld(activeWorld);

    if (m_showLevelEditor && m_levelEditorForScene && activeScene != m_levelEditorForScene) {

        // If pinned scene disappears or changes, follow active scene rather than hiding editor UI
        m_levelEditorForScene = activeScene;
        if (!m_levelEditorForScene) {
            m_levelEditorForScene = nullptr;
        }
    }

    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    // Lazy-create level editor on first use so startup remains lightweight until editor is actually shown
    if (shouldShowLevelEditor && !m_levelEditor) {
        LevelEditorConfig config;
        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : activeScene;
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);
        m_levelEditor->SetEditorSettings(m_editorSettings);

        // Pass native window handle so renderer/input hooks are initialized against the correct platform window
        if (Engine::CORE) {
            auto* platformContext = Engine::CORE->GetPlatformContext();

            if (platformContext) {
                auto* mainWindow = platformContext->GetMainWindow();
                
                if (mainWindow) {
                    m_levelEditor->Initialize(static_cast<GLFWwindow*>(mainWindow->GetNativeHandle()));
                }
            }
        }
        m_levelEditor->SetProjectBrowserRequestCallback([this]() { RequestProjectBrowser(); });
    }
}

// Renders ImGui frame and routes startup overlays versus level editor windows
void EditorService::Render() {
    if (!m_initialized || !m_backendInitialized) {
        return;
    }

    // Consume completed query results from prior frames without stalling CPU.
    if (m_gpuTimingSupported) {
        auto tryConsumeGpuQuery = [](const unsigned int queryId, bool& pending, float& outputMs) {
            if (!pending || queryId == 0u) {
                return;
            }

            GLuint available = 0u;
            glGetQueryObjectuiv(queryId, GL_QUERY_RESULT_AVAILABLE, &available);
            if (available == 0u) {
                return;
            }

            GLuint64 elapsedNs = 0u;
            glGetQueryObjectui64v(queryId, GL_QUERY_RESULT, &elapsedNs);
            outputMs = static_cast<float>(static_cast<double>(elapsedNs) / 1000000.0);
            pending = false;
        };

        tryConsumeGpuQuery(m_sceneGpuQuery, m_sceneGpuQueryPending, m_sceneGpuMs);
        tryConsumeGpuQuery(m_imguiGpuQuery, m_imguiGpuQueryPending, m_imguiGpuMs);
    }

    auto* activeScene = m_sceneManager.GetActive();
    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    // Feed core timing and viewport size into ImGui before starting a new frame
    ImGuiIO& io = ImGui::GetIO();
    auto* platformContext = Engine::CORE ? Engine::CORE->GetPlatformContext() : nullptr;
    if (platformContext) {
        if (auto* w = platformContext->GetMainWindow()) {
            io.DisplaySize = ImVec2(static_cast<float>(w->GetWidth()), static_cast<float>(w->GetHeight()));
        }
    }
    io.DeltaTime = static_cast<float>(TimeSystem::Instance().GetUnscaledDeltaTime());

    // Forward current mouse position and button state from engine input to ImGui IO
    double mx, my;
    Input::GetMousePosition(mx, my);
    io.MousePos = ImVec2(static_cast<float>(mx), static_cast<float>(my));
    io.MouseDown[0] = Input::IsMouseDown(MOUSE_LEFT);
    io.MouseDown[1] = Input::IsMouseDown(MOUSE_RIGHT);
    io.MouseDown[2] = Input::IsMouseDown(MOUSE_MIDDLE);
    io.MouseWheel = static_cast<float>(Input::GetScrollY());

    // Update modifier keys explicitly because many widgets depend on Ctrl/Shift/Alt state
    io.KeyCtrl = Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL);
    io.KeyShift = Input::IsKeyDown(KEY_LEFT_SHIFT) || Input::IsKeyDown(KEY_RIGHT_SHIFT);
    io.KeyAlt = Input::IsKeyDown(KEY_LEFT_ALT) || Input::IsKeyDown(KEY_RIGHT_ALT);
    io.KeySuper = Input::IsKeyDown(KEY_LEFT_SUPER) || Input::IsKeyDown(KEY_RIGHT_SUPER);

    // Helper maps engine key state to individual ImGui keys
    auto feedKey = [&](ImGuiKey imguiKey, int engineKey) {
        io.AddKeyEvent(imguiKey, Input::IsKeyDown(engineKey));
    };

    // Navigation and editing cluster keys
    feedKey(ImGuiKey_Tab, KEY_TAB);
    feedKey(ImGuiKey_LeftArrow, KEY_LEFT);
    feedKey(ImGuiKey_RightArrow, KEY_RIGHT);
    feedKey(ImGuiKey_UpArrow, KEY_UP);
    feedKey(ImGuiKey_DownArrow, KEY_DOWN);
    feedKey(ImGuiKey_PageUp, KEY_PAGE_UP);
    feedKey(ImGuiKey_PageDown, KEY_PAGE_DOWN);
    feedKey(ImGuiKey_Home, KEY_HOME);
    feedKey(ImGuiKey_End, KEY_END);
    feedKey(ImGuiKey_Insert, KEY_INSERT);
    feedKey(ImGuiKey_Delete, KEY_DELETE);
    feedKey(ImGuiKey_Backspace, KEY_BACKSPACE);
    feedKey(ImGuiKey_Space, KEY_SPACE);
    feedKey(ImGuiKey_Enter, KEY_ENTER);
    feedKey(ImGuiKey_Escape, KEY_ESCAPE);
    feedKey(ImGuiKey_Apostrophe, KEY_APOSTROPHE);
    feedKey(ImGuiKey_Comma, KEY_COMMA);
    feedKey(ImGuiKey_Minus, KEY_MINUS);
    feedKey(ImGuiKey_Period, KEY_PERIOD);
    feedKey(ImGuiKey_Slash, KEY_SLASH);
    feedKey(ImGuiKey_Semicolon, KEY_SEMICOLON);
    feedKey(ImGuiKey_Equal, KEY_EQUAL);
    feedKey(ImGuiKey_LeftBracket, KEY_LEFT_BRACKET);
    feedKey(ImGuiKey_Backslash, KEY_BACKSLASH);
    feedKey(ImGuiKey_RightBracket, KEY_RIGHT_BRACKET);
    feedKey(ImGuiKey_GraveAccent, KEY_GRAVE_ACCENT);
    feedKey(ImGuiKey_CapsLock, KEY_CAPS_LOCK);
    feedKey(ImGuiKey_LeftShift, KEY_LEFT_SHIFT);
    feedKey(ImGuiKey_LeftCtrl, KEY_LEFT_CONTROL);
    feedKey(ImGuiKey_LeftAlt, KEY_LEFT_ALT);
    feedKey(ImGuiKey_LeftSuper, KEY_LEFT_SUPER);
    feedKey(ImGuiKey_RightShift, KEY_RIGHT_SHIFT);
    feedKey(ImGuiKey_RightCtrl, KEY_RIGHT_CONTROL);
    feedKey(ImGuiKey_RightAlt, KEY_RIGHT_ALT);
    feedKey(ImGuiKey_RightSuper, KEY_RIGHT_SUPER);

    // Top-row number keys
    feedKey(ImGuiKey_0, KEY_0);
    feedKey(ImGuiKey_1, KEY_1);
    feedKey(ImGuiKey_2, KEY_2);
    feedKey(ImGuiKey_3, KEY_3);
    feedKey(ImGuiKey_4, KEY_4);
    feedKey(ImGuiKey_5, KEY_5);
    feedKey(ImGuiKey_6, KEY_6);
    feedKey(ImGuiKey_7, KEY_7);
    feedKey(ImGuiKey_8, KEY_8);
    feedKey(ImGuiKey_9, KEY_9);

    // Alphabet keys
    feedKey(ImGuiKey_A, KEY_A);
    feedKey(ImGuiKey_B, KEY_B);
    feedKey(ImGuiKey_C, KEY_C);
    feedKey(ImGuiKey_D, KEY_D);
    feedKey(ImGuiKey_E, KEY_E);
    feedKey(ImGuiKey_F, KEY_F);
    feedKey(ImGuiKey_G, KEY_G);
    feedKey(ImGuiKey_H, KEY_H);
    feedKey(ImGuiKey_I, KEY_I);
    feedKey(ImGuiKey_J, KEY_J);
    feedKey(ImGuiKey_K, KEY_K);
    feedKey(ImGuiKey_L, KEY_L);
    feedKey(ImGuiKey_M, KEY_M);
    feedKey(ImGuiKey_N, KEY_N);
    feedKey(ImGuiKey_O, KEY_O);
    feedKey(ImGuiKey_P, KEY_P);
    feedKey(ImGuiKey_Q, KEY_Q);
    feedKey(ImGuiKey_R, KEY_R);
    feedKey(ImGuiKey_S, KEY_S);
    feedKey(ImGuiKey_T, KEY_T);
    feedKey(ImGuiKey_U, KEY_U);
    feedKey(ImGuiKey_V, KEY_V);
    feedKey(ImGuiKey_W, KEY_W);
    feedKey(ImGuiKey_X, KEY_X);
    feedKey(ImGuiKey_Y, KEY_Y);
    feedKey(ImGuiKey_Z, KEY_Z);

    // Function keys
    feedKey(ImGuiKey_F1, KEY_F1);
    feedKey(ImGuiKey_F2, KEY_F2);
    feedKey(ImGuiKey_F3, KEY_F3);
    feedKey(ImGuiKey_F4, KEY_F4);
    feedKey(ImGuiKey_F5, KEY_F5);
    feedKey(ImGuiKey_F6, KEY_F6);
    feedKey(ImGuiKey_F7, KEY_F7);
    feedKey(ImGuiKey_F8, KEY_F8);
    feedKey(ImGuiKey_F9, KEY_F9);
    feedKey(ImGuiKey_F10, KEY_F10);
    feedKey(ImGuiKey_F11, KEY_F11);
    feedKey(ImGuiKey_F12, KEY_F12);

    // Push UTF character stream for text input widgets then clear per-frame text buffer
    const std::string& charInput = Input::GetCharInput();
    for (unsigned char c : charInput) {
        io.AddInputCharacter(c);
    }
    Input::ClearCharInput();

    // Begin ImGui frame lifecycle
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // Startup stages decide whether editor UI is blocked, overlaid, or fully interactive
    const auto& stageGetter = m_projectStartupUI.GetStageGetter();
    const EditorStartupStage stage = stageGetter ? stageGetter() : EditorStartupStage::Ready;
    const bool showScenePicker = stageGetter && stage == EditorStartupStage::SelectScene;
    const bool showBooting = stageGetter && stage == EditorStartupStage::Booting;
    const bool blockEditor = stageGetter && (m_projectStartupUI.WantsProjectBrowser() || stage == EditorStartupStage::SelectProject);
    const bool showStartupOverlay = showBooting || showScenePicker;
    
    if (blockEditor) {
        m_projectStartupUI.Render();
    }
    else {

        // Normal editor update/render path while startup screens are not in exclusive mode
        if (m_levelEditor && shouldShowLevelEditor) {
            m_levelEditor->Update();

            // Track GPU cost of scene/editor world rendering independent of ImGui pass.
            const bool canProfileSceneGpu = m_gpuTimingSupported && !m_sceneGpuQueryPending && (m_sceneGpuQuery != 0u);
            if (canProfileSceneGpu) {
                glBeginQuery(GL_TIME_ELAPSED, m_sceneGpuQuery);
            }
            m_levelEditor->Render();
            if (canProfileSceneGpu) {
                glEndQuery(GL_TIME_ELAPSED);
                m_sceneGpuQueryPending = true;
            }
        }

        // When startup controller exists but no overlay mode is active, allow it to render contextual UI panels
        else if (stageGetter && !showStartupOverlay) {
            m_projectStartupUI.Render();
        }

        // Overlay modes must draw last to visually and interactively sit above editor content
        if (showStartupOverlay) {
            m_projectStartupUI.Render();
        }
    }

    // Global Select-All handler for focused ImGui text fields
    // Run after UI build so final focused input id for this frame is known
    const bool ctrlDown = Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL);
    const bool superDown = Input::IsKeyDown(KEY_LEFT_SUPER) || Input::IsKeyDown(KEY_RIGHT_SUPER);
    const bool selectAllPressed = Input::IsKeyPressed(KEY_A);

    if ((ctrlDown || superDown) && selectAllPressed) {
        ImGuiContext& g = *GImGui;
        if (g.InputTextState.ID != 0) {
            g.InputTextState.SelectAll();
            g.InputTextState.CursorAnimReset();
        }
    }

    ImGui::EndFrame();
    ImGui::Render();

    auto* drawData = ImGui::GetDrawData();
    if (drawData) {
        // Track GPU cost for pure ImGui rendering pass.
        const bool canProfileImGuiGpu = m_gpuTimingSupported && !m_imguiGpuQueryPending && (m_imguiGpuQuery != 0u);
        if (canProfileImGuiGpu) {
            glBeginQuery(GL_TIME_ELAPSED, m_imguiGpuQuery);
        }
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
        if (canProfileImGuiGpu) {
            glEndQuery(GL_TIME_ELAPSED);
            m_imguiGpuQueryPending = true;
        }
    }
}

// Ends frame for level editor when startup flow does not own the UI
void EditorService::EndFrame() {
    if (!m_initialized) return;

    // Startup block mode skips level-editor end-frame work
    const auto& stageGetter = m_projectStartupUI.GetStageGetter();
    const EditorStartupStage stage = stageGetter ? stageGetter() : EditorStartupStage::Ready;
    const bool blockEditor = stageGetter && (m_projectStartupUI.WantsProjectBrowser() || stage == EditorStartupStage::SelectProject);
    if (blockEditor) {
        return;
    }

    // Mirror same show condition used in BeginFrame/Render for lifecycle consistency
    auto* activeScene = m_sceneManager.GetActive();
    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    // Propagate end-frame so editor panels can finalize temporary per-frame state
    if (m_levelEditor && shouldShowLevelEditor) {
        m_levelEditor->EndFrame();
    }
}

// Shuts down level editor and ImGui backend resources in dependency-safe order
void EditorService::Terminate() {

    // Destroy LevelEditor first while ImGui context is still active
    if (m_levelEditor) {
        m_levelEditor.reset();
    }

    // Destroy ImGui context only after level editor teardown so panel destructors never touch dead ImGui state
    if (ImGui::GetCurrentContext() != nullptr) {
        if (m_backendInitialized) {

            // Ensure GL context is current before shutting down OpenGL backend
            if (Engine::CORE) {
                auto* platformContext = Engine::CORE->GetPlatformContext();
                if (platformContext) {
                    auto* mainWindow = platformContext->GetMainWindow();
                    if (mainWindow) {
                        GLFWwindow* glfwHandle = static_cast<GLFWwindow*>(mainWindow->GetNativeHandle());
                        if (glfwHandle) {
                            glfwMakeContextCurrent(glfwHandle);
                        }
                    }
                }
            }

            ImGui_ImplOpenGL3_Shutdown();

            // Only OpenGL renderer backend is initialized in this service
            // Do not call ImGui_ImplGlfw_Shutdown() because we do not use
            // ImGui's GLFW platform backend (it would attempt to shutdown
            // a backend that was never initialized and trigger assertions)
            m_backendInitialized = false;
        }
        ImGui::DestroyContext();
    }

    if (m_sceneGpuQuery != 0u) {
        glDeleteQueries(1, &m_sceneGpuQuery);
        m_sceneGpuQuery = 0u;
    }
    if (m_imguiGpuQuery != 0u) {
        glDeleteQueries(1, &m_imguiGpuQuery);
        m_imguiGpuQuery = 0u;
    }
    m_gpuTimingSupported = false;
    m_sceneGpuQueryPending = false;
    m_imguiGpuQueryPending = false;
    m_sceneGpuMs = 0.0f;
    m_imguiGpuMs = 0.0f;
    
    m_initialized = false;
}

// Enables level editor and optionally pins it to a specific scene
void EditorService::EnableLevelEditorForScene(Scenes::Scene* scene) {
    m_showLevelEditor = true;
    m_levelEditorForScene = scene;
    LOG_DEBUG(scene ? "LevelEditor enabled for scene" : "LevelEditor enabled (scene-less)");
}

// Disables level editor and clears scene pin
void EditorService::DisableLevelEditor() {
    m_showLevelEditor = false;
    m_levelEditorForScene = nullptr;
    if (m_levelEditor) m_levelEditor.reset();
    LOG_DEBUG("LevelEditor disabled");
}

#else
// Stub no-op when ImGui/editor integration is not compiled
void EditorService::SetStartupStageGetter(std::function<EditorStartupStage()>) {}
void EditorService::SetProjectStartupCallbacks(const Editor::ProjectStartupCallbacks&) {}
void EditorService::SetEditorSettings(EditorSettings*) {}
void EditorService::RequestProjectBrowser() {}
void EditorService::RequestLevelEditorRebuild() {}
void EditorService::Update() {}
void EditorService::Render() {}
void EditorService::Terminate() {}
void EditorService::EnableLevelEditorForScene(Scenes::Scene* /*scene*/) {}
void EditorService::DisableLevelEditor() {}
bool EditorService::IsGamePlaying() const { return false; }
bool EditorService::IsStepRequested() const { return false; }
void EditorService::ClearStepRequest() const {}
EditorState EditorService::GetPlaybackState() const { return EditorState::Edit; }
bool EditorService::HasGpuTiming() const { return false; }
float EditorService::GetSceneRenderGpuMs() const { return 0.0f; }
float EditorService::GetImGuiRenderGpuMs() const { return 0.0f; }
void EditorService::SetWorld(ECS::World* world) { (void)world; }
#endif
