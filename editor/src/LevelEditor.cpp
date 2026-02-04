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
#include <core/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include "CompilePanel.h"
#include "TilePalettePanel.h"
#include "services/ResourceManager.h"
#include "graphics/texture.hpp"
#include "ecs/StringTable.h"
#include "core/World/TileTypes.hpp"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cmath>

#ifdef ERROR
#undef ERROR
#endif

#ifdef max
#undef max
#endif

namespace {
    // Normalize file extensions for comparisons.
    std::string ToLowerCopy(std::string value) {
        // Convert each character to lowercase for case-insensitive matching.
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    // Return true when the extension looks like a texture asset we can treat as a tileset.
    bool IsTilesetTextureExtension(const std::string& ext) {
        const std::string lower = ToLowerCopy(ext); // Normalize extension case.
        return lower == ".png" || lower == ".jpg" || lower == ".jpeg" || lower == ".tga" || lower == ".bmp";
    }

    // Build a tileset by slicing a texture into uniform grid cells.
    std::shared_ptr<Tileset> BuildTilesetFromTexture(const std::string& texturePath, uint32_t tilePixelSize) {
        auto texture = RM.Get<Texture>(texturePath); // Load (or fetch cached) texture.
        if (!texture) {
            LOG_WARNING("[TileMap] Failed to load tileset texture: " << texturePath);
            return nullptr;
        }

        const uint32_t texWidth = static_cast<uint32_t>(texture->Width()); // Texture width in pixels.
        const uint32_t texHeight = static_cast<uint32_t>(texture->Height()); // Texture height in pixels.
        const uint32_t tilePx = std::max(1u, tilePixelSize); // Clamp tile size to at least 1px.
        const uint32_t cols = texWidth / tilePx; // Number of tiles across.
        const uint32_t rows = texHeight / tilePx; // Number of tiles down.

        if (cols == 0 || rows == 0) {
            LOG_WARNING("[TileMap] Tileset texture too small for tile size: " << texturePath);
            return nullptr;
        }

        auto tileset = std::make_shared<Tileset>(static_cast<uint32_t>(texture->ID())); // Create tileset bound to texture ID.
        TileID id = 0; // Tile IDs start at 0.

        for (uint32_t row = 0; row < rows; ++row) {
            for (uint32_t col = 0; col < cols; ++col) {
                if (id >= TILE_ID_MASK) {
                    // Tile IDs are limited to 12 bits (see TileTypes.hpp).
                    return tileset;
                }

                // Compute UVs with row 0 at the top of the texture.
                const float u0 = static_cast<float>(col * tilePx) / static_cast<float>(texWidth);
                const float u1 = static_cast<float>((col + 1) * tilePx) / static_cast<float>(texWidth);
                const float v1 = 1.0f - static_cast<float>(row * tilePx) / static_cast<float>(texHeight);
                const float v0 = 1.0f - static_cast<float>((row + 1) * tilePx) / static_cast<float>(texHeight);

                TileUV uv{ u0, v0, u1, v1 }; // Store OpenGL-style UVs (v=0 at bottom).
                tileset->DefineTile(id, uv, CollisionType::NONE); // No collision by default.
                ++id; // Advance to the next tile ID.
            }
        }

        return tileset;
    }

    // Load a tilemap from disk or create a new one with defaults (no auto-save).
    std::shared_ptr<TileMap> LoadOrCreateTileMap(const std::string& mapPath, float tileWorldSize, uint32_t width, uint32_t height) {
        auto map = std::make_shared<TileMap>(tileWorldSize); // Create a tilemap with the requested world scale.

        if (!mapPath.empty() && std::filesystem::exists(mapPath)) {
            if (map->LoadMap(mapPath)) {
                return map; // Use the loaded map if it succeeds.
            }
            LOG_WARNING("[TileMap] Failed to load tilemap, creating a new one: " << mapPath);
        }

        map->AddLayer(width, height); // Create the base layer for a new map.

        return map;
    }
}

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
    // Ensure play-mode scene reloads don't reuse stale snapshots.
    m_fileMenu.SetPlaybackSnapshotClearCallback([this]() { m_playback.ClearSavedState(); });

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
        [this](ECS::World* w) {
            const bool preservePlayback = m_playback.GetEditorState() != EditorState::Edit;
            m_playback.SetWorld(w, preservePlayback);
        }
    );

    _registerPanel("Asset Browser",
        [this]() {
            m_assetBrowser.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);
            m_assetBrowser.SetInspector(&m_inspector);
            m_assetBrowser.SetSelectionChangedCallback([this](const std::string& assetPath) { _onAssetSelected(assetPath); });
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
            m_tilePalette.SetAssetDropCallback([this](const std::string& assetPath) { _onAssetSelected(assetPath); });
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
            _syncTilePaletteToSelection(ECS::Entity::NPOS32); // Clear tile palette when no world.
            return;
        } // Clear when no world

        if (id == ECS::Entity::NPOS32) {
            m_inspector.ClearSelection();
            if (m_sceneViewport.HasValidWorld()) m_sceneViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            if (m_gameViewport.HasValidWorld()) m_gameViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            _syncTilePaletteToSelection(ECS::Entity::NPOS32); // Clear tile palette when selection is empty.
            return;
        } // Clear when no entity

        // Resolve the entity to the current generation before checking aliveness
        const ECS::Entity e = m_world->Resolve(id);
        if (m_world->IsAlive(e)) {
            m_inspector.InspectEntity(id); // Inspect when valid
            if (m_sceneViewport.HasValidWorld()) m_sceneViewport.SetSelectedEntity(id);
            if (m_gameViewport.HasValidWorld()) m_gameViewport.SetSelectedEntity(id);
            _syncTilePaletteToSelection(id); // Sync tile palette to the newly selected entity.
        }
        else {
            m_inspector.ClearSelection(); // Clear when entity is dead
            if (m_sceneViewport.HasValidWorld()) m_sceneViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            if (m_gameViewport.HasValidWorld()) m_gameViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            _syncTilePaletteToSelection(ECS::Entity::NPOS32); // Clear tile palette for invalid entity.
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

    // Handle undo/redo shortcuts globally (not tied to a specific viewport).
    m_undoSystem.Update();

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

    // SceneManager::Update is already called in EditorService::Update.
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
        _syncTilePaletteToSelection(ECS::Entity::NPOS32); // Clear tile palette when no world.
        return;
    }

    if (id == ECS::Entity::NPOS32) {
        m_inspector.ClearSelection();
        m_hierarchyWindow.SetSelectedEntity(ECS::Entity::NPOS32);
        _syncTilePaletteToSelection(ECS::Entity::NPOS32); // Clear tile palette when no selection.
        return;
    }

    // Validate entity before inspecting
    const ECS::Entity e = m_world->Resolve(id);
    if (m_world->IsAlive(e)) {
        m_inspector.InspectEntity(id);
        m_hierarchyWindow.SetSelectedEntity(id);
        _syncTilePaletteToSelection(id); // Sync tile palette to viewport selection.
    }
        else {
            m_inspector.ClearSelection();
            m_hierarchyWindow.SetSelectedEntity(ECS::Entity::NPOS32);
            _syncTilePaletteToSelection(ECS::Entity::NPOS32); // Clear tile palette for invalid entity.
        }
}

void LevelEditor::_onAssetSelected(const std::string& assetPath) {
    if (!m_world) {
        return; // No active world, so ignore asset selections.
    }

    const std::filesystem::path path(assetPath); // Build a filesystem path for extension checks.
    if (std::filesystem::is_directory(path)) {
        return; // Ignore folder selections.
    }

    const std::string ext = path.extension().string(); // Read the file extension.
    if (!IsTilesetTextureExtension(ext)) {
        return; // Only handle image assets as tilesets.
    }

    // Pick a target entity: use the selected one if possible, otherwise create a new tilemap entity.
    ECS::Entity target = ECS::NULL_ENTITY; // Start with a null entity until we find or create one.
    const EntityId selectedId = m_hierarchyWindow.GetPrimarySelectedEntity(); // Read the current hierarchy selection.
    if (selectedId != ECS::Entity::NPOS32) {
        target = m_world->Resolve(selectedId); // Resolve the selected entity to the latest generation.
    }

    if (!m_world->IsAlive(target) || !m_world->Has<ECS::Components::TileMapComponent>(target)) {
        target = m_world->Create(); // Create a new entity for the tilemap.

        ECS::Components::Name name; // Add a readable name for the hierarchy.
        name.Value = ECS::StringTable::Intern("TileMap");
        m_world->Set<ECS::Components::Name>(target, name);

        ECS::Components::LocalTransform transform; // Ensure a transform exists for editor tooling.
        m_world->Set<ECS::Components::LocalTransform>(target, transform);
    }

    // Fetch or create the tilemap component and update its tileset path.
    ECS::Components::TileMapComponent comp{}; // Local copy for edits before writing back to the world.
    if (m_world->Has<ECS::Components::TileMapComponent>(target)) {
        comp = m_world->Get<ECS::Components::TileMapComponent>(target); // Pull existing data if present.
    }

    comp.TilesetTexturePath = ECS::StringTable::Intern(assetPath); // Store tileset texture path.

    if (comp.TileMapPath == 0) {
        // Default tilemap path: same folder + same stem with .tilemap extension.
        std::filesystem::path mapPath = path;
        mapPath.replace_extension(".tilemap");
        comp.TileMapPath = ECS::StringTable::Intern(mapPath.string());
    }

    m_world->Set<ECS::Components::TileMapComponent>(target, comp); // Persist component changes to the world.

    // Sync the palette/editor to this tilemap component immediately.
    _syncTilePaletteToSelection(target.Index); // Rebuild tile palette and renderer from this component.
}

void LevelEditor::_syncTilePaletteToSelection(const EntityId id) {
    if (!m_world) {
        return; // Nothing to sync without a world.
    }

    if (id == ECS::Entity::NPOS32) {
        // Clear palette if nothing is selected.
        m_activeTileMap.reset();
        m_activeTileset.reset();
        m_activeTileMapPath.clear();
        m_activeTilesetPath.clear();
        m_activeTileMapEntityId = ECS::Entity::NPOS32;
        m_tilePalette.SetEditingContext(nullptr, nullptr, std::string());
        if (auto* renderer = ECS::RendererSystem::GetInstance()) {
            renderer->ClearDebugTileMap();
        }
        return;
    }

    ECS::Entity entity = m_world->Resolve(id); // Resolve entity to current generation.
    if (!m_world->IsAlive(entity)) {
        return; // Ignore dead entities.
    }

    if (!m_world->Has<ECS::Components::TileMapComponent>(entity)) {
        return; // Only sync when a tilemap component is present.
    }

    const auto& comp = m_world->Get<ECS::Components::TileMapComponent>(entity); // Read the component data.
    std::string mapPath = ECS::StringTable::Resolve(comp.TileMapPath); // Resolve map path from StringId.
    const std::string tilesetPath = ECS::StringTable::Resolve(comp.TilesetTexturePath); // Resolve tileset path.

    if (tilesetPath.empty()) {
        return; // Can't build a tileset without a texture path.
    }

    if (mapPath.empty()) {
        // Synthesize a default tilemap path when none is stored yet.
        std::filesystem::path defaultPath = tilesetPath;
        defaultPath.replace_extension(".tilemap");
        mapPath = defaultPath.string();

        // Write the synthesized path back to the component so it persists in the scene.
        ECS::Components::TileMapComponent updated = comp;
        updated.TileMapPath = ECS::StringTable::Intern(mapPath);
        m_world->Set<ECS::Components::TileMapComponent>(entity, updated);
    }

    // Build runtime tileset and tilemap instances from the component data.
    m_activeTileset = BuildTilesetFromTexture(tilesetPath, comp.TilePixelSize); // Create tileset from texture grid.
    m_activeTileMap = LoadOrCreateTileMap(mapPath, comp.TileWorldSize, comp.DefaultWidth, comp.DefaultHeight); // Load or create tilemap data.
    m_activeTileMapPath = mapPath; // Cache map path for palette auto-save.
    m_activeTilesetPath = tilesetPath; // Cache tileset path for debugging and reloads.
    m_activeTileMapEntityId = entity.Index; // Track which entity owns the active tilemap.

    if (m_activeTileMap && m_activeTileset) {
        m_tilePalette.SetEditingContext(m_activeTileMap, m_activeTileset, m_activeTileMapPath);
        if (auto* renderer = ECS::RendererSystem::GetInstance()) {
            if (comp.Visible) {
                renderer->SetDebugTileMap(*m_activeTileMap, *m_activeTileset);
            } else {
                renderer->ClearDebugTileMap();
            }
        }
    } else {
        // Clear palette and debug draw if we failed to build the runtime data.
        m_tilePalette.SetEditingContext(nullptr, nullptr, std::string());
        if (auto* renderer = ECS::RendererSystem::GetInstance()) {
            renderer->ClearDebugTileMap();
        }
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

    // Clear tile palette state when switching scenes.
    m_activeTileMap.reset();
    m_activeTileset.reset();
    m_activeTileMapPath.clear();
    m_activeTilesetPath.clear();
    m_activeTileMapEntityId = ECS::Entity::NPOS32;
    m_tilePalette.SetEditingContext(nullptr, nullptr, std::string());

    if (auto* renderer = ECS::RendererSystem::GetInstance()) {
        renderer->ClearDebugTileMap(); // Remove any previous debug tilemap.
    }
}

// Set the active Scenes::Scene so EntityActions has the scene pointer
void LevelEditor::SetScene(Scenes::Scene* scene) {
    m_entityActions.SetScene(scene);
    ECS::World* world = scene ? &scene->GetWorld() : nullptr;
    SetWorld(world);
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
