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
#include "ecs/systems/BoidSystem.h"
#include "EditorStyle.h"
#include "EditorIcons.h"
#include "EditorECSUtils.h"
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

	// Resolve a project-relative path to an absolute path for loading assets (for existing maps)
    std::string ResolveProjectPathForLoad(const std::string& path) {
        if (path.empty() || !Engine::ProjectPaths::IsInitialized()) {
            return path;
        }

        std::filesystem::path fsPath(path);
        if (fsPath.is_absolute()) {
            return fsPath.lexically_normal().string();
        }

        return std::filesystem::path(Engine::ProjectPaths::ToAbsolutePath(path)).lexically_normal().string();
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

		// Attempt to load from disk if a path is provided
        // If loading fails, we will create a new map in memory
        if (!mapPath.empty()) {
			// Resolve the path relative to the project directory for better UX in editor (allows loading from project-relative paths)
            const std::string resolvedPath = ResolveProjectPathForLoad(mapPath);
            // Use the loaded map if it succeeds
            if (std::filesystem::exists(resolvedPath)) {
                if (map->LoadMap(resolvedPath)) {
                    LOG_INFO("[TileMap] Loaded tilemap: " << resolvedPath);
                    return map; 
                }
                LOG_WARNING("[TileMap] Failed to load tilemap, creating a new one: " << resolvedPath);
            } 
            else {
                LOG_WARNING("[TileMap] Tilemap file does not exist: " << resolvedPath);
            }
        }

		// If loading failed or no path provided, create the base layer for a new map in memory (not saved to disk yet)
        LOG_INFO("[TileMap] Creating new tilemap in memory for path=\"" << mapPath << "\"");
        map->AddLayer(width, height); 
        return map;
    }

	// Constant component name for lookups; avoid hardcoding strings in multiple places
    constexpr const char* kTileMapComponentName = "TileMapComponent";

	// Cached lookup for the TileMapComponent type ID to avoid repeated string lookups during iteration
    ECS::ComponentTypeId GetTileMapComponentId() {
        static ECS::ComponentTypeId id = ECS::NULL_COMPONENT_ID;
        if (id == ECS::NULL_COMPONENT_ID) {
            id = Editor::ECSUtils::GetComponentIdFromName(kTileMapComponentName);
        }
        return id;
    }

	// Utility to iterate over all entities with a TileMapComponent and apply a function to them
    template<typename Fn>
    void ForEachTileMapComponent(ECS::World* world, Fn&& fn) {
        if (!world) {
            return;
        }

		// Get the component type ID for TileMapComponent; if it doesn't exist, we can't proceed
        const ECS::ComponentTypeId id = GetTileMapComponentId();
        if (id == ECS::NULL_COMPONENT_ID) {
            return;
        }

		// Build a signature to query archetypes that contain the TileMapComponent
        const ECS::Signature signature({ id });
        const auto& archetypes = world->GetMatchingArchetypes(signature);

		// Iterate through matching archetypes and apply the function to each TileMapComponent instance
        for (ECS::Archetype* arch : archetypes) {
            if (!arch) {
                continue;
            }

			// Iterate through chunks in the archetype; each chunk contains a batch of entities with the same component layout
            const uint32_t chunkCount = arch->GetChunkCount();
            for (uint32_t ci = 0; ci < chunkCount; ci++) {
                ECS::Chunk* chunk = arch->GetChunk(ci);
                const uint32_t count = chunk->Count();

				// Iterate through entities in the chunk; apply the function to each valid TileMapComponent instance
                for (uint32_t slot = 0; slot < count; slot++) {
					// Get the entity for this slot; skip if it's null (empty slot)
                    const ECS::Entity entity = chunk->GetEntity(slot);
                    if (entity.IsNull()) {
                        continue;
                    }
					// Get a pointer to the TileMapComponent for this entity; skip if it's not present 
                    // (shouldn't happen due to signature filter, but guard against it)
                    auto* comp = static_cast<ECS::Components::TileMapComponent*>(arch->GetRaw(id, ci, slot));
                    if (!comp) {
                        continue;
                    }
					// Apply the provided function to this entity and its TileMapComponent
                    fn(entity, *comp);
                }
            }
        }
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
    Messaging::MessageSystem::Unsubscribe<Messaging::TileMapCollisionEditRequested>(m_tileMapCollisionEditSubscription);
    Messaging::MessageSystem::Unsubscribe<Messaging::EditorViewportLayoutRequested>(m_viewportLayoutSubscription);
    
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
    // Optional world setter for panels that require ECS context.
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

    // Reset the dock builder root node.
    ImGui::DockBuilderRemoveNode(m_dockspaceId);
    // Flags: DockSpace creates a root docking container; PassthruCentralNode lets the central area render game content underneath
    // Combine dock node flags using integer casts to avoid deprecated enum-| between different enum types (C5054)
    const ImGuiDockNodeFlags dockNodeFlags = static_cast<ImGuiDockNodeFlags>(
        static_cast<int>(ImGuiDockNodeFlags_DockSpace) | static_cast<int>(ImGuiDockNodeFlags_PassthruCentralNode)
    );
    // Create the dock builder root node.
    ImGui::DockBuilderAddNode(m_dockspaceId, dockNodeFlags);
    ImGui::DockBuilderSetNodeSize(m_dockspaceId, vp->Size); // Match viewport size

    // Target layout: left hierarchy, center viewport with controls on top,
    // bottom asset browser, right strip for inspector/prefab editors

    // First split: toolbar at the very top (calculate ratio from config)
    const float toolbarRatio = m_config.ToolbarHeight / vp->Size.y;
    ImGuiID toolbarNode, mainAreaNode;
    // Split the dock node for layout regions.
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
        // Dock a window into the layout region.
        ImGui::DockBuilderDockWindow("Game", sceneGameNode);
    // Layout preset 2: split Scene/Game side-by-side.
    } else if (m_viewportLayoutPreset == 2) {
        ImGuiID sceneNode, gameNode;
        // Split the dock node for layout regions.
        ImGui::DockBuilderSplitNode(sceneGameNode, ImGuiDir_Right, 0.5f, &gameNode, &sceneNode);
        ImGui::DockBuilderDockWindow("Scene", sceneNode);
        // Dock a window into the layout region.
        ImGui::DockBuilderDockWindow("Game", gameNode);
    // Layout preset 4: quad split for multiple viewports.
    } else {
        ImGuiID topNode, bottomNode;
        // Split the dock node for layout regions.
        ImGui::DockBuilderSplitNode(sceneGameNode, ImGuiDir_Down, 0.5f, &bottomNode, &topNode);
        ImGuiID topLeft, topRight;
        // Split the dock node for layout regions.
        ImGui::DockBuilderSplitNode(topNode, ImGuiDir_Right, 0.5f, &topRight, &topLeft);
        ImGuiID bottomLeft, bottomRight;
        // Split the dock node for layout regions.
        ImGui::DockBuilderSplitNode(bottomNode, ImGuiDir_Right, 0.5f, &bottomRight, &bottomLeft);
        ImGui::DockBuilderDockWindow("Scene", topLeft);
        // Dock a window into the layout region.
        ImGui::DockBuilderDockWindow("Game", topRight);
    }
    // Dock a window into the layout region.
    ImGui::DockBuilderDockWindow("Prefab Editor", rightNode);
    ImGui::DockBuilderDockWindow("Property Editor", rightNode);
    // Dock a window into the layout region.
    ImGui::DockBuilderDockWindow("Layers", rightNode);
    ImGui::DockBuilderDockWindow("Tile Palette", rightNode);
    // Dock a window into the layout region.
    ImGui::DockBuilderDockWindow("Asset Browser", assetBrowserNode);
    ImGui::DockBuilderDockWindow("Console", assetBrowserNode);
    // Dock a window into the layout region.
    ImGui::DockBuilderDockWindow("Performance", assetBrowserNode);
    ImGui::DockBuilderDockWindow("Systems", assetBrowserNode);
    ImGui::DockBuilderDockWindow("Game Configuration", assetBrowserNode);

    // Finalize the dock builder layout.
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

    // Set next window pos.
    ImGui::SetNextWindowPos(safePos);     // Position dock host under main menu bar
    ImGui::SetNextWindowSize(safeSize);   // Size dock host to fill viewport
    // Set next window viewport.
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

    // Push a temporary style override.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);                  // Square corners for host
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);                // No border around host
    // Push a temporary style override.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));     // No padding; dockspace fills fully
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorStyle::Transparent);       // Transparent host background

    // Begin invisible host window
    ImGui::Begin("MainDockSpaceHost", nullptr, hostFlags);
    ImGui::PopStyleColor();
    // Restore the previous style override.
    ImGui::PopStyleVar(3);

    m_dockspaceId = ImGui::GetID("MainDockSpace");                         // Stable ID for this dockspace
    m_sceneViewport.SetDefaultDockspaceId(m_dockspaceId);
    constexpr ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode; // Let central node pass content
    // Create the dock space for editor panels.
    ImGui::DockSpace(m_dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);        // Create dockspace filling the host window

    _buildDockLayout(); // Build once on first frame or after resize
    // End.
    ImGui::End();       // End host window
}

// -------------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------------
// Initialize fonts build atlas and set up editor panels and hooks
void LevelEditor::Initialize(const GLFWwindow* pWin) {
    if (!pWin) return;

    ImGuiStyle& style = ImGui::GetStyle();
    m_uiScale = EditorStyle::FontScale;
    ImGui::GetIO().FontGlobalScale = m_uiScale;

    // Initialize audio asset library:
    // this will scan for audio anywhere under the project root
    AudioAssetLibrary::Get().Refresh(Engine::ProjectPaths::GetProjectRoot());

    style.ChildBorderSize = 0.75f; // Subtle child border for visual separation
    _loadFonts();

    Scenes::SceneManager* sm = Engine::CORE ? &Engine::CORE->GetSceneManager() : nullptr;
    m_fileMenu.Initialize(sm);

    // Wire up fonts to file menu so it can render bold asterisk and icons.
    m_fileMenu.SetFonts(m_mainFont, m_boldFont);
    m_fileMenu.SetSymbolsFont(m_symbolsFont);

    // Wire up hierarchy panel to file menu for entity order preservation
    m_fileMenu.SetHierarchyPanel(&m_hierarchyWindow);
    // Wire up undo system so the Edit menu can call Undo/Redo.
    m_fileMenu.SetUndoSystem(&m_undoSystem);

    // Wire up playback state getter to file menu for edit/play mode checking
    m_fileMenu.SetEditorStateGetter([this]() { return m_playback.GetEditorState(); });
    // Ensure play-mode scene reloads don't reuse stale snapshots.
    m_fileMenu.SetPlaybackSnapshotClearCallback([this]() { m_playback.ClearSavedState(); });
    // Ensure tilemaps are flushed to disk before the scene serializer runs.
    m_fileMenu.SetPreSaveCallback([this](const std::string& scenePath) { _saveActiveTileMapAsset(scenePath); });
    m_fileMenu.SetProjectBrowserRequestCallback([this]() {
        if (m_projectBrowserRequest) {
            m_projectBrowserRequest();
        }
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

            // Push updated collision grid to BoidSystem when tilemap collision changes
            if (evt.Reason == "Tilemap collision" && m_activeTileMap) {
                if (auto* boidSystem = Engine::CORE->GetSystemManager().GetSystem<ECS::BoidSystem>()) {
                    boidSystem->UpdateCollisionGrid(*m_activeTileMap);
                }
            }
        }
    );

    // Subscribe to tilemap collision edit requests from the inspector
    m_tileMapCollisionEditSubscription = Messaging::MessageSystem::Subscribe<Messaging::TileMapCollisionEditRequested>(
        [this](const Messaging::TileMapCollisionEditRequested& evt) {
            if (!m_world) return;
            const ECS::Entity entity = m_world->Resolve(evt.EntityId);

			// Validate that the entity is alive and has a TileMapComponent before proceeding
            if (!m_world->IsAlive(entity) || !Editor::ECSUtils::HasComponent(m_world, entity, kTileMapComponentName)) {
                LOG_WARNING("[LevelEditor] Collision edit requested for invalid tilemap entity " << evt.EntityId);
                return;
            }

			// Set the active tilemap in the palette and switch to collision edit mode
            _setActiveTileMap(evt.EntityId);
            m_tilePalette.SetCollisionEditActive(true);
            m_focusTilePaletteNextFrame = true;
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
            m_playback.SetHasScenePathProvider([this]() { return !m_fileMenu.GetCurrentScenePath().empty(); });
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
            // Asset Browser selection should be passive; tilesets are applied only via explicit drag-drop
            m_assetBrowser.SetSelectionChangedCallback(nullptr);
            m_assetBrowser.SetSceneOpenCallback([this](const std::string& scenePath) { m_fileMenu.OpenSceneFromPath(scenePath); });
        },
        [this]() { m_assetBrowser.Render(); },
        // Set world.
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
        // Set undo system.
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
        // Set undo system.
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
        // Set world.
        [this](ECS::World* w) { m_inspector.SetWorld(w); }
    );

    _registerPanel("Tile Palette",
        [this]() {
            m_tilePalette.Initialize(nullptr, nullptr, m_world, m_symbolsFont, m_boldFont);
            m_tilePalette.SetAssetDropCallback([this](const std::string& assetPath) { _onAssetSelected(assetPath); });
            m_tilePalette.SetUndoSystem(&m_undoSystem);
        },
        [this]() { m_tilePalette.Render(); },
        // Set world.
        [this](ECS::World* w) { m_tilePalette.SetWorld(w); }
    );

    _registerPanel("Layers",
        [this]() {
            m_layersPanel.Initialize(m_mainFont, m_boldFont, m_symbolsFont);
            m_layersPanel.SetFileMenu(&m_fileMenu);
            m_layersPanel.SetUndoSystem(&m_undoSystem);
        },
        [this]() { m_layersPanel.Render(); },
        // Set world.
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
            // Initialize.
            CompilePanel::Initialize();
        },
        [this]() { CompilePanel::Render(); },
        nullptr
    );

    // Register Performance panel (monitoring)
    _registerPanel("Performance",
        [this]() {
            m_performancePanel.Initialize(m_mainFont, m_boldFont, m_symbolsFont);
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
        // Set world.
        [this](ECS::World* w) { m_performancePanel.SetWorld(w); }
    );

    // Register Systems panel (shows registered C# and C++ systems)
    _registerPanel("Systems",
        [this]() {
            m_systemsPanel.Initialize(m_mainFont, m_boldFont, m_symbolsFont);
        },
        [this]() { 
            // Get SystemManager from engine
            ECS::SystemManager* systemManager = nullptr;
            if (Engine::CORE) {
                systemManager = &Engine::CORE->GetSystemManager();
            }
            m_systemsPanel.Render(systemManager);
        },
        // Set world.
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
    m_tilePalette.SetPaintModeChangedCallback([this](bool enabled) {
        if (!enabled) {
            m_sceneViewport.SetGridVisible(false);
        }
    });
    m_tilePalette.SetActiveTileMapCallback([this](const EntityId id) { _setActiveTileMap(id); });
    // Set active tileset callback.
    m_tilePalette.SetActiveTilesetCallback([this](const uint8_t index) {
        const auto it = m_tileMapCache.find(m_activeTileMapEntityId);
        if (it == m_tileMapCache.end()) {
            return;
        }

        TileMapCacheEntry& entry = it->second;
        entry.ActiveTilesetIndex = index; // Persist active tileset selection for this tilemap.

        if (index < entry.Tilesets.size()) {
            m_activeTileset = entry.Tilesets[index];
            m_activeTilesetPath = (index < entry.TilesetPaths.size()) ? entry.TilesetPaths[index] : std::string();
        }
    });

    // Set up hierarchy selection callback to sync with inspector and viewports
    m_hierarchyWindow.OnSelectionChanged([this](const EntityId id) {
        m_suppressViewportSelectionSync = true;

        if (!m_world) {
            m_inspector.ClearSelection();
            if (m_sceneViewport.HasValidWorld()) m_sceneViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            if (m_gameViewport.HasValidWorld()) m_gameViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            _syncTilePaletteToSelection(ECS::Entity::NPOS32); // Clear tile palette when no world.
            m_suppressViewportSelectionSync = false;
            return;
        } // Clear when no world

        const auto& selectedEntities = m_hierarchyWindow.GetSelectedEntities();
        m_inspector.SetSelectedEntities(selectedEntities);

        if (id == ECS::Entity::NPOS32) {
            m_inspector.ClearSelection();
            if (m_sceneViewport.HasValidWorld()) m_sceneViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            if (m_gameViewport.HasValidWorld()) m_gameViewport.SetSelectedEntity(ECS::Entity::NPOS32);
            _syncTilePaletteToSelection(ECS::Entity::NPOS32); // Clear tile palette when selection is empty.
            m_suppressViewportSelectionSync = false;
            return;
        } // Clear when no entity

        // Resolve the entity to the current generation before checking aliveness
        const ECS::Entity e = m_world->Resolve(id);
        if (m_world->IsAlive(e)) {
            m_inspector.InspectEntity(id); // Inspect when valid

			// If the inspector is already in entity mode, focus it to ensure it's visible
            if (m_inspector.GetMode() == InspectorPanel::InspectionMode::Entity) {
                m_inspector.RequestFocus();
            }

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
        m_suppressViewportSelectionSync = false;
        });
}

// Load editor fonts and icon sets
void LevelEditor::_loadFonts() {
    auto& io = ImGui::GetIO();
    float textFontSize = m_config.TextFontSize;

    // Helper to load font via RM
    auto loadFont = [&](const std::string& path, float size, ImFontConfig* config = nullptr, const ImWchar* ranges = nullptr) -> ImFont* {
        // WE'RE USING RAWDATA BECAUSE ENGINE VS IMGUI FONTS ARE DIFFERENT
        // IMGUI FONTS NEED RAW TTF BYTES
        auto raw = RM.Get<RawData>(path);
        
        // Load from memory if valid
        if (raw && raw->IsValid) {
            ImFontConfig cfg = config ? *config : ImFontConfig();
            cfg.FontDataOwnedByAtlas = false; // Data is owned by RM (shared_ptr)

            // Make a name for debug    
            strncpy_s(cfg.Name, path.c_str(), sizeof(cfg.Name) - 1);
            return io.Fonts->AddFontFromMemoryTTF(raw->Data.data(), (int)raw->Data.size(), size, &cfg, ranges);
        }

        LOG_WARNING("Failed to load font via RM, falling back to file: " << path);
        ImFontConfig cfg = config ? *config : ImFontConfig();
        strncpy_s(cfg.Name, path.c_str(), sizeof(cfg.Name) - 1);
        return io.Fonts->AddFontFromFileTTF(path.c_str(), size, &cfg, ranges);
    };

	// Load main text font (caching handled by RM)
    if (!m_mainFont && io.Fonts->Fonts.empty()) {
        m_mainFont = loadFont("assets/fonts/Inter/static/Inter_24pt-Medium.ttf", textFontSize);

		// Safety check (fallback to default font)
        if (!m_mainFont) {
            LOG_ERROR("Failed to load Open Sans Medium font");
            m_mainFont = io.Fonts->AddFontDefault();
        }
    }

	// Use first font if already loaded
    else if (!m_mainFont) {
        m_mainFont = io.Fonts->Fonts[0];
    }

	// Load bold font
    if (!m_boldFont && io.Fonts->Fonts.size() < 2) {
        m_boldFont = loadFont("assets/fonts/Inter/static/Inter_24pt-ExtraBold.ttf", textFontSize);

		// Safety check (fallback to default font)
        if (!m_boldFont) {
            LOG_ERROR("Failed to load Open Sans ExtraBold font");
            m_boldFont = io.Fonts->AddFontDefault();
        }
    }

	// Use second font if already loaded
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

    // Merge icon glyphs into the main font for tab labels and mixed text.
    static ImFontAtlas* s_mergedAtlas = nullptr;
    if (m_mainFont && s_mergedAtlas != io.Fonts) {
        ImFontConfig mergeConfig = iconsConfig;
        mergeConfig.MergeMode = true;
        loadFont(
            "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
            textFontSize,
            &mergeConfig,
            iconRanges
        );
        s_mergedAtlas = io.Fonts;
    }


	// Load icon font (Material Symbols); AGAIN, caching via RM
    if (!m_symbolsFont && io.Fonts->Fonts.size() < 3) {
        m_symbolsFont = loadFont(
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

    (void)io;
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

    m_undoSystem.Update();

    // Auto-sync to active scene world if it changed (e.g. via File > New Scene)
    if (Engine::CORE) {
        auto& sm = Engine::CORE->GetSceneManager();
        auto* active = sm.GetActive();
        ECS::World* activeWorld = active ? &active->GetWorld() : nullptr;
        if (activeWorld != m_world) {
            SetScene(active);
        }
    }

    m_playback.ProcessInput(); // Process playback hotkeys and actions
    m_sceneViewport.HandleInWorldInteraction(); // Process scene viewport interactions and camera input control
    m_gameViewport.HandleInWorldInteraction(); // Process game viewport interactions

    // Ensure the engine's RendererSystem uses the editor viewport camera
    // for the upcoming systems update. This must happen here because
    // systems are executed after Editor::Update() but before Editor::Render().
    if (Engine::CORE) {
        auto* rendererSystem = Engine::CORE->GetSystemManager().GetSystem<ECS::RendererSystem>();
        if (rendererSystem) {
            m_gameViewport.PrepareFrame();
            auto* editorCam = m_sceneViewport.GetEditorCamera();
            if (editorCam) {
                rendererSystem->SetCamera(editorCam->GetCamera());
            }
        }
    }

    // Update tilemap cache and push all visible tilemaps to the renderer.
    _refreshTileMapCache();

    // SceneManager::Update is already called in EditorService::Update.
}

// -------------------------------------------------------------------------
// Event Handlers
// -------------------------------------------------------------------------
// Process playback state changes from the toolbar.
void LevelEditor::_onPlaybackStateChanged(EditorState oldState, EditorState newState) {
    // Any editor-specific logic that needs to happen on state change goes here
    // (The time scale is already handled by Playback itself)
    LOG_INFO("Playback state changed from " << static_cast<int>(oldState) << " to " << static_cast<int>(newState));

	// When entering play mode, ensure any unsaved changes are saved to disk so the runtime scene serializer has 
    // the latest data
    if (newState == EditorState::Play) {
        _saveActiveTileMapAsset(m_fileMenu.GetCurrentScenePath());
    }

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

    if (newState == EditorState::Edit) {
        // Entity IDs are preserved during restore, so keep selection intact.
        // Rebuild entity order to reflect restored hierarchy.
        m_hierarchyWindow.RebuildEntityOrder();
    }

    if (newState == EditorState::Play && m_console.IsClearOnPlayBuildEnabled()) {
        m_console.Clear();
    }
}

// Process selection changes coming from viewports.
void LevelEditor::_onViewportSelectionChanged(const EntityId id) {
    if (m_suppressViewportSelectionSync) {
        return; // Ignore feedback loop when hierarchy drives selection.
    }

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

    // Sync multi-selection set to inspector
    m_inspector.SetSelectedEntities(m_hierarchyWindow.GetSelectedEntities());

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

// Process asset selection events from the asset browser.
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

    // Apply tileset to selected/active tilemap entity, otherwise prompt to create one.
    ECS::Entity target = ECS::NULL_ENTITY;
    const EntityId selectedId = m_hierarchyWindow.GetPrimarySelectedEntity();
    if (selectedId != ECS::Entity::NPOS32) {
        target = m_world->Resolve(selectedId);
    }

    if (m_world->IsAlive(target) && Editor::ECSUtils::HasComponent(m_world, target, kTileMapComponentName)) {
        _applyTilesetToTilemap(target, assetPath);
        return;
    }

    // If the palette already has an active tilemap, prefer that before prompting.
    if (m_activeTileMapEntityId != ECS::Entity::NPOS32) {
        ECS::Entity activeEntity = m_world->Resolve(m_activeTileMapEntityId);
        if (m_world->IsAlive(activeEntity) && Editor::ECSUtils::HasComponent(m_world, activeEntity, kTileMapComponentName)) {
            _applyTilesetToTilemap(activeEntity, assetPath);
            return;
        }
    }

    // If no tilemap is selected, reuse the first tilemap in the cache or scene.
    if (!m_tileMapCache.empty()) {
        for (const auto& [id, entry] : m_tileMapCache) {
            (void)entry; // Cache entry is unused; we only need the entity id.
            ECS::Entity cachedEntity = m_world->Resolve(id);
            if (m_world->IsAlive(cachedEntity) && Editor::ECSUtils::HasComponent(m_world, cachedEntity, kTileMapComponentName)) {
                _applyTilesetToTilemap(cachedEntity, assetPath);
                return;
            }
        }
    }

    // Final fallback: scan the world for any tilemap component.
    ECS::Entity fallback = ECS::NULL_ENTITY;
    ForEachTileMapComponent(m_world, [&fallback](const ECS::Entity entity, ECS::Components::TileMapComponent&) {
        if (fallback.IsNull()) {
            fallback = entity;
        }
    });

    if (m_world->IsAlive(fallback)) {
        _applyTilesetToTilemap(fallback, assetPath);
        return;
    }

    // Auto-create a tilemap if none exists.
    const EntityId newId = m_entityActions.AddEntity("Tilemap", ECS::Entity::NPOS32);

	// Add TileMapComponent to the new entity
    ECS::Entity targetEntity = m_world->Resolve(newId);

	// Initialize TileMapComponent with default values
    if (m_world->IsAlive(targetEntity)) {
        _applyTilesetToTilemap(targetEntity, assetPath);
        m_hierarchyWindow.SetSelectedEntity(targetEntity.Index);
        m_inspector.InspectEntity(targetEntity.Index);
    }
	// Sync viewports to new selection
    if (m_sceneViewport.HasValidWorld()) {
        m_sceneViewport.SetSelectedEntity(newId);
    }
    if (m_gameViewport.HasValidWorld()) {
        m_gameViewport.SetSelectedEntity(newId);
    }
}

// Sync the tile palette to the active selection.
void LevelEditor::_syncTilePaletteToSelection(const EntityId id) {
    if (!m_world) {
        return; // Nothing to sync without a world.
    }

    if (id == ECS::Entity::NPOS32) {
        // Keep the current tile palette context when selection is cleared.
        return;
    }

    ECS::Entity entity = m_world->Resolve(id); // Resolve entity to current generation.
    if (!m_world->IsAlive(entity)) {
        return; // Ignore dead entities.
    }

    if (!Editor::ECSUtils::HasComponent(m_world, entity, kTileMapComponentName)) {
        // Keep the current tile palette context when selecting non-tilemap entities.
        return;
    }

    _setActiveTileMap(id); // Activate the tilemap for the palette without hiding others.
}

// Refresh cached tilemap data after edits.
void LevelEditor::_refreshTileMapCache() {
    if (!m_world) {
        return; // No world available to refresh tilemaps.
    }

    const std::string scenePath = m_fileMenu.GetCurrentScenePath(); // Use saved scene path for tilemap derivation.
    std::unordered_set<EntityId> seen; // Track which entities still have tilemaps.
    m_tileMapList.clear(); // Rebuild the list of tilemaps for the palette dropdown.

    // Update tilemap panels when tilemap components change.
    ForEachTileMapComponent(m_world, [this, &seen, &scenePath](const ECS::Entity entity, ECS::Components::TileMapComponent& comp) {
        seen.insert(entity.Index); // Mark this tilemap entity as active.

        std::string mapPath = ECS::StringTable::Resolve(comp.TileMapPath); // Resolve map path from StringId
        std::string legacyTilesetPath = ECS::StringTable::Resolve(comp.TilesetTexturePath); // Legacy single tileset path

        // If we have a legacy tileset path and the project paths system is ready
        if (!legacyTilesetPath.empty() && Engine::ProjectPaths::IsInitialized()) {
            std::filesystem::path legacyPathFs(legacyTilesetPath);

            // Only process if the path is absolute (e.g. C:/project/tileset.png)
            // Relative paths are already fine, no need to convert
            if (legacyPathFs.is_absolute()) {

                // Convert absolute path to relative (e.g. assets/tileset.png)
                const std::string relative = Engine::ProjectPaths::ToRelativePath(legacyTilesetPath);

                // Only update if conversion succeeded (non-empty result)
                if (!relative.empty()) {
                    legacyTilesetPath = relative;
                    // Store the relative path in the component's interned string table
                    comp.TilesetTexturePath = ECS::StringTable::Intern(legacyTilesetPath);
                }
            }
        }

        if (mapPath.empty() && !scenePath.empty()) {
            // Derive a tilemap path from the saved scene path when none exists.
            std::filesystem::path derived = scenePath;
            derived.replace_extension(".tilemap");
            mapPath = derived.string();
            comp.TileMapPath = ECS::StringTable::Intern(mapPath);
        }

        const std::string mapPathOnDisk = ResolveProjectPathForLoad(mapPath);

        glm::vec2 origin(0.0f, 0.0f);
        if (auto* transform = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(m_world, entity, "LocalTransform")) {
            origin = glm::vec2(transform->Position.X, transform->Position.Y); // Use entity position as tilemap origin.
        }

        std::string displayName = "Tilemap " + std::to_string(entity.Index);
        if (const auto* name = Editor::ECSUtils::GetComponentPtr<ECS::Components::Name>(m_world, entity, "Name")) {
            displayName = ECS::StringTable::Resolve(name->Value); // Prefer the entity name when available.
        }

        m_tileMapList.push_back({ entity.Index, displayName });

        TileMapCacheEntry& entry = m_tileMapCache[entity.Index];
        const bool generationChanged = (entry.Generation != entity.Generation);
        if (generationChanged) {
            // Scene reload can reuse entity indices; reset cache when generation changes.
            entry.Map.reset();
            entry.Tilesets.clear();
            entry.TilesetPaths.clear();
            entry.MapPath.clear();
        }
        entry.Generation = entity.Generation;
        const bool missingTilesetList = (!mapPathOnDisk.empty() &&
            entry.Map &&
            entry.MapPath == mapPathOnDisk &&
            entry.Map->GetTilesetPaths().empty() &&
            !legacyTilesetPath.empty() &&
            // Skip entries that already exist.
            std::filesystem::exists(mapPathOnDisk));
        const bool mapNeedsReload = generationChanged ||
            (!entry.Map) ||
            (!mapPathOnDisk.empty() && entry.MapPath != mapPathOnDisk) ||
            missingTilesetList ||
            entry.TileWorldSize != comp.TileWorldSize ||
            entry.DefaultWidth != comp.DefaultWidth ||
            entry.DefaultHeight != comp.DefaultHeight;

        if (mapNeedsReload) {
            entry.Map = mapPathOnDisk.empty() ? nullptr : LoadOrCreateTileMap(mapPathOnDisk, comp.TileWorldSize, comp.DefaultWidth, comp.DefaultHeight);
            entry.MapPath = mapPathOnDisk;
            entry.TileWorldSize = comp.TileWorldSize;
            entry.DefaultWidth = comp.DefaultWidth;
            entry.DefaultHeight = comp.DefaultHeight;
            if (entry.Map) {
                LOG_INFO("[TileMap] Cache reload entity " << entity.Index
                    << " tilesets=" << entry.Map->GetTilesetPaths().size()
                    << " layers=" << entry.Map->LayerCount());
            }
        // Skip empty entries.
        } else if (mapPathOnDisk.empty() && !entry.MapPath.empty()) {
            // Drop stale map paths when the component no longer points to a file.
            entry.MapPath.clear();
        }

        if (!entry.Map) {
            // Keep an in-memory tilemap when no path is assigned yet.
            entry.Map = std::make_shared<TileMap>(comp.TileWorldSize);
            entry.Map->AddLayer(comp.DefaultWidth, comp.DefaultHeight);
            entry.MapPath.clear();
            entry.TileWorldSize = comp.TileWorldSize;
            entry.DefaultWidth = comp.DefaultWidth;
            entry.DefaultHeight = comp.DefaultHeight;
            LOG_INFO("[TileMap] Created in-memory tilemap for entity " << entity.Index);
        }

        bool addedLegacyTileset = false;
        if (entry.Map && !legacyTilesetPath.empty()) {
            // Lambda that checks if a tileset path already exists in the map
            // (handles both raw paths and resolved/normalized paths)
            auto hasLegacyTileset = [&entry](const std::string& path) {
                // If no path given or map doesn't exist, obviously can't have it
                if (path.empty() || !entry.Map) {
                    return false;
                }
                // Fast path: check if the path exists directly in the map's tileset list
                if (entry.Map->FindTilesetPath(path) >= 0) {
                    return true;
                }
                // Slow path: the path might be stored differently (e.g. absolute vs relative)
                // so resolve both sides and compare normalized versions
                const std::string resolved = ResolveProjectPathForLoad(path);
                for (const auto& existing : entry.Map->GetTilesetPaths()) {
                    if (ResolveProjectPathForLoad(existing) == resolved) {
                        return true;
                    }
                }
                // Not found either way
                return false;
            };

            // Ensure the legacy tileset path exists in the map's tileset list.
            if (!hasLegacyTileset(legacyTilesetPath)) {
                entry.Map->AddTilesetPath(legacyTilesetPath);
                addedLegacyTileset = true;
            }
        }

		// Normalize tileset paths to be relative to the project when possible and remove invalid paths
        std::vector<std::string> mapTilesetPaths = entry.Map ? entry.Map->GetTilesetPaths() : std::vector<std::string>{};
        std::vector<std::string> normalizedTilesetPaths;
        normalizedTilesetPaths.reserve(mapTilesetPaths.size());

		// First pass: normalize paths but keep all paths for now (including invalid ones) to avoid dropping tilesets on load before validation
        for (const auto& path : mapTilesetPaths) {
            if (path.empty()) {
                continue;
            }
            std::string normalized = path;

			// Only attempt normalization if project paths are initialized, otherwise we may be in a context where relative paths aren't valid 
            // (e.g. loading a tilemap before the project is fully loaded)
            if (Engine::ProjectPaths::IsInitialized()) {
                std::filesystem::path fsPath(path);
				// Only convert to relative if the path is absolute, otherwise assume it's already relative to avoid messing with non-project paths 
                // (e.g. absolute paths or special schemes like "rm://")
                if (fsPath.is_absolute()) {
                    const std::string relative = Engine::ProjectPaths::ToRelativePath(path);
                    if (!relative.empty()) {
                        normalized = relative;
                    }
                }
            }
			// Keep the original path if normalization fails or isn't applicable
            normalizedTilesetPaths.push_back(normalized);
        }

		// Second pass: validate normalized paths and filter out any that don't exist on disk, but keep the legacy tileset path if it exists 
        // to avoid losing tilesets on load before the user can fix them
        std::vector<std::string> validTilesetPaths;
        validTilesetPaths.reserve(normalizedTilesetPaths.size());

		// Check if the normalized paths exist on disk and keep them if they do
        for (const auto& path : normalizedTilesetPaths) {
            const std::string resolvedPath = ResolveProjectPathForLoad(path);
            if (std::filesystem::exists(resolvedPath)) {
                validTilesetPaths.push_back(path);
            }
        }

		// If no valid tileset paths remain but a legacy tileset path exists, keep the legacy path 
        if (validTilesetPaths.empty() && !legacyTilesetPath.empty()) {
            validTilesetPaths.push_back(legacyTilesetPath);
        }

		// Update the map's tileset paths if they differ from the validated list, which may include normalization changes, filtering out invalid paths, 
        // or adding back a legacy path if needed
        if (entry.Map && validTilesetPaths != mapTilesetPaths) {
            entry.Map->SetTilesetPaths(validTilesetPaths);
            if (!entry.MapPath.empty()) {
                entry.Map->SaveMap(entry.MapPath);
                LOG_INFO("[TileMap] Normalized tileset list for " << entry.MapPath);
            }
			// Refresh the cache from the map after normalization
            mapTilesetPaths = entry.Map->GetTilesetPaths();
        } 
		// Update the cache with the validated tileset paths even if no changes were made
        else {
            mapTilesetPaths = validTilesetPaths;
        }

		// If the legacy tileset path is still present after normalization and validation, keep it in sync with the component for 
        // backwards compatibility with scenes that reference it
        if (legacyTilesetPath.empty() && !mapTilesetPaths.empty()) {
            // Keep the legacy component field in sync so scenes retain a usable tileset path
            comp.TilesetTexturePath = ECS::StringTable::Intern(mapTilesetPaths.front());
        }
		// Determine if the tileset list changed based on the component and cache state, which may require rebuilding the tileset textures for rendering
        // We compare against the original map paths before normalization to avoid unnecessary rebuilds during path normalization, but we also consider changes 
        // in tile pixel size or an empty tileset list when the map has tilesets as changes that require a rebuild
        const bool tilesetListChanged = entry.TilePixelSize != comp.TilePixelSize || entry.TilesetPaths != mapTilesetPaths || (entry.Tilesets.empty() && !mapTilesetPaths.empty());

		// Rebuild the tileset textures
        if (tilesetListChanged) {
            entry.Tilesets.clear();
            entry.TilesetPaths = mapTilesetPaths;
            entry.Tilesets.reserve(mapTilesetPaths.size());

			// Build new tilesets from the validated and normalized paths, which may include the legacy path if it was kept for compatibility
            for (const auto& tilesetPath : mapTilesetPaths) {
                const std::string resolvedPath = ResolveProjectPathForLoad(tilesetPath);
                entry.Tilesets.push_back(BuildTilesetFromTexture(resolvedPath, comp.TilePixelSize));
            }
			// Cache the tile pixel size to detect changes that require tileset rebuilds
            entry.TilePixelSize = comp.TilePixelSize;
            if (!entry.Tilesets.empty()) {
                entry.ActiveTilesetIndex = std::min(entry.ActiveTilesetIndex, static_cast<uint8_t>(entry.Tilesets.size() - 1));
            } 
            else {
                entry.ActiveTilesetIndex = 0;
            }
        }

        entry.Origin = origin; // Update origin every frame to follow entity transforms.
        entry.Visible = comp.Visible && m_world->IsActiveInHierarchy(entity); // Match runtime visibility gating
        entry.DisplayName = displayName;

        if (addedLegacyTileset && !entry.MapPath.empty()) {
            // Persist legacy tileset migration so future reloads include the tileset list.
            entry.Map->SaveMap(entry.MapPath);
            LOG_INFO("[TileMap] Persisted legacy tileset to " << entry.MapPath);
        }
    });

    for (auto it = m_tileMapCache.begin(); it != m_tileMapCache.end(); ) {
        if (!seen.contains(it->first)) {
            it = m_tileMapCache.erase(it); // Drop cache entries for deleted tilemaps.
        } else {
            ++it;
        }
    }

    // Auto-select the only tilemap if none is active yet.
    if (m_activeTileMapEntityId == ECS::Entity::NPOS32 && m_tileMapList.size() == 1) {
        _setActiveTileMap(m_tileMapList[0].Id);
    }

    // Update palette dropdown list and active selection.
    m_tilePalette.SetTileMapList(m_tileMapList, m_activeTileMapEntityId);

    if (m_activeTileMapEntityId != ECS::Entity::NPOS32 &&
        // Skip duplicate entries.
        !m_tileMapCache.contains(m_activeTileMapEntityId)) {
        // Clear active state if the tilemap entity was removed.
        m_activeTileMapEntityId = ECS::Entity::NPOS32;
        m_activeTileMap.reset();
        m_activeTileset.reset();
        m_activeTileMapPath.clear();
        m_activeTilesetPath.clear();
        const std::vector<std::shared_ptr<Tileset>> emptyTilesets;
        const std::vector<std::string> emptyPaths;
        m_tilePalette.SetEditingContext(nullptr, emptyTilesets, emptyPaths, 0, std::string(), glm::vec2(0.0f, 0.0f));
    // Select the appropriate execution path.
    } else if (m_activeTileMapEntityId != ECS::Entity::NPOS32) {
        const auto it = m_tileMapCache.find(m_activeTileMapEntityId);
        if (it != m_tileMapCache.end()) {
            const TileMapCacheEntry& entry = it->second;

            if (entry.Map) {
                const uint8_t activeTilesetIndex = entry.Tilesets.empty() ? 0 : std::min(entry.ActiveTilesetIndex, static_cast<uint8_t>(entry.Tilesets.size() - 1));
                const std::shared_ptr<Tileset> activeTileset = entry.Tilesets.empty() ? nullptr : entry.Tilesets[activeTilesetIndex];

                if (m_activeTileMap != entry.Map || m_activeTileset != activeTileset) {
                    // Refresh the palette context if the cached assets changed.
                    m_activeTileMap = entry.Map;
                    m_activeTileset = activeTileset;
                    m_activeTileMapPath = entry.MapPath;
                    m_activeTilesetPath = (activeTilesetIndex < entry.TilesetPaths.size()) ? entry.TilesetPaths[activeTilesetIndex] : std::string();
                    m_tilePalette.SetEditingContext(m_activeTileMap, entry.Tilesets, entry.TilesetPaths, activeTilesetIndex, m_activeTileMapPath, entry.Origin);
                } else {
                    // Keep selection state but update origin as the entity moves.
                    m_tilePalette.SetTileMapOrigin(entry.Origin);

					// Update the active tileset path if it changed externally (e.g. via another tilemap entry or direct map edits) 
                    // to keep them in sync
                    if (m_activeTileMapPath != entry.MapPath) {
                        m_activeTileMapPath = entry.MapPath;
                        m_tilePalette.SetTileMapPath(m_activeTileMapPath);
                    }
                }
            }
        }
    }

    if (auto* renderer = ECS::RendererSystem::GetInstance()) {
        std::vector<ECS::RendererSystem::DebugTileMapEntry> debugMaps;
        debugMaps.reserve(m_tileMapCache.size());

    // const bool hasMultiple = (m_tileMapCache.size() > 1);
    for (auto& [id, entry] : m_tileMapCache) {
        if (!entry.Visible || !entry.Map || entry.Tilesets.empty()) {
            continue; // Skip hidden or incomplete tilemaps.
        }

        std::vector<std::shared_ptr<const Tileset>> tilesets;
        tilesets.reserve(entry.Tilesets.size());
        for (const auto& tileset : entry.Tilesets) {
            tilesets.push_back(tileset);
        }
        debugMaps.push_back({ *entry.Map, tilesets, entry.Origin, m_world->Resolve(id) });
    }

        renderer->SetDebugTileMaps(debugMaps);
    }
}

// Apply the selected tileset to the active tilemap.
void LevelEditor::_applyTilesetToTilemap(ECS::Entity entity, const std::string& assetPath) {
    if (!m_world || !m_world->IsAlive(entity)) {
        return;
    }

    ECS::Components::TileMapComponent comp{};
    if (auto* compPtr = Editor::ECSUtils::GetComponentPtr<ECS::Components::TileMapComponent>(m_world, entity, kTileMapComponentName)) {
        comp = *compPtr;
    }

    comp.TilesetTexturePath = ECS::StringTable::Intern(assetPath);

    Editor::ECSUtils::SetComponent(m_world, entity, kTileMapComponentName, comp);
    LOG_INFO("[TileMap] Apply tileset \"" << assetPath << "\" to entity " << entity.Index);

    // Ensure the tilemap cache entry exists so we can add the tileset path.
    TileMapCacheEntry& entry = m_tileMapCache[entity.Index];
    if (!entry.Map) {
        entry.Map = std::make_shared<TileMap>(comp.TileWorldSize);
        entry.Map->AddLayer(comp.DefaultWidth, comp.DefaultHeight);
    }

    if (entry.MapPath.empty()) {
        // Prefer the component path if it already exists so we can persist tileset changes.
        const std::string mapPath = ECS::StringTable::Resolve(comp.TileMapPath);
        if (!mapPath.empty()) {
            entry.MapPath = mapPath;
        }
    }

    const uint8_t tilesetIndex = entry.Map->AddTilesetPath(assetPath); // Add tileset path to the map list.
    entry.TilesetPaths = entry.Map->GetTilesetPaths();
    entry.Tilesets.clear();
    entry.Tilesets.reserve(entry.TilesetPaths.size());
    for (const auto& tilesetPath : entry.TilesetPaths) {
        entry.Tilesets.push_back(BuildTilesetFromTexture(tilesetPath, comp.TilePixelSize));
    }
    entry.TilePixelSize = comp.TilePixelSize;
    entry.ActiveTilesetIndex = tilesetIndex; // Switch active tileset to the newly added one.

    if (!entry.MapPath.empty()) {
        // Save immediately so tileset paths are available on the next reload.
        entry.Map->SaveMap(entry.MapPath);
        LOG_INFO("[TileMap] Saved tilemap after tileset add: " << entry.MapPath);
    }

    _setActiveTileMap(entity.Index);
}

// Set the active tilemap from the selector.
void LevelEditor::_setActiveTileMap(EntityId id) {
    if (!m_world || id == ECS::Entity::NPOS32) {
        return;
    }

    ECS::Entity entity = m_world->Resolve(id);
    if (!m_world->IsAlive(entity) || !Editor::ECSUtils::HasComponent(m_world, entity, kTileMapComponentName)) {
        return;
    }

    m_activeTileMapEntityId = id;

    const auto it = m_tileMapCache.find(id);
    if (it != m_tileMapCache.end() && it->second.Map) {
        const TileMapCacheEntry& entry = it->second;
        const uint8_t activeTilesetIndex = entry.Tilesets.empty() ? 0 : std::min(entry.ActiveTilesetIndex, static_cast<uint8_t>(entry.Tilesets.size() - 1));
        
        // Grab a shared_ptr to the active tileset
        // This ensures the tileset remains alive even if the editor replaces it (e.g. via drag-and-drop) 
        // before the renderer finishes the current frame
        const std::shared_ptr<Tileset> activeTileset = entry.Tilesets.empty() ? nullptr : entry.Tilesets[activeTilesetIndex];
        
        m_activeTileMap = entry.Map;
        m_activeTileset = activeTileset;
        m_activeTileMapPath = entry.MapPath;
        m_activeTilesetPath = (activeTilesetIndex < entry.TilesetPaths.size()) ? entry.TilesetPaths[activeTilesetIndex] : std::string();
        m_tilePalette.SetEditingContext(m_activeTileMap, entry.Tilesets, entry.TilesetPaths, activeTilesetIndex, m_activeTileMapPath, entry.Origin);

        // Sync collision grid to BoidSystem on tilemap switch
        if (auto* boidSystem = Engine::CORE->GetSystemManager().GetSystem<ECS::BoidSystem>()) {
            boidSystem->UpdateCollisionGrid(*it->second.Map);
        }

        return;
    }

    // Cache is expected to populate via _refreshTileMapCache.
}

// Save the active tilemap asset to disk.
void LevelEditor::_saveActiveTileMapAsset(const std::string& scenePath) {
    if (m_tileMapCache.empty()) {
        return; // No cached tilemaps to save.
    }

    const bool hasMultiple = (m_tileMapCache.size() > 1);
    for (auto& [id, entry] : m_tileMapCache) {
        if (!entry.Map) {
            continue; // Skip missing tilemaps.
        }

        ECS::Entity entity = m_world->Resolve(id); // Resolve entity for component reads.
        std::string legacyTilesetPath;
        if (m_world->IsAlive(entity)) {
			// Read the legacy tileset path from the component for backwards compatibility with scenes that reference it, 
            // but only if the cache doesn't already have tileset paths from the map file
            if (const auto* comp = Editor::ECSUtils::GetComponentPtr<ECS::Components::TileMapComponent>(m_world, entity, kTileMapComponentName)) {
                legacyTilesetPath = ECS::StringTable::Resolve(comp->TilesetTexturePath); // Legacy fallback for tileset list.
            }
        }

        std::string mapPathString = entry.MapPath;
        if (mapPathString.empty() && !scenePath.empty()) {
            // Derive the tilemap path from the scene path when saving for the first time.
            std::filesystem::path derived = scenePath;
            if (hasMultiple) {
                derived.replace_extension("");
                derived += "_tilemap_" + std::to_string(id);
                derived.replace_extension(".tilemap");
            } else {
                derived.replace_extension(".tilemap");
            }
            mapPathString = derived.string();

			// Persist the derived path into the component so it's available for future edits and saves
            if (m_world->IsAlive(entity)) {
				// Update the component with the derived path so it's saved into the scene and available on reload without needing to rely on the cache
                if (auto* compPtr = Editor::ECSUtils::GetComponentPtr<ECS::Components::TileMapComponent>(m_world, entity, kTileMapComponentName)) {
                    auto comp = *compPtr;
                    comp.TileMapPath = ECS::StringTable::Intern(mapPathString);
                    if (comp.TilesetTexturePath == 0 && !entry.Map->GetTilesetPaths().empty()) {
                        comp.TilesetTexturePath = ECS::StringTable::Intern(entry.Map->GetTilesetPaths().front());
                    }
                    Editor::ECSUtils::SetComponent(m_world, entity, kTileMapComponentName, comp);
                }
            }
            // Cache the derived path for future saves
            entry.MapPath = mapPathString; 
        }

        if (mapPathString.empty()) {
            continue; // Still no path to save to.
        }

        LOG_INFO("[TileMap] Save pre-scene: entity " << id
            << " mapPath=\"" << mapPathString
            << "\" tilesets=" << entry.Map->GetTilesetPaths().size());

        if (!legacyTilesetPath.empty() && entry.Map->GetTilesetPaths().empty()) {
            // Ensure legacy tileset paths are written into the tilemap before save.
            entry.Map->AddTilesetPath(legacyTilesetPath);
        }

		// Persist the tileset path into the component for backwards compatibility with scenes that reference it, 
        // but only if the cache doesn't already have tileset paths from the map file
        if (m_world->IsAlive(entity)) {
            if (auto* compPtr = Editor::ECSUtils::GetComponentPtr<ECS::Components::TileMapComponent>(m_world, entity, kTileMapComponentName)) {
                if (compPtr->TilesetTexturePath == 0 && !entry.Map->GetTilesetPaths().empty()) {
                    auto comp = *compPtr;
                    // Persist a tileset path into the scene so reloads can rebuild tilesets
                    comp.TilesetTexturePath = ECS::StringTable::Intern(entry.Map->GetTilesetPaths().front());
                    Editor::ECSUtils::SetComponent(m_world, entity, kTileMapComponentName, comp);
                }
            }
        }

        const std::filesystem::path mapPath(mapPathString); // Convert to path for directory handling.
        const std::filesystem::path mapDir = mapPath.parent_path(); // Save directory so we can create it if missing.

        if (!mapDir.empty() && !std::filesystem::exists(mapDir)) {
            // Create output directories if missing.
            std::filesystem::create_directories(mapDir); // Ensure the tilemap folder exists before writing.
        }

        if (!entry.Map->SaveMap(mapPathString)) {
            LOG_WARNING("[TileMap] Failed to save tilemap before scene save: " << mapPathString);
        } else {
            LOG_INFO("[TileMap] Saved tilemap before scene save: " << mapPathString);
        }
    }
}

// -------------------------------------------------------------------------
// Render
// -------------------------------------------------------------------------
// Render dock space and editor panels with a fallback when world is missing
void LevelEditor::Render() {
    if (m_gameViewport.IsImmersiveModeEnabled()) {
        m_gameViewport.ShowEditorWindows();
        return;
    }

    if (ImGui::BeginMainMenuBar()) {
        m_fileMenu.RenderFileMenu();
        m_fileMenu.RenderEditMenu();
        m_fileMenu.RenderViewMenu(m_uiScale);
        // End main menu bar.
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
        // Render disabled text.
        ImGui::TextDisabled("No scene attached"); // Inform user about missing scene
        ImGui::PopFont();
        // End.
        ImGui::End();
    }

	// Set focus to tile palette if requested (e.g. after selecting a tilemap entity)
    if (m_focusTilePaletteNextFrame) {
        ImGui::SetWindowFocus("Tile Palette");
        m_focusTilePaletteNextFrame = false;
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
    m_tileMapCache.clear(); // Drop cached tilemaps when changing scenes.
    m_tileMapList.clear();  // Drop tilemap list for the new scene.
    m_tilePalette.ClearTilePreviewSizeCache(); // Reset per-scene tile preview size cache.
    const std::vector<std::shared_ptr<Tileset>> emptyTilesets;
    const std::vector<std::string> emptyPaths;
    m_tilePalette.SetEditingContext(nullptr, emptyTilesets, emptyPaths, 0, std::string(), glm::vec2(0.0f, 0.0f));

    if (auto* renderer = ECS::RendererSystem::GetInstance()) {
        renderer->ClearDebugTileMaps(); // Remove any previous debug tilemaps.
        renderer->ClearDebugTileMap();  // Clear the legacy single debug map as well.
    }
}

// Set the active Scenes::Scene so EntityActions has the scene pointer
void LevelEditor::SetScene(Scenes::Scene* scene) {
    m_entityActions.SetScene(scene);
    if (scene) {
        m_fileMenu.SyncActiveScenePath(scene->GetPath());
    }
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

// Check whether step requested.
bool LevelEditor::IsStepRequested() const {
    return m_playback.IsStepRequested();
}

// Clear step request.
void LevelEditor::ClearStepRequest() {
    m_playback.ClearStepRequest();
}

// Return editor state.
EditorState LevelEditor::GetEditorState() const {
    return m_playback.GetEditorState();
}
