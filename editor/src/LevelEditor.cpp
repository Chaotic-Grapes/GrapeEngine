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

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "AudioAssetLibrary.h"
#include "core/ProjectPaths.h"
#include "core/Logger.h"
#include "ecs/systems/RendererSystem.h"
#include "EditorStyle.h"
#include "graphics/graphicsConfig.hpp"
#include "LevelEditor.h"
#include "services/TimeSystem.h"
#include "UndoSystem.h"
#include "ViewportPicking.h"
#include "ecs/ui/GUIContext.h"
#include <core/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include "CompilePanel.h"
#include "TilePalettePanel.h"
#include <cmath>

#ifdef ERROR
#undef ERROR
#endif

#ifdef max
#undef max
#endif

// Create the editor and initialize panel members and config
LevelEditor::LevelEditor(ECS::World* world, const LevelEditorConfig& config, Scenes::Scene* scene)
    : m_world(world), m_config(config), m_playback(world), m_inspector(), m_entityActions(scene) {
    // Defer panel initialization to Initialize to use loaded fonts
}

// Destroy the editor instance without owning the world
LevelEditor::~LevelEditor() {
    // Unsubscribe from engine messages
    Messaging::MessageSystem::Unsubscribe<Messaging::EntityCreated>(m_entityCreatedSubscription);
    Messaging::MessageSystem::Unsubscribe<Messaging::EntityDestroyed>(m_entityDestroyedSubscription);
    Messaging::MessageSystem::Unsubscribe<Messaging::SceneModified>(m_sceneModifiedSubscription);
    
    // Clear file menu state getter to avoid calling back into this object during member teardown
    m_fileMenu.SetEditorStateGetter(nullptr);

    // Shutdown console panel to disconnect Logger callback
    m_console.Shutdown();
    
    // Shutdown performance panel
    m_performancePanel.Shutdown();
}

// -------------------------------------------------------------------------
// Panel Registration System
// -------------------------------------------------------------------------
// Register a panel with initialization and render callbacks.
// Centralizes panel management and eliminates repetitive init/render boilerplate.
void LevelEditor::_registerPanel(const char* panelName,
    const std::function<void()>& initFn,
    const std::function<void()>& renderFn,
    const std::function<void(ECS::World*)>& setWorldFn) {
    PanelRegistration reg;
    reg.Name = panelName;
    reg.InitializeCallback = std::move(initFn);
    reg.RenderCallback = std::move(renderFn);
    reg.SetWorldCallback = std::move(setWorldFn);
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

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp->Size.x <= 0 || vp->Size.y <= 0) return;         // Guard against zero size

    ImGui::DockBuilderRemoveNode(m_dockspaceId);
    // Flags: DockSpace creates a root docking container; PassthruCentralNode lets the central area render game content underneath
    // Combine dock node flags using integer casts to avoid deprecated enum-| between different enum types (C5054)
    const ImGuiDockNodeFlags dockNodeFlags = static_cast<ImGuiDockNodeFlags>(
        static_cast<int>(ImGuiDockNodeFlags_DockSpace) | static_cast<int>(ImGuiDockNodeFlags_PassthruCentralNode)
    );
    ImGui::DockBuilderAddNode(m_dockspaceId, dockNodeFlags);
    ImGui::DockBuilderSetNodeSize(m_dockspaceId, vp->Size); // Match viewport size

    // Target layout: left hierarchy, center viewport with controls on top,
    // bottom asset browser, right strip for inspector/prefab editors

    // First split: toolbar at the very top (calculate ratio from config)
    const float toolbarRatio = m_config.ToolbarHeight / vp->Size.y;
    ImGuiID toolbarNode, mainAreaNode;
    ImGui::DockBuilderSplitNode(m_dockspaceId, ImGuiDir_Up, toolbarRatio, &toolbarNode, &mainAreaNode);

    // Hide tab bar and disable resizing on toolbar node
    ImGuiDockNode* toolbarDockNode = ImGui::DockBuilderGetNode(toolbarNode);
    toolbarDockNode->LocalFlags |= static_cast<ImGuiDockNodeFlags>(
        static_cast<int>(ImGuiDockNodeFlags_NoTabBar) | static_cast<int>(ImGuiDockNodeFlags_NoResize)
    );

    ImGuiID leftCenterNode, rightNode;
    // Split main area: carve right strip (28% width) for inspectors and tools
    // Params: (source_node, direction, size_ratio, out_id_primary, out_id_remaining)
    ImGui::DockBuilderSplitNode(mainAreaNode, ImGuiDir_Right, 0.28f, &rightNode, &leftCenterNode);

    ImGuiID topSection, assetBrowserNode;
    // Split left-center area vertically: top work area (70%), bottom asset browser (30%)
    ImGui::DockBuilderSplitNode(leftCenterNode, ImGuiDir_Up, 0.70f, &topSection, &assetBrowserNode);

    ImGuiID leftTopNode, sceneGameNode;
    // Split top work area horizontally: left strip (28%), center scene/game (72%)
    ImGui::DockBuilderSplitNode(topSection, ImGuiDir_Left, 0.28f, &leftTopNode, &sceneGameNode);

    // Map panels to target nodes to realize the layout
    ImGui::DockBuilderDockWindow("Game Controls", toolbarNode);  // Toolbar at top
    ImGui::DockBuilderDockWindow("Hierarchy", leftTopNode);
    // Layout preset 1: single shared Scene/Game dock.
    if (m_viewportLayoutPreset == 1) {
        ImGui::DockBuilderDockWindow("Scene", sceneGameNode);
        ImGui::DockBuilderDockWindow("Game", sceneGameNode);
    // Layout preset 2: split Scene/Game side-by-side.
    } else if (m_viewportLayoutPreset == 2) {
        ImGuiID sceneNode, gameNode;
        ImGui::DockBuilderSplitNode(sceneGameNode, ImGuiDir_Right, 0.5f, &gameNode, &sceneNode);
        ImGui::DockBuilderDockWindow("Scene", sceneNode);
        ImGui::DockBuilderDockWindow("Game", gameNode);
    // Layout preset 4: quad split for multiple viewports.
    } else {
        ImGuiID topNode, bottomNode;
        ImGui::DockBuilderSplitNode(sceneGameNode, ImGuiDir_Down, 0.5f, &bottomNode, &topNode);
        ImGuiID topLeft, topRight;
        ImGui::DockBuilderSplitNode(topNode, ImGuiDir_Right, 0.5f, &topRight, &topLeft);
        ImGuiID bottomLeft, bottomRight;
        ImGui::DockBuilderSplitNode(bottomNode, ImGuiDir_Right, 0.5f, &bottomRight, &bottomLeft);
        ImGui::DockBuilderDockWindow("Scene", topLeft);
        ImGui::DockBuilderDockWindow("Game", topRight);
    }
    ImGui::DockBuilderDockWindow("Prefab Editor", rightNode);
    ImGui::DockBuilderDockWindow("Property Editor", rightNode);
    ImGui::DockBuilderDockWindow("Layers", rightNode);
    ImGui::DockBuilderDockWindow("Tile Palette", rightNode);
    ImGui::DockBuilderDockWindow("Asset Browser", assetBrowserNode);
    ImGui::DockBuilderDockWindow("Console", assetBrowserNode);
    ImGui::DockBuilderDockWindow("Performance", assetBrowserNode);
    ImGui::DockBuilderDockWindow("Systems", assetBrowserNode);

    ImGui::DockBuilderFinish(m_dockspaceId); // Finalize docking layout
    
    // Programmatically select Asset Browser tab by default
    ImGui::SetWindowFocus("Asset Browser");
    
    m_dockLayoutBuilt = true;                // Mark layout as built
}

// Invalidate the layout so the next frame rebuilds it on new size
void LevelEditor::OnWindowResized(const int width, const int height) {
    (void)width;
    (void)height;
    m_dockLayoutBuilt = false;
}

// Render the dock host window and central dock space
void LevelEditor::_renderDockSpace() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp->Size.x <= 0 || vp->Size.y <= 0) return; // Guard against hidden or zero viewport

    // Track viewport size changes and trigger layout rebuild when it changes
    // Use a tolerance to avoid rebuilding every frame due to tiny float jitter.
    constexpr float sizeEpsilon = 0.5f;
    if (std::fabs(m_lastViewportSize.x - vp->Size.x) > sizeEpsilon || std::fabs(m_lastViewportSize.y - vp->Size.y) > sizeEpsilon) {
        m_lastViewportSize = vp->Size;
        m_dockLayoutBuilt = false; // Force a rebuild on size change
    }

    const float topOffset = ImGui::GetFrameHeight();
    const ImVec2 safePos(vp->Pos.x, vp->Pos.y + topOffset);

    // Use raw viewport size and keep proportions stable via docking splits
    const ImVec2 safeSize(vp->Size.x, std::max(1.0f, vp->Size.y - topOffset));

    ImGui::SetNextWindowPos(safePos);     // Position dock host under main menu bar
    ImGui::SetNextWindowSize(safeSize);   // Size dock host to fill viewport
    ImGui::SetNextWindowViewport(vp->ID); // Pin host to current viewport

    // Host window flags: prevent interactions and visuals on the host container
    // NoDocking: disallow docking into this window
    // NoTitleBar/NoResize/NoMove: make it a static, decoration-less host
    // NoBringToFrontOnFocus/NoNavFocus: keep focus behavior stable
    // Compose window flags via integer casts to avoid deprecated enum-| warnings (C5054)
    constexpr ImGuiWindowFlags hostFlags = static_cast<ImGuiWindowFlags>(
        static_cast<int>(ImGuiWindowFlags_NoDocking) | static_cast<int>(ImGuiWindowFlags_NoTitleBar) |
        static_cast<int>(ImGuiWindowFlags_NoResize) | static_cast<int>(ImGuiWindowFlags_NoMove) |
        static_cast<int>(ImGuiWindowFlags_NoBringToFrontOnFocus) | static_cast<int>(ImGuiWindowFlags_NoNavFocus)
    );

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);                  // Square corners for host
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);                // No border around host
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));     // No padding; dockspace fills fully
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorStyle::Transparent);       // Transparent host background

    // Begin invisible host window
    ImGui::Begin("MainDockSpaceHost", nullptr, hostFlags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    m_dockspaceId = ImGui::GetID("MainDockSpace");                         // Stable ID for this dockspace
    constexpr ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode; // Let central node pass content
    ImGui::DockSpace(m_dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);        // Create dockspace filling the host window

    _buildDockLayout(); // Build once on first frame or after resize
    ImGui::End();       // End host window
}

// -------------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------------
// Initialize fonts build atlas and set up editor panels and hooks
void LevelEditor::Initialize(const GLFWwindow* pWin) {
    if (!pWin) return;

    ImGuiStyle& style = ImGui::GetStyle();

    // Initialize audio asset library:
    // this will scan for audio anywhere under the project root
    AudioAssetLibrary::Get().Refresh(Engine::ProjectPaths::GetProjectRoot());

    style.ChildBorderSize = 0.75f; // Subtle child border for visual separation
    _loadFonts();

    Scenes::SceneManager* sm = Engine::CORE ? &Engine::CORE->GetSceneManager() : nullptr;
    m_fileMenu.Initialize(sm);

    // Wire up fonts to file menu so it can render bold asterisk
    m_fileMenu.SetFonts(m_mainFont, m_boldFont);

    // Wire up hierarchy panel to file menu for entity order preservation
    m_fileMenu.SetHierarchyPanel(&m_hierarchyWindow);

    // Wire up playback state getter to file menu for edit/play mode checking
    m_fileMenu.SetEditorStateGetter([this]() { return m_playback.GetEditorState(); });

    // ===================================================================
    // Subscribe to Engine Messages
    // ===================================================================
    
    // Subscribe to entity creation events
    m_entityCreatedSubscription = Messaging::MessageSystem::Subscribe<Messaging::EntityCreated>(
        [this](const Messaging::EntityCreated& evt) {
            LOG_INFO("[LevelEditor] Entity created: " << evt.EntityId);
            // Hierarchy panel will rebuild its tree automatically
        }
    );

    // Subscribe to entity destruction events
    m_entityDestroyedSubscription = Messaging::MessageSystem::Subscribe<Messaging::EntityDestroyed>(
        [this](const Messaging::EntityDestroyed& evt) {
            LOG_INFO("[LevelEditor] Entity destroyed: " << evt.EntityId);
            // Clear selection if the destroyed entity was selected
            if (m_hierarchyWindow.GetPrimarySelectedEntity() == evt.EntityId) {
                m_hierarchyWindow.SetSelectedEntity(ECS::Entity::NPOS32);
                m_inspector.ClearSelection();
                if (m_sceneViewport.HasValidWorld()) {
                    m_sceneViewport.SetSelectedEntity(ECS::Entity::NPOS32);
                }
                if (m_gameViewport.HasValidWorld()) {
                    m_gameViewport.SetSelectedEntity(ECS::Entity::NPOS32);
                }
            }
        }
    );

    // Subscribe to scene modified events
    m_sceneModifiedSubscription = Messaging::MessageSystem::Subscribe<Messaging::SceneModified>(
        [this](const Messaging::SceneModified& evt) {
            // Mark the scene as dirty in the file menu
            if (Engine::CORE->GetSceneManager().GetActive()) {
                LOG_DEBUG("[LevelEditor] Scene modified: " << evt.Reason);
                m_fileMenu.MarkSceneDirty();
            }
        }
    );

    // Subscribe to viewport layout requests from the scene viewport header
    m_viewportLayoutSubscription = Messaging::MessageSystem::Subscribe<Messaging::EditorViewportLayoutRequested>(
        [this](const Messaging::EditorViewportLayoutRequested& evt) {
            // Clamp layout requests and rebuild dock layout next frame.
            m_viewportLayoutPreset = std::max(1, std::min(4, evt.Layout));
            m_dockLayoutBuilt = false;
        }
    );

    // Wire up file menu to entity actions for dirty tracking
    m_entityActions.SetFileMenu(&m_fileMenu);

    // Undo system
    m_entityActions.SetUndoSystem(&m_undoSystem);

    if (m_world) {
        m_undoSystem.Initialize(m_world, 50);
        // LOG_INFO("[LevelEditor] Undo system initialized with world");
    }

    // Register all panels with their initialization and render callbacks.
    // Centralizes panel lifecycle management and reduces code duplication.
    _registerPanel("Playback Controls",
        [this]() {
            m_playback.Initialize(m_mainFont, m_symbolsFont, m_config.ToolbarHeight);
            // Register playback state change callback
            m_playback.OnStateChanged([this](const EditorState oldState, const EditorState newState) {
                _onPlaybackStateChanged(oldState, newState);
                });
            // Wire play controls into file menu dirty tracking.
            m_playback.SetUnsavedChangesProvider([this]() { return m_fileMenu.HasUnsavedChanges(); });
            m_playback.SetSaveSceneCallback([this]() { m_fileMenu.SaveScene(); });
        },
        [this]() { m_playback.Render(); },
        [this](ECS::World* w) { m_playback.SetWorld(w); }
    );

    _registerPanel("Asset Browser",
        [this]() {
            m_assetBrowser.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);
            m_assetBrowser.SetInspector(&m_inspector);
            // Wire up asset browser to load tilesets into the palette
            m_assetBrowser.SetOnLoadTileset([this](const std::string& path) {
                m_tilePalette.LoadTilesetFromPath(path);
            });
        },
        [this]() { m_assetBrowser.Render(); },
        [this](ECS::World* w) { m_assetBrowser.SetWorld(w); }
    );

    _registerPanel("Scene",
        [this]() {
            Scenes::SceneManager* sm = Engine::CORE ? &Engine::CORE->GetSceneManager() : nullptr;
            m_sceneViewport.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world, sm);
            m_sceneViewport.SetViewportType(BaseViewport::ViewportType::Scene);
            m_sceneViewport.SetUndoSystem(&m_undoSystem);
            // Register viewport selection callback
            m_sceneViewport.OnSelectionChanged([this](const EntityId id) {
                _onViewportSelectionChanged(id);
                });
        },
        [this]() { m_sceneViewport.ShowEditorWindows(); },
        [this](ECS::World* w) { m_sceneViewport.SetWorld(w); m_sceneViewport.SetUndoSystem(&m_undoSystem); }
    );

    _registerPanel("Game",
        [this]() {
            Scenes::SceneManager* sm = Engine::CORE ? &Engine::CORE->GetSceneManager() : nullptr;
            m_gameViewport.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world, sm);
            m_gameViewport.SetViewportType(BaseViewport::ViewportType::Game);
            m_gameViewport.SetUndoSystem(&m_undoSystem);
        },
        [this]() { m_gameViewport.ShowEditorWindows(); },
        [this](ECS::World* w) { m_gameViewport.SetWorld(w); m_gameViewport.SetUndoSystem(&m_undoSystem); }
    );

    _registerPanel("Hierarchy",
        [this]() { m_hierarchyWindow.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world, &m_entityActions); 
                   m_hierarchyWindow.SetViewport(&m_sceneViewport); 
                   m_hierarchyWindow.SetFileMenu(&m_fileMenu);
                   m_hierarchyWindow.SetUndoSystem(&m_undoSystem); // Enable reorder undo/redo.
        },
        [this]() { m_hierarchyWindow.Render(); },
        [this](ECS::World* w) { m_hierarchyWindow.SetWorld(w); m_hierarchyWindow.SetUndoSystem(&m_undoSystem); } // Keep undo wired after world changes.
    );

    _registerPanel("Inspector",
        [this]() {
            m_inspector.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);
            // WIRE UP FILE MENU to inspector
            m_inspector.SetFileMenu(&m_fileMenu);
            m_inspector.SetUndoSystem(&m_undoSystem);
        },
        [this]() { m_inspector.Render(); },
        [this](ECS::World* w) { m_inspector.SetWorld(w); }
    );

    _registerPanel("Tile Palette",
        [this]() {
            m_tilePalette.Initialize(nullptr, nullptr, m_world);
        },
        [this]() { m_tilePalette.Render(); },
        [this](ECS::World* w) { m_tilePalette.SetWorld(w); }
    );

    _registerPanel("Layers",
        [this]() {
            m_layersPanel.Initialize(m_mainFont, m_boldFont, m_symbolsFont);
            m_layersPanel.SetFileMenu(&m_fileMenu);
            m_layersPanel.SetUndoSystem(&m_undoSystem);
        },
        [this]() { m_layersPanel.Render(); },
        [this](ECS::World* w) { m_layersPanel.SetWorld(w); }
    );

    // Register Console Panel
    _registerPanel("Console",
        [this]() {
            m_console.Initialize(m_mainFont, m_boldFont, m_symbolsFont);

            // Connect Logger to Console - use singleton instance
            Logger::Get().SetConsoleCallback([this](const LogLevel level, const LogSource source,
                const std::string& timestamp, const std::string& message) {
                    m_console.AddMessage(level, source, timestamp, message);
                });
        },
        [this]() { m_console.Render(); },
        nullptr
    );

    // Register Compile Panel as an editor-global modal for blocking compilation status
    _registerPanel("Compile",
        [this]() {
            CompilePanel::Initialize();
        },
        [this]() { CompilePanel::Render(); },
        nullptr
    );

    // Register Performance panel (monitoring)
    _registerPanel("Performance",
        [this]() {
            m_performancePanel.Initialize(m_mainFont, m_boldFont);
        },
        [this]() { 
            // Get SystemManager from engine
            ECS::SystemManager* systemManager = nullptr;
            if (Engine::CORE) {
                systemManager = &Engine::CORE->GetSystemManager();
            }
            m_performancePanel.SetSystemManager(systemManager);
            m_performancePanel.Render(m_playback.IsPlaying()); 
        },
        [this](ECS::World* w) { m_performancePanel.SetWorld(w); }
    );

    // Register Systems panel (shows registered C# and C++ systems)
    _registerPanel("Systems",
        [this]() {
            m_systemsPanel.Initialize(m_mainFont, m_boldFont);
        },
        [this]() { 
            // Get SystemManager from engine
            ECS::SystemManager* systemManager = nullptr;
            if (Engine::CORE) {
                systemManager = &Engine::CORE->GetSystemManager();
            }
            m_systemsPanel.Render(systemManager);
        },
        [this](ECS::World* w) { m_systemsPanel.SetWorld(w); }
    );

    // Initialize all registered panels
    _initializePanels();

    // Provide panels with cross-panel pointers
    m_layersPanel.SetHierarchy(&m_hierarchyWindow);
    m_layersPanel.SetEntityActions(&m_entityActions);

    // Wire up file menu to viewports after panels are initialized
    m_sceneViewport.SetFileMenu(&m_fileMenu);
    m_gameViewport.SetFileMenu(&m_fileMenu);
    m_sceneViewport.SetTilePalette(&m_tilePalette);

    // Set up hierarchy selection callback to sync with inspector and viewports
    m_hierarchyWindow.OnSelectionChanged([this](const EntityId id) {
        if (!m_world) {
            m_inspector.ClearSelection();
            if (m_sceneViewport.HasValidWorld()) m_sceneViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            if (m_gameViewport.HasValidWorld()) m_gameViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            return;
        } // Clear when no world

        if (id == ECS::Entity::NPOS32) {
            m_inspector.ClearSelection();
            if (m_sceneViewport.HasValidWorld()) m_sceneViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            if (m_gameViewport.HasValidWorld()) m_gameViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            return;
        } // Clear when no entity

        // Resolve the entity to the current generation before checking aliveness
        const ECS::Entity e = m_world->Resolve(id);
        if (m_world->IsAlive(e)) {
            m_inspector.InspectEntity(id); // Inspect when valid
            if (m_sceneViewport.HasValidWorld()) m_sceneViewport.SetSelectedEntity(id);
            if (m_gameViewport.HasValidWorld()) m_gameViewport.SetSelectedEntity(id);
        }
        else {
            m_inspector.ClearSelection(); // Clear when entity is dead
            if (m_sceneViewport.HasValidWorld()) m_sceneViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            if (m_gameViewport.HasValidWorld()) m_gameViewport.SetSelectedEntity(ECS::Entity::NPOS32);
        }
        });
}

void LevelEditor::_loadFonts() {
    auto& io = ImGui::GetIO();
    float textFontSize = m_config.TextFontSize;

    if (!m_mainFont && io.Fonts->Fonts.empty()) {
        m_mainFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/Open_Sans/static/OpenSans-Medium.ttf",
            textFontSize
        );
        if (!m_mainFont) {
            LOG_ERROR("Failed to load Open Sans Medium font");
            m_mainFont = io.Fonts->AddFontDefault();
        }
    }
    else if (!m_mainFont) {
        m_mainFont = io.Fonts->Fonts[0];
    }

    if (!m_boldFont && io.Fonts->Fonts.size() < 2) {
        m_boldFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/Open_Sans/static/OpenSans-ExtraBold.ttf",
            textFontSize
        );
        if (!m_boldFont) {
            LOG_ERROR("Failed to load Open Sans ExtraBold font");
            m_boldFont = io.Fonts->AddFontDefault();
        }
    }
    else if (!m_boldFont) {
        m_boldFont = io.Fonts->Fonts[1];
    }

    float iconFontSize = m_config.IconFontSize;
    static constexpr ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };
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

    if (!io.Fonts->Fonts.empty() && !io.Fonts->IsBuilt()) {
        io.Fonts->Build();
    }
}

// -------------------------------------------------------------------------
// Frame Processing
// -------------------------------------------------------------------------
// Begin frame - handle input and request picking before systems update
void LevelEditor::BeginFrame() {
    auto& guiCtx = ECS::UI::GUIContext::Get();
    guiCtx.UseViewportBounds = false;
    // Begin frame for all viewports - handle input and request picking
    m_sceneViewport.BeginFrame();
    m_gameViewport.BeginFrame();
}

// -------------------------------------------------------------------------
// Update Loop
// -------------------------------------------------------------------------
// Process input and in world interactions for editor panels
void LevelEditor::Update() {

    // Apply global shortcuts
    m_fileMenu.HandleShortcuts(m_uiScale);

    // Auto-sync to active scene world if it changed (e.g. via File > New Scene)
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
    m_sceneViewport.HandleInWorldInteraction(); // Handle scene viewport interactions and camera input control
    m_gameViewport.HandleInWorldInteraction(); // Handle game viewport interactions

    // Ensure the engine's RendererSystem uses the editor viewport camera
    // for the upcoming systems update. This must happen here because
    // systems are executed after Editor::Update() but before Editor::Render().
    if (Engine::CORE) {
        auto* rendererSystem = Engine::CORE->GetSystemManager().GetSystem<ECS::RendererSystem>();
        if (rendererSystem) {
            auto* editorCam = m_sceneViewport.GetEditorCamera();
            if (editorCam) {
                rendererSystem->SetCamera(editorCam->GetCamera());
            }
        }
    }

    if (m_playback.IsPlaying()) {
        Engine::CORE->GetSceneManager().Update(); // Updates scenemanager to run Audio
    }
}

// -------------------------------------------------------------------------
// Event Handlers
// -------------------------------------------------------------------------
// Handle playback state changes (called by Playback via callback)
void LevelEditor::_onPlaybackStateChanged(EditorState oldState, EditorState newState) {
    // Any editor-specific logic that needs to happen on state change goes here
    // (The time scale is already handled by Playback itself)
    LOG_INFO("Playback state changed from " << static_cast<int>(oldState) << " to " << static_cast<int>(newState));

    // Handle audio pause/resume based on state
    auto audioService = Engine::CORE ? Engine::CORE->GetAudioService() : nullptr;
    if (audioService) {
        if (newState == EditorState::Paused || newState == EditorState::Step) {
            // Pause all audio when entering paused or step state
            audioService->PauseAll();
        }
        else if (newState == EditorState::Play) {
            // Resume audio when entering play state
            audioService->ResumeAll();
        }
        // When entering Edit state, audio will be stopped by OnSceneStop callback in AudioSystem
    }

    // Handle state transitions
    if (newState == EditorState::Edit) {
        // Entity IDs are preserved during restore, so keep selection intact.
        // Rebuild entity order to reflect restored hierarchy.
        m_hierarchyWindow.RebuildEntityOrder();
    }

    if (newState == EditorState::Play && m_console.IsClearOnPlayBuildEnabled()) {
        m_console.Clear();
    }
}

// Handle viewport selection changes (called by Viewport via callback)
void LevelEditor::_onViewportSelectionChanged(const EntityId id) {
    if (!m_world) {
        m_inspector.ClearSelection();
        m_hierarchyWindow.SetSelectedEntity(ECS::Entity::NPOS32);
        return;
    }

    if (id == ECS::Entity::NPOS32) {
        m_inspector.ClearSelection();
        m_hierarchyWindow.SetSelectedEntity(ECS::Entity::NPOS32);
        return;
    }

    // Validate entity before inspecting
    const ECS::Entity e = m_world->Resolve(id);
    if (m_world->IsAlive(e)) {
        m_inspector.InspectEntity(id);
        m_hierarchyWindow.SetSelectedEntity(id);
    }
        else {
            m_inspector.ClearSelection();
            m_hierarchyWindow.SetSelectedEntity(ECS::Entity::NPOS32);
        }
}

// -------------------------------------------------------------------------
// Render
// -------------------------------------------------------------------------
// Render dock space and editor panels with a fallback when world is missing
void LevelEditor::Render() {
    if (ImGui::BeginMainMenuBar()) {
        m_fileMenu.RenderFileMenu();
        m_fileMenu.RenderEditMenu();
        m_fileMenu.RenderViewMenu(m_uiScale);
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
// End Frame
// -------------------------------------------------------------------------
// End frame - resolve picking and update selection after rendering
void LevelEditor::EndFrame() {
    // End frame for all viewports - resolve picking and update selection
    m_sceneViewport.EndFrame();
    m_gameViewport.EndFrame();
}

// -------------------------------------------------------------------------
// World Management
// -------------------------------------------------------------------------
// Update the world reference and propagate it to all editor panels
void LevelEditor::SetWorld(ECS::World* world) {
    m_world = world; // Store new world

    // Undo system
    if (world) { m_undoSystem.Initialize(world, 50); }

    // Propagate world to all registered panels using centralized system
    _updatePanelWorlds(world);
    
    // Reset performance panel to show paused message for new scene
    m_performancePanel.ResetForNewScene();
}

// Set the active Scenes::Scene so EntityActions has the scene pointer
void LevelEditor::SetScene(Scenes::Scene* scene) {
    m_entityActions.SetScene(scene);
    ECS::World* world = scene ? &scene->GetWorld() : nullptr;
    SetWorld(world);
    
    // Auto-Initialize TileMap if needed
    if (world) {
        bool hasTileMap = false;
        
        // Iterate all entities with TileMapComponent
        world->Each<ECS::Components::TileMapComponent>([&](ECS::Entity e, ECS::Components::TileMapComponent& tc) {
            (void)e;
            hasTileMap = true;
            m_tilePalette.Initialize(tc.Map, m_tilePalette.GetTileset(), world);
        });

        if (!hasTileMap) {
            // Create a default TileMap
            ECS::Entity e = world->Create();
            ECS::Components::Name nameComp;
            nameComp.Value = ECS::StringTable::Intern("TileMap");
            world->Add<ECS::Components::Name>(e, nameComp);
            
            auto tileMap = std::make_shared<TileMap>(32.0f); // Default size
            tileMap->AddLayer(100, 100); //AddLayer to set the 100x100 dimensions
            world->Add<ECS::Components::TileMapComponent>(e, tileMap);
            
            // Initialize palette with new map
            m_tilePalette.Initialize(tileMap, m_tilePalette.GetTileset(), world);
            
            LOG_INFO("Auto-initialized default TileMap entity with 32.0f tileSize.");
        }
    }

    // Propagate scene to panels that require Scene access
    m_layersPanel.SetScene(scene);
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

EditorState LevelEditor::GetEditorState() const {
    return m_playback.GetEditorState();
}
