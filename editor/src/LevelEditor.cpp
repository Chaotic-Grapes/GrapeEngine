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

#include "LevelEditor.h"
#include "core/Logger.h"
#include "core/EditorCallbacks.h"
#include "ViewportPicking.h"
#include <imgui.h>
#include "EditorStyle.h"
#include "graphics/graphicsConfig.hpp"
#include "ecs/systems/RendererSystem.h"
#include <imgui_internal.h>
#include <core/Application.h>
#include "services/Time.h"
#include "AudioAssetLibrary.h"
#include "UndoSystem.h"

// Create the editor and initialize panel members and config
LevelEditor::LevelEditor(ECS::World* world, const LevelEditorConfig& config, Scenes::Scene* scene)
    : m_world(world), m_config(config), m_playback(world), m_symbolsFont(nullptr),
    m_mainFont(nullptr), m_boldFont(nullptr), m_assetBrowser(), m_viewport(),
    m_hierarchyWindow(), m_inspector(), m_entityActions(scene) {
    // Defer panel initialization to Initialize to use loaded fonts
}

// Destroy the editor instance without owning the world
LevelEditor::~LevelEditor() {
    // Unsubscribe from engine messages
    Messaging::MessageSystem::Unsubscribe<Messaging::EntityCreated>(m_entityCreatedSubscription);
    Messaging::MessageSystem::Unsubscribe<Messaging::EntityDestroyed>(m_entityDestroyedSubscription);
    Messaging::MessageSystem::Unsubscribe<Messaging::SceneModified>(m_sceneModifiedSubscription);
    
    // Clear engine callbacks
    Engine::EditorCallbackRegistry::Get().ClearAll();
    
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

    // First split: toolbar at the very top (calculate ratio from config)
    float toolbarRatio = m_config.ToolbarHeight / vp->Size.y;
    ImGuiID toolbarNode, mainAreaNode;
    ImGui::DockBuilderSplitNode(m_dockspaceId, ImGuiDir_Up, toolbarRatio, &toolbarNode, &mainAreaNode);

    // Hide tab bar and disable resizing on toolbar node
    ImGuiDockNode* toolbarDockNode = ImGui::DockBuilderGetNode(toolbarNode);
    toolbarDockNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResize;

    ImGuiID leftCenterNode, rightNode;
    // Split main area: carve right strip (25% width) for inspectors
    // Params: (source_node, direction, size_ratio, out_id_primary, out_id_remaining)
    ImGui::DockBuilderSplitNode(mainAreaNode, ImGuiDir_Right, 0.25f, &rightNode, &leftCenterNode);  // Reserve right strip

    ImGuiID topSection, assetBrowserNode;
    // Split left-center area vertically: top work area (65%), bottom asset browser (35%)
    ImGui::DockBuilderSplitNode(leftCenterNode, ImGuiDir_Up, 0.65f, &topSection, &assetBrowserNode); // Split for assets

    ImGuiID leftTopNode, viewportNode;
    // Split top work area horizontally: left hierarchy (33%), center viewport (67%)
    ImGui::DockBuilderSplitNode(topSection, ImGuiDir_Left, 0.333f, &leftTopNode, &viewportNode); // Hierarchy strip

    // Map panels to target nodes to realize the layout
    ImGui::DockBuilderDockWindow("Game Controls", toolbarNode);  // Toolbar at top
    ImGui::DockBuilderDockWindow("Hierarchy", leftTopNode);
    ImGui::DockBuilderDockWindow("Scene", viewportNode);
    ImGui::DockBuilderDockWindow("Game", viewportNode);
    ImGui::DockBuilderDockWindow("Prefab Editor", rightNode);
    ImGui::DockBuilderDockWindow("Property Editor", rightNode);
    ImGui::DockBuilderDockWindow("Asset Browser", assetBrowserNode);
    ImGui::DockBuilderDockWindow("Console", assetBrowserNode);
    ImGui::DockBuilderDockWindow("Performance", assetBrowserNode);

    ImGui::DockBuilderFinish(m_dockspaceId); // Finalize docking layout
    
    // Programmatically select Asset Browser tab by default
    ImGui::SetWindowFocus("Asset Browser");
    
    m_dockLayoutBuilt = true;                // Mark layout as built
}

// Invalidate the layout so the next frame rebuilds it on new size
void LevelEditor::OnWindowResized(int width, int height) {
    (void)width;
    (void)height;
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
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorStyle::Transparent);       // Transparent host background

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

    ImGuiStyle& style = ImGui::GetStyle();

    // Initialize audio asset library:
    // this will scan for folder assets/Audio
    AudioAssetLibrary::Get().Refresh("assets/Audio");

    style.ChildBorderSize = 0.75f; // Subtle child border for visual separation
    _loadFonts();

    Scenes::SceneManager* sm = Engine::CORE ? &Engine::CORE->GetSceneManager() : nullptr;
    m_fileMenu.Initialize(sm);

    // Wire up fonts to file menu so it can render bold asterisk
    m_fileMenu.SetFonts(m_mainFont, m_boldFont);

    // Wire up hierarchy panel to file menu for entity order preservation
    m_fileMenu.SetHierarchyPanel(&m_hierarchyWindow);

    // ===================================================================
    // Register Editor Callbacks with Engine
    // ===================================================================
    auto& callbacks = Engine::EditorCallbackRegistry::Get();

    // Register entity selection handler
    callbacks.RegisterEntitySelection([this](uint32_t entityId, bool addToSelection) {
        if (addToSelection) {
            // Multi-select not yet implemented in hierarchy
            LOG_WARNING("[LevelEditor] Multi-select not yet implemented");
        }
        m_hierarchyWindow.SetSelectedEntity(entityId);
        m_inspector.InspectEntity(entityId);
        if (m_viewport.HasValidWorld()) {
            m_viewport.SetSelectedEntity(entityId);
        }
    });

    // Register inspector refresh handler
    callbacks.RegisterInspectorRefresh([this](uint32_t entityId) {
        if (entityId == 0) {
            // Refresh current selection - just re-inspect it
            EntityId currentId = m_hierarchyWindow.GetPrimarySelectedEntity();
            if (currentId != 0) {
                m_inspector.InspectEntity(currentId);
            }
        } else {
            m_inspector.InspectEntity(entityId);
        }
    });

    // Register notification handler
    callbacks.RegisterNotification([this](int severity, const std::string& title,
                                         const std::string& message, float duration) {
        // Forward to console with appropriate log level
        (void)duration; // Duration not used by console currently
        LogLevel level;
        switch (severity) {
            case 0: level = LogLevel::Info; break;
            case 1: level = LogLevel::Warning; break;
            case 2: level = LogLevel::Error; break;
            case 3: level = LogLevel::Info; break; // Success as Info
            default: level = LogLevel::Info; break;
        }
        m_console.AddMessage(level, LogSource::Engine, "", title + ": " + message);
    });

    // Register debug draw handler
    callbacks.RegisterDebugDraw([this](ECS::World* world) {
        // Editor can implement debug visualization here
        // For now, this is a placeholder
        (void)world;
    });

    // Register editor camera provider
    callbacks.RegisterEditorCamera([this](glm::vec3& outPos, glm::mat4& outView, glm::mat4& outProj) {
        auto* cam = m_viewport.GetEditorCamera();
        if (cam) {
            outPos = cam->GetPosition();
            outView = cam->GetViewMatrix();
            outProj = cam->GetProjectionMatrix();
            return true;
        }
        return false;
    });

    // Register viewport picking using editor-side ViewportPicking utility
    callbacks.RegisterPick([this](float screenX, float screenY) {
        // Get the renderer system from the viewport
        if (!m_viewport.HasValidWorld()) {
            return uint32_t(0);
        }
        
        // Check if viewport is hovered (don't pick if mouse is outside viewport)
        if (!m_viewport.IsViewportHovered()) {
            return uint32_t(0);
        }
        
        // Get viewport bounds for coordinate conversion
        ImVec2 viewportPos = m_viewport.GetSceneDrawPos();
        ImVec2 viewportSize = m_viewport.GetSceneDrawSize();
        
        // Use editor-side picking utility (no engine coupling)
        auto* rendererSystem = ECS::RendererSystem::GetInstance();
        if (rendererSystem) {
            return Editor::ViewportPicking::PickEntityAtScreenPosition(
                screenX, screenY,
                glm::vec2(viewportPos.x, viewportPos.y),    // using glm because ViewportPicking uses glm
                glm::vec2(viewportSize.x, viewportSize.y),  // using glm because ViewportPicking uses glm
                rendererSystem
            );
        }
        
        return uint32_t(0);
    });

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
                m_hierarchyWindow.SetSelectedEntity(0);
                m_inspector.ClearSelection();
                if (m_viewport.HasValidWorld()) {
                    m_viewport.SetSelectedEntity(0);
                }
            }
        }
    );

    // Subscribe to scene modified events
    m_sceneModifiedSubscription = Messaging::MessageSystem::Subscribe<Messaging::SceneModified>(
        [this](const Messaging::SceneModified& evt) {
            // Mark the scene as dirty in the file menu
            if (m_fileMenu.GetSceneManager()) {
                LOG_DEBUG("[LevelEditor] Scene modified: " << evt.Reason);
                m_fileMenu.MarkSceneDirty();
            }
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

    _registerPanel("Scene",
        [this]() {
            Scenes::SceneManager* sm = Engine::CORE ? &Engine::CORE->GetSceneManager() : nullptr;
            m_viewport.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world, sm);
            m_viewport.SetUndoSystem(&m_undoSystem);
            // Register viewport selection callback
            m_viewport.OnSelectionChanged([this](EntityId id) {
                _onViewportSelectionChanged(id);
                });
        },
        [this]() { m_viewport.ShowEditorWindows(); },
        [this](ECS::World* w) { m_viewport.SetWorld(w); m_viewport.SetUndoSystem(&m_undoSystem); }
    );

    _registerPanel("Hierarchy",
        [this]() { m_hierarchyWindow.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world, &m_entityActions); 
                   m_hierarchyWindow.SetViewport(&m_viewport); 
                   m_hierarchyWindow.SetFileMenu(&m_fileMenu);
        },
        [this]() { m_hierarchyWindow.Render(); },
        [this](ECS::World* w) { m_hierarchyWindow.SetWorld(w); }
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

    // Register Console Panel
    _registerPanel("Console",
        [this]() {
            m_console.Initialize(m_mainFont, m_boldFont, m_symbolsFont);

            // Connect Logger to Console - use singleton instance
            Logger::Get().SetConsoleCallback([this](LogLevel level, LogSource source,
                const std::string& timestamp, const std::string& message) {
                    m_console.AddMessage(level, source, timestamp, message);
                });
        },
        [this]() { m_console.Render(); },
        nullptr
    );

    // Register Performance panel (monitoring)
    _registerPanel("Performance",
        [this]() {
            m_performancePanel.Initialize(m_mainFont, m_boldFont);
        },
        [this]() { m_performancePanel.Render(m_playback.IsPlaying()); },
        nullptr//[this](ECS::World* w) { void(*w); } // No world needed
    );

    // Initialize all registered panels
    _initializePanels();

    // Wire up file menu to viewport after panels are initialized
    m_viewport.SetFileMenu(&m_fileMenu);

    // Set up hierarchy selection callback to sync with inspector
    m_hierarchyWindow.OnSelectionChanged([this](EntityId id) {
        if (!m_world) {
            m_inspector.ClearSelection();
            if (m_viewport.HasValidWorld()) m_viewport.SetSelectedEntity(ECS::Entity::NPOS32);
            return;
        } // Clear when no world

        if (id == ECS::Entity::NPOS32) {
            m_inspector.ClearSelection();
            if (m_viewport.HasValidWorld()) m_viewport.SetSelectedEntity(ECS::Entity::NPOS32);
            return;
        } // Clear when no entity

        // Resolve the entity to the current generation before checking aliveness
        ECS::Entity e = m_world->Resolve(id);
        if (m_world->IsAlive(e)) {
            m_inspector.InspectEntity(id); // Inspect when valid
            if (m_viewport.HasValidWorld()) m_viewport.SetSelectedEntity(id);
        }
        else {
            m_inspector.ClearSelection(); // Clear when entity is dead
            if (m_viewport.HasValidWorld()) m_viewport.SetSelectedEntity(ECS::Entity::NPOS32);
        }
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
    m_viewport.HandleInWorldInteraction(); // Handle viewport interactions and camera input control

    if (m_playback.IsPlaying()) {
        Engine::CORE->GetSceneManager().Update(); // Updates scenemanager to run Audio
    }
}

// -------------------------------------------------------------------------
// Event Handlers
// -------------------------------------------------------------------------
// Handle playback state changes (called by Playback via callback)
void LevelEditor::_onPlaybackStateChanged(Playback::GameState oldState, Playback::GameState newState) {
    // Any editor-specific logic that needs to happen on state change goes here
    // (The time scale is already handled by Playback itself)
    LOG_INFO("Playback state changed from " << static_cast<int>(oldState) << " to " << static_cast<int>(newState));

    // Handle state transitions
    if (newState == Playback::GameState::Stopped) {
        // Note: Entity IDs are now preserved during restore, so selection can remain valid
        // However, we still clear selection as a safe UX pattern when stopping play mode
        m_hierarchyWindow.SetSelectedEntity(0);
        m_inspector.ClearSelection();

        // Rebuild entity order to reflect restored hierarchy
        m_hierarchyWindow.RebuildEntityOrder();
    }
}

// Handle viewport selection changes (called by Viewport via callback)
void LevelEditor::_onViewportSelectionChanged(EntityId id) {
    if (!m_world) {
        m_inspector.ClearSelection();
        m_hierarchyWindow.SetSelectedEntity(0);
        return;
    }

    if (id == 0 || id == ECS::Entity::NPOS32) {
        m_inspector.ClearSelection();
        m_hierarchyWindow.SetSelectedEntity(0);
        return;
    }

    // Validate entity before inspecting
    ECS::Entity e = m_world->Resolve(id);
    if (m_world->IsAlive(e)) {
        m_inspector.InspectEntity(id);
        m_hierarchyWindow.SetSelectedEntity(id);
    }
    else {
        m_inspector.ClearSelection();
        m_hierarchyWindow.SetSelectedEntity(0);
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
