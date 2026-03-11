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
    // Default font sizes for the editor UI
    constexpr float kDefaultTextFontSize = 20.0f;
    constexpr float kDefaultIconFontSize = 22.0f;

    // Helper function to load fonts from the ResourceManager, falling back to file loading if RM fails
    ImFont* LoadFontFromRM(ImFontAtlas& atlas, const std::string& path, float size,
        ImFontConfig* config = nullptr, const ImWchar* ranges = nullptr) {
        auto raw = RM.Get<RawData>(path); // Try to load the font as raw data from the ResourceManager

        // If RM fails to load the font, fall back to loading directly from file path
        if (!raw || !raw->IsValid) {
            LOG_WARNING("Failed to load font via RM, falling back to file: " << path);

            ImFontConfig cfg = config ? *config : ImFontConfig(); // Use provided config or default if null
            strncpy_s(cfg.Name, path.c_str(), sizeof(cfg.Name) - 1); // Set the font name in the config for debugging purposes
            return atlas.AddFontFromFileTTF(path.c_str(), size, &cfg, ranges); // Load font directly from file
        }

        // Successfully loaded font data from RM, add it to the ImGui font atlas
        ImFontConfig cfg = config ? *config : ImFontConfig();
        cfg.FontDataOwnedByAtlas = false;
        strncpy_s(cfg.Name, path.c_str(), sizeof(cfg.Name) - 1);

        // Add the font to the atlas from memory data
        return atlas.AddFontFromMemoryTTF(raw->Data.data(), (int)raw->Data.size(), size, &cfg, ranges);
    }

    // Helper function to ensure the default editor fonts are loaded at startup, merging icon fonts into the main atlas
    void EnsureStartupFontsLoaded(ImGuiIO& io) {
        if (!io.Fonts || !io.Fonts->Fonts.empty()) {
            return;
        }

        // Load the main text font for the editor UI, with fallback to default if loading fails
        ImFont* mainFont = LoadFontFromRM(*io.Fonts,
            "assets/fonts/Inter/static/Inter_24pt-Medium.ttf",
            kDefaultTextFontSize);
        if (!mainFont) {
            mainFont = io.Fonts->AddFontDefault();
        }

        // Load the bold font for the editor UI, with fallback to default if loading fails
        ImFont* boldFont = LoadFontFromRM(*io.Fonts,
            "assets/fonts/Inter/static/Inter_24pt-ExtraBold.ttf",
            kDefaultTextFontSize);
        if (!boldFont) {
            boldFont = io.Fonts->AddFontDefault();
        }

        // Set up the icon font configuration for merging into the main atlas, using the Material Symbols Rounded font
        static constexpr ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };
        ImFontConfig iconsConfig;
        iconsConfig.MergeMode = false;
        iconsConfig.PixelSnapH = true;
        iconsConfig.OversampleH = 3;
        iconsConfig.OversampleV = 3;

        // Merge the icon font into the main atlas to ensure all editor fonts are in a single atlas, which is more efficient for rendering.
        // We check if we've already merged to avoid doing it multiple times.
        static ImFontAtlas* s_mergedAtlas = nullptr;
        if (mainFont && s_mergedAtlas != io.Fonts) {
            ImFontConfig mergeConfig = iconsConfig;
            mergeConfig.MergeMode = true;

            // Load the icon font and merge it into the existing atlas
            LoadFontFromRM(*io.Fonts,
                "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
                kDefaultTextFontSize,
                &mergeConfig,
                iconRanges);
            s_mergedAtlas = io.Fonts;
        }

        // Load the icon font separately as well, in case we want to use it directly for icon-specific rendering.
        // This is optional since the icons are merged into the main atlas, but it allows for more direct access if needed.
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

EditorService::~EditorService() { m_editorInstance = nullptr; }

// Clean up ImGui resources on shutdown
void EditorService::SetStartupStageGetter(std::function<EditorStartupStage()> getter) {
    m_projectStartupUI.SetStageGetter(std::move(getter));
}

// Proxy function to set the project startup callbacks in the ProjectStartupUI, allowing the 
// editor to provide callback implementations for project selection and creation actions initiated from the UI.
void EditorService::SetProjectStartupCallbacks(const Editor::ProjectStartupCallbacks& callbacks) {
    m_projectStartupUI.SetCallbacks(callbacks);
}

// Proxy function to set the editor settings in the ProjectStartupUI, allowing the 
// UI to access configuration settings such as recent projects and UI scale for rendering the startup screens.
void EditorService::SetEditorSettings(EditorSettings* settings) {
    m_editorSettings = settings;
    m_projectStartupUI.SetEditorSettings(settings);

	// Also propagate the editor settings to the level editor if it exists, so that any changes 
    // to settings (e.g. UI scale) can be reflected in the editor views immediately
    if (m_levelEditor) {
        m_levelEditor->SetEditorSettings(settings);
    }
}

// Proxy function to request the project browser UI to be shown, which can be called 
// from other parts of the editor to trigger the project selection screen.
void EditorService::RequestProjectBrowser() {
    m_projectStartupUI.RequestProjectBrowser();
}

// Proxy function to request a rebuild of the level editor, which can be called from 
// other parts of the editor to trigger a refresh of the editor views
// (e.g., after a scene change or significant update that requires the editor to re-query the scene for updated data).
void EditorService::RequestLevelEditorRebuild() {
    m_pendingLevelEditorRebuild = true;
}

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
        // manages GLFW callbacks and that caused assertion failures).
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        io.FontGlobalScale = EditorStyle::FontScale;
        EnsureStartupFontsLoaded(io); // Ensure the default editor fonts are loaded at startup, merging icon fonts into the main atlas for efficient rendering.

        ImGuiStyle& style = ImGui::GetStyle();
        // Apply the editor's modern theme overrides.
        EditorStyle::ApplyModernTheme(style);

        GLFWwindow* glfwHandle = static_cast<GLFWwindow*>(mainWindow->GetNativeHandle());
        if (!glfwHandle) {
            LOG_ERROR("[EditorService] GLFW native handle is null; cannot initialize ImGui");
            ImGui::DestroyContext();
            return;
        }

        // Make sure the window's context is current before initializing GL loader/backends
        glfwMakeContextCurrent(glfwHandle);

        // Ensure GL functions are available (GLAD). Window creation normally initializes GLAD,
        // but re-check here to be safe in case of ordering differences during refactor.
        if (!gladLoadGL()) {
            LOG_ERROR("[EditorService] gladLoadGL() failed");
            ImGui_ImplOpenGL3_Shutdown();
            ImGui::DestroyContext();
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

            LOG_INFO("Running ImGui version " << IMGUI_VERSION);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in EditorService::Initialize(): " << e.what());
        // Clean up partially initialized state
        if (m_backendInitialized) {
            ImGui_ImplOpenGL3_Shutdown();
            m_backendInitialized = false;
        }
        if (ImGui::GetCurrentContext() != nullptr) {
            ImGui::DestroyContext();
        }
    } catch (...) {
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

void EditorService::BeginFrame() {
    if (!m_initialized) return;

    // Check the project startup stage to determine if we should block editor updates/renders.
    // If the project browser is active or we're in the project selection stage, we skip updating 
    // and rendering the editor to avoid conflicts and ensure the startup UI is responsive.
    const auto& stageGetter = m_projectStartupUI.GetStageGetter();
    const EditorStartupStage stage = stageGetter ? stageGetter() : EditorStartupStage::Ready;
    const bool blockEditor = stageGetter && (m_projectStartupUI.WantsProjectBrowser() || stage == EditorStartupStage::SelectProject);

    // If the editor should be blocked due to the startup stage, we skip the rest of the update/render logic.
    if (blockEditor) {
        return;
    }

    // Determine if we should show the level editor based on the current active scene and the target scene for the level editor.
    auto* activeScene = m_sceneManager.GetActive();
    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    if (m_levelEditor && shouldShowLevelEditor) {
        m_levelEditor->BeginFrame();
    }
}

void EditorService::Update() {
    if (!m_initialized) return;

    // Check the project startup stage to determine if we should block editor updates/renders.
    // If the project browser is active or we're in the project selection stage, we skip updating
    // and rendering the editor to avoid conflicts and ensure the startup UI is responsive.
    const auto& stageGetter = m_projectStartupUI.GetStageGetter();
    const EditorStartupStage stage = stageGetter ? stageGetter() : EditorStartupStage::Ready;
    const bool blockEditor = stageGetter && (m_projectStartupUI.WantsProjectBrowser() || stage == EditorStartupStage::SelectProject);
    if (blockEditor) {
        return;
    }

    // Handle pending level editor rebuild requests, which can be triggered by other parts of the 
    // editor when a refresh of the editor views is needed (e.g., after a scene change).
    if (m_pendingLevelEditorRebuild && m_levelEditor) {
        LevelEditorConfig config;
        m_levelEditor.reset();

        // When rebuilding the level editor, we need to determine the appropriate target scene to use for the new instance.
        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : m_sceneManager.GetActive();
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);
        m_levelEditor->SetEditorSettings(m_editorSettings);

        // Reinitialize the level editor with the main window's native handle to ensure it has the correct context for rendering and input handling.
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

    // Propagate active scene's world to editor panels so UI updates immediately
    // when a new scene is created or activated.
    ECS::World* activeWorld = activeScene ? &activeScene->GetWorld() : nullptr;
    SetWorld(activeWorld);

    if (m_showLevelEditor && m_levelEditorForScene && activeScene != m_levelEditorForScene) {
        // Follow the newly activated scene instead of hiding the editor UI.
        m_levelEditorForScene = activeScene;
        if (!m_levelEditorForScene) {
            m_levelEditorForScene = nullptr;
        }
    }

    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    // If we should show the level editor but it doesn't exist yet, create it targeting the current active scene (or null if no active scene).
    // This allows the editor to be available immediately when toggled on, even if there is no active scene at the time of toggling.
    // The editor will then update to show the correct scene once one becomes active.
    if (shouldShowLevelEditor && !m_levelEditor) {
        LevelEditorConfig config;
        Scenes::Scene* targetScene = m_levelEditorForScene ? m_levelEditorForScene : activeScene;
        m_levelEditor = std::make_unique<LevelEditor>(m_world, config, targetScene);
        m_levelEditor->SetEditorSettings(m_editorSettings);

        // Initialize the level editor with the main window's native handle to ensure it has the correct context for rendering and input handling.
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

void EditorService::Render() {
    if (!m_initialized || !m_backendInitialized) {
        return;
    }

    auto* activeScene = m_sceneManager.GetActive();
    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

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
    io.MouseWheel = static_cast<float>(Input::GetScrollY());

    // Feed keyboard state to ImGui
    io.KeyCtrl = Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL);
    io.KeyShift = Input::IsKeyDown(KEY_LEFT_SHIFT) || Input::IsKeyDown(KEY_RIGHT_SHIFT);
    io.KeyAlt = Input::IsKeyDown(KEY_LEFT_ALT) || Input::IsKeyDown(KEY_RIGHT_ALT);
    io.KeySuper = Input::IsKeyDown(KEY_LEFT_SUPER) || Input::IsKeyDown(KEY_RIGHT_SUPER);

    // Feed key presses to ImGui for input handling.
    auto feedKey = [&](ImGuiKey imguiKey, int engineKey) {
        io.AddKeyEvent(imguiKey, Input::IsKeyDown(engineKey));
    };

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

    // Feed character input to ImGui
    const std::string& charInput = Input::GetCharInput();
    for (unsigned char c : charInput) {
        io.AddInputCharacter(c);
    }
    Input::ClearCharInput();

    // Start the ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // Check the project startup stage to determine if we should block editor updates/renders.
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
        // If the editor is not blocked, we proceed with normal update and render logic.
        if (m_levelEditor && shouldShowLevelEditor) {
            m_levelEditor->Update();
            m_levelEditor->Render();
        }
        // If we're in a startup stage that requires an overlay (booting or scene picker), we render the startup UI on top of the editor. 
        // Otherwise, if there's a stage getter but no overlay is needed, we render the startup UI normally (it will decide what to show based on the stage).
        else if (stageGetter && !showStartupOverlay) {
            m_projectStartupUI.Render();
        }

        // If the startup stage indicates we should show an overlay (e.g., booting screen or scene picker), 
        // we render the startup UI on top of everything else to ensure it is visible and interactive, blocking access 
        // to the editor until the user has made a selection or the booting process is complete.
        if (showStartupOverlay) {
            m_projectStartupUI.Render();
        }
    }

    // Global Ctrl+A for active text inputs
    // Run this after all UI is built so the current frame's focused input field
    // is known (e.g. clicking into "New Entity" then pressing Ctrl+A)
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
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
}

void EditorService::EndFrame() {
    if (!m_initialized) return;

    // Check the project startup stage to determine if we should block editor updates/renders.
    const auto& stageGetter = m_projectStartupUI.GetStageGetter();
    const EditorStartupStage stage = stageGetter ? stageGetter() : EditorStartupStage::Ready;
    const bool blockEditor = stageGetter && (m_projectStartupUI.WantsProjectBrowser() || stage == EditorStartupStage::SelectProject);
    if (blockEditor) {
        return;
    }

    // Determine if we should show the level editor based on the current active scene and the target scene for the level editor.
    auto* activeScene = m_sceneManager.GetActive();
    bool shouldShowLevelEditor = m_showLevelEditor && (m_levelEditorForScene == nullptr || (activeScene && activeScene == m_levelEditorForScene));

    // If the level editor is active and should be shown, we call EndFrame to allow it to perform any necessary cleanup or state updates after rendering.
    if (m_levelEditor && shouldShowLevelEditor) {
        m_levelEditor->EndFrame();
    }
}

void EditorService::Terminate() {
    // Destroy LevelEditor first while ImGui context is still active
    if (m_levelEditor) {
        m_levelEditor.reset();
    }

    // Clean up ImGui resources carefully
    // Must happen AFTER LevelEditor is destroyed to avoid accessing ImGui from destructors
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
            // We only initialize the OpenGL renderer backend (see Initialize()).
            // Do not call ImGui_ImplGlfw_Shutdown() because we do not use
            // ImGui's GLFW platform backend (it would attempt to shutdown
            // a backend that was never initialized and trigger assertions).
            m_backendInitialized = false;
        }
        ImGui::DestroyContext();
    }
    
    m_initialized = false;
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
void EditorService::SetWorld(ECS::World* world) { (void)world; }
#endif
