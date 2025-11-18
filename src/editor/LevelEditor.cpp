/* Start Header *****************************************************************/
/*!
\file   LevelEditor.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the LevelEditor class - main orchestrator for the game editor interface.
Manages docking layout, panel coordination, entity selection, and playback controls.
Integrates Hierarchy, Inspector, Asset Browser, and Viewport panels.
*/
/* End Header *******************************************************************/

#include "../../include/editor/LevelEditor.h"
#include "core/Logger.h"
#include <imgui.h>
#include "graphics/graphicsConfig.hpp"
#include "ecs/systems/RendererSystem.h"
#include <imgui_internal.h>
#include <core/Application.h>
#include "services/Time.h"
#include "../editor/AudioAssetLibrary.h"

// Create the editor and initialize panel members and config
LevelEditor::LevelEditor(ECS::World* world, const LevelEditorConfig& config, Scenes::Scene* scene)
    : m_world(world), m_config(config), m_playback(world), m_symbolsFont(nullptr),
    m_mainFont(nullptr), m_boldFont(nullptr), m_assetBrowser(), m_viewport(),
    m_hierarchyWindow(), m_inspector(), m_entityActions(scene) {
    // Defer panel initialization to Initialize to use loaded fonts
}

// Destroy the editor instance without owning the world
LevelEditor::~LevelEditor() {}

// -------------------------------------------------------------------------
// Panel Registration System
// -------------------------------------------------------------------------
// Register a panel with initialization and render callbacks.
// Centralizes panel management and eliminates repetitive init/render boilerplate.
void LevelEditor::_registerPanel(const char* panelName,
    std::function<void()> initFn,
    std::function<void()> renderFn,
    std::function<void(ECS::World*)> setWorldFn) {
    PanelRegistration reg;
    reg.Name = panelName;
    reg.InitializeCallback = initFn;
    reg.RenderCallback = renderFn;
    reg.SetWorldCallback = setWorldFn;
    m_panelRegistry.push_back(reg);
}

// Initialize all registered panels.
// Called after fonts are loaded to set up each panel's state.
void LevelEditor::_initializePanels() {
    for (auto& panel : m_panelRegistry) {
        if (panel.InitializeCallback) {
            panel.InitializeCallback();
        }
    }
}

// Render all registered panels.
// Iterates through the registry and calls each panel's render function.
void LevelEditor::_renderPanels() {
    for (auto& panel : m_panelRegistry) {
        if (panel.RenderCallback) {
            panel.RenderCallback();
        }
    }
}

// Propagate world reference to all registered panels.
// Updates each panel when the active scene changes.
void LevelEditor::_updatePanelWorlds(ECS::World* world) {
    for (auto& panel : m_panelRegistry) {
        if (panel.SetWorldCallback) {
            panel.SetWorldCallback(world);
        }
    }
}

// -------------------------------------------------------------------------
// Dock Layout
// -------------------------------------------------------------------------
// Build the dock layout once using split ratios and target windows
void LevelEditor::_buildDockLayout() {
    // Rebuild only when flagged to avoid resetting panel positions
    if (m_dockLayoutBuilt) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp->Size.x <= 0 || vp->Size.y <= 0) return;         // Guard against zero size

    ImGui::DockBuilderRemoveNode(m_dockspaceId);
    // Flags: DockSpace creates a root docking container; PassthruCentralNode lets the central area render game content underneath
    ImGui::DockBuilderAddNode(m_dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderSetNodeSize(m_dockspaceId, vp->Size); // Match viewport size

    // Target layout: left hierarchy, center viewport with controls on top,
    // bottom asset browser, right strip for inspector/prefab editors

    ImGuiID leftCenterNode, rightNode;
    // Split root: carve right strip (25% width) for inspectors
    // Params: (source_node, direction, size_ratio, out_id_primary, out_id_remaining)
    ImGui::DockBuilderSplitNode(m_dockspaceId, ImGuiDir_Right, 0.25f, &rightNode, &leftCenterNode);  // Reserve right strip

    ImGuiID topSection, assetBrowserNode;
    // Split main area vertically: top work area (65%), bottom asset browser (35%)
    ImGui::DockBuilderSplitNode(leftCenterNode, ImGuiDir_Up, 0.65f, &topSection, &assetBrowserNode); // Split for assets

    ImGuiID leftTopNode, centerTopSection;
    // Split top work area horizontally: left hierarchy (33%), center viewport/controls (67%)
    ImGui::DockBuilderSplitNode(topSection, ImGuiDir_Left, 0.333f, &leftTopNode, &centerTopSection); // Hierarchy strip

    ImGuiID centerControlsNode, centerViewportNode;
    // Split center area vertically: controls bar (~15.4%), viewport below
    ImGui::DockBuilderSplitNode(centerTopSection, ImGuiDir_Up, 0.154f, &centerControlsNode, &centerViewportNode); // Controls above viewport

    // Map panels to target nodes to realize the layout
    ImGui::DockBuilderDockWindow("Hierarchy", leftTopNode);
    ImGui::DockBuilderDockWindow("Game Controls", centerControlsNode);
    ImGui::DockBuilderDockWindow("Viewport", centerViewportNode);
    ImGui::DockBuilderDockWindow("Asset Browser", assetBrowserNode);
    ImGui::DockBuilderDockWindow("Prefab Editor", rightNode);
    ImGui::DockBuilderDockWindow("Property Editor", rightNode);

    ImGui::DockBuilderFinish(m_dockspaceId); // Finalize docking layout
    m_dockLayoutBuilt = true;                // Mark layout as built
}

// Invalidate the layout so the next frame rebuilds it on new size
void LevelEditor::OnWindowResized(int width, int height) {
    m_dockLayoutBuilt = false;
}

// Render the dock host window and central dock space
void LevelEditor::_renderDockSpace() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp->Size.x <= 0 || vp->Size.y <= 0) return; // Guard against hidden or zero viewport

    // Track viewport size changes and trigger layout rebuild when it changes
    if (m_lastViewportSize.x != vp->Size.x || m_lastViewportSize.y != vp->Size.y) {
        m_lastViewportSize = vp->Size;
        m_dockLayoutBuilt = false; // Force a rebuild on size change
    }

    const float topOffset = ImGui::GetFrameHeight();
    ImVec2 safePos(vp->Pos.x, vp->Pos.y + topOffset);

    // Use raw viewport size and keep proportions stable via docking splits
    ImVec2 safeSize(vp->Size.x, std::max(1.0f, vp->Size.y - topOffset));

    ImGui::SetNextWindowPos(safePos);     // Position dock host under main menu bar
    ImGui::SetNextWindowSize(safeSize);   // Size dock host to fill viewport
    ImGui::SetNextWindowViewport(vp->ID); // Pin host to current viewport

    // Host window flags: prevent interactions and visuals on the host container
    // NoDocking: disallow docking into this window
    // NoTitleBar/NoResize/NoMove: make it a static, decoration-less host
    // NoBringToFrontOnFocus/NoNavFocus: keep focus behavior stable
    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);                  // Square corners for host
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);                // No border around host
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));     // No padding; dockspace fills fully
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent host background

    // Begin invisible host window
    ImGui::Begin("MainDockSpaceHost", nullptr, hostFlags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    m_dockspaceId = ImGui::GetID("MainDockSpace");                         // Stable ID for this dockspace
    ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode; // Let central node pass content
    ImGui::DockSpace(m_dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);        // Create dockspace filling the host window

    _buildDockLayout(); // Build once on first frame or after resize
    ImGui::End();       // End host window
}

// -------------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------------
// Initialize fonts build atlas and set up editor panels and hooks
void LevelEditor::Initialize(GLFWwindow* pWin) {
    if (!pWin) return;

    auto& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    // Initialize audio asset library:
    // this will scan for folder assets/Audio
    AudioAssetLibrary::Get().Refresh("assets/Audio");

    style.ChildBorderSize = 0.75f; // Subtle child border for visual separation
    _loadFonts();

    Scenes::SceneManager* sm = Engine::CORE ? &Engine::CORE->GetSceneManager() : nullptr;
    m_fileMenu.Initialize(sm);

    // Register all panels with their initialization and render callbacks.
    // Centralizes panel lifecycle management and reduces code duplication.
    _registerPanel("Playback Controls",
        [this]() {
            m_playback.Initialize(m_mainFont, m_symbolsFont);
            // Register playback state change callback
            m_playback.OnStateChanged([this](Playback::GameState oldState, Playback::GameState newState) {
                _onPlaybackStateChanged(oldState, newState);
                });
        },
        [this]() { m_playback.Render(); },
        [this](ECS::World* w) { m_playback.SetWorld(w); }
    );

    _registerPanel("Asset Browser",
        [this]() {
            m_assetBrowser.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);
            m_assetBrowser.SetInspector(&m_inspector);
        },
        [this]() { m_assetBrowser.Render(); },
        [this](ECS::World* w) { m_assetBrowser.SetWorld(w); }
    );

    _registerPanel("Editor Core",
        [this]() {
            Scenes::SceneManager* sm = Engine::CORE ? &Engine::CORE->GetSceneManager() : nullptr;
            m_viewport.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world, sm);
            // Register viewport selection callback
            m_viewport.OnSelectionChanged([this](EntityId id) {
                _onViewportSelectionChanged(id);
                });
        },
        [this]() { m_viewport.ShowEditorWindows(); },
        [this](ECS::World* w) { m_viewport.SetWorld(w); }
    );

    _registerPanel("Hierarchy",
        [this]() { m_hierarchyWindow.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world, &m_entityActions); },
        [this]() { m_hierarchyWindow.Render(); },
        [this](ECS::World* w) { m_hierarchyWindow.SetWorld(w); }
    );

    _registerPanel("Inspector",
        [this]() { m_inspector.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world); },
        [this]() { m_inspector.Render(); },
        [this](ECS::World* w) { m_inspector.SetWorld(w); }
    );

    // Initialize all registered panels
    _initializePanels();

    // Set up hierarchy selection callback to sync with inspector
    m_hierarchyWindow.OnSelectionChanged([this](EntityId id) {
        if (!m_world) { m_inspector.ClearSelection(); return; } // Clear when no world
        if (id == ECS::Entity::NPOS32) { m_inspector.ClearSelection(); return; } // Clear when no entity
        // Resolve the entity to the current generation before checking aliveness
        ECS::Entity e = m_world->Resolve(id);
        if (m_world->IsAlive(e)) m_inspector.InspectEntity(id); // Inspect when valid
        else m_inspector.ClearSelection(); // Clear when entity is dead
        });
}

void LevelEditor::_loadFonts() {
    auto& io = ImGui::GetIO();
    float textFontSize = m_config.TextFontSize;

    if (!m_mainFont && io.Fonts->Fonts.empty()) {
        m_mainFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/Inter/static/Inter_24pt-Medium.ttf",
            textFontSize
        );
        if (!m_mainFont) {
            LOG_ERROR("Failed to load Inter Medium font");
            m_mainFont = io.Fonts->AddFontDefault();
        }
    }
    else if (!m_mainFont) {
        m_mainFont = io.Fonts->Fonts[0];
    }

    if (!m_boldFont && io.Fonts->Fonts.size() < 2) {
        m_boldFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/Inter/static/Inter_24pt-ExtraBold.ttf",
            textFontSize
        );
        if (!m_boldFont) {
            LOG_ERROR("Failed to load Inter ExtraBold font");
            m_boldFont = io.Fonts->AddFontDefault();
        }
    }
    else if (!m_boldFont) {
        m_boldFont = io.Fonts->Fonts[1];
    }

    float iconFontSize = m_config.IconFontSize;
    static const ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };
    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = false;
    iconsConfig.PixelSnapH = true;
    iconsConfig.OversampleH = 3;
    iconsConfig.OversampleV = 3;

    if (!m_symbolsFont && io.Fonts->Fonts.size() < 3) {
        m_symbolsFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
            iconFontSize,
            &iconsConfig,
            iconRanges
        );
        if (!m_symbolsFont) {
            LOG_ERROR("Failed to load Material Symbols font");
            m_symbolsFont = io.Fonts->AddFontDefault();
        }
    }
    else if (!m_symbolsFont) {
        m_symbolsFont = io.Fonts->Fonts[2];
    }

    if (io.Fonts->Fonts.size() > 0 && !io.Fonts->IsBuilt()) {
        io.Fonts->Build();
    }
}

// -------------------------------------------------------------------------
// Update Loop
// -------------------------------------------------------------------------
// Process input and in world interactions for editor panels
void LevelEditor::Update() {
    // Apply global shortcuts
    m_fileMenu.HandleShortcuts(m_uiScale);

    // Auto-sync to active scene world if it changed (e.g., via File > New Scene)
    if (Engine::CORE) {
        auto& sm = Engine::CORE->GetSceneManager();
        auto* active = sm.GetActive();
        ECS::World* activeWorld = active ? &active->GetWorld() : nullptr;
        if (activeWorld != m_world) {
            m_entityActions.SetScene(active);
            SetWorld(activeWorld);
        }
    }

    m_playback.ProcessInput(); // Handle playback hotkeys and actions
    m_viewport.HandleInWorldInteraction(); // Handle viewport interactions and camera input control
}

// -------------------------------------------------------------------------
// Event Handlers
// -------------------------------------------------------------------------
// Handle playback state changes (called by Playback via callback)
void LevelEditor::_onPlaybackStateChanged(Playback::GameState oldState, Playback::GameState newState) {
    // Any editor-specific logic that needs to happen on state change goes here
    // (The time scale is already handled by Playback itself)

    // Example: You could emit events to other systems, update UI, etc.
    LOG_INFO("Playback state changed from " << static_cast<int>(oldState) << " to " << static_cast<int>(newState));
}

// Handle viewport selection changes (called by Viewport via callback)
void LevelEditor::_onViewportSelectionChanged(EntityId id) {
    if (!m_world) {
        m_inspector.ClearSelection();
        return;
    }

    if (id == 0 || id == ECS::Entity::NPOS32) {
        m_inspector.ClearSelection();
        return;
    }

    // Validate entity before inspecting
    ECS::Entity e = m_world->Resolve(id);
    if (m_world->IsAlive(e)) {
        m_inspector.InspectEntity(id);
        // Optionally: m_hierarchyWindow.HighlightEntity(id);
    }
    else {
        m_inspector.ClearSelection();
    }
}

// -------------------------------------------------------------------------
// Render
// -------------------------------------------------------------------------
// Render dock space and editor panels with a fallback when world is missing
void LevelEditor::Render() {
    if (ImGui::BeginMainMenuBar()) {
        m_fileMenu.RenderFileMenu(m_uiScale);
        ImGui::EndMainMenuBar();
    }
    _renderDockSpace();

    if (m_world) {
        // Render all registered panels
        _renderPanels();
    }
    else {
        // Render all panels but show placeholder in inspector
        _renderPanels();

        // Override inspector with placeholder message when no world attached
        ImGui::PushFont(m_mainFont);
        ImGui::Begin("Property Editor");
        ImGui::TextDisabled("No scene attached"); // Inform user about missing scene
        ImGui::PopFont();
        ImGui::End();
    }
}

// -------------------------------------------------------------------------
// World Management
// -------------------------------------------------------------------------
// Update the world reference and propagate it to all editor panels
void LevelEditor::SetWorld(ECS::World* world) {
    m_world = world; // Store new world

    // Propagate world to all registered panels using centralized system
    _updatePanelWorlds(world);
}

// -------------------------------------------------------------------------
// Accessors
// -------------------------------------------------------------------------
bool LevelEditor::IsPlaying() const {
    return m_playback.IsPlaying();
}

bool LevelEditor::IsStepRequested() const {
    return m_playback.IsStepRequested();
}

void LevelEditor::ClearStepRequest() {
    m_playback.ClearStepRequest();
}
