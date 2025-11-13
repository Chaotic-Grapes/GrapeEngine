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

// Create the editor and initialize panel members and config
LevelEditor::LevelEditor(ECS::World* world, const LevelEditorConfig& config)
    : m_world(world), m_config(config), m_playback(world), m_symbolsFont(nullptr),
    m_mainFont(nullptr), m_boldFont(nullptr), m_assetBrowser(), m_editorCore(),
    m_hierarchyWindow(), m_inspector() {
// Defer panel initialization to Initialize to use loaded fonts
}

// Destroy the editor instance without owning the world
LevelEditor::~LevelEditor() {}

// Build the dock layout once using split ratios and target windows
void LevelEditor::_buildDockLayout() {
    // Rebuild only when flagged to avoid resetting panel positions
    if (m_dockLayoutBuilt) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp->Size.x <= 0 || vp->Size.y <= 0) return; // Guard against zero size

    ImGui::DockBuilderRemoveNode(m_dockspaceId);
    ImGui::DockBuilderAddNode(m_dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    // Flags: DockSpace creates a root docking container; PassthruCentralNode lets the central area render game content underneath
    ImGui::DockBuilderSetNodeSize(m_dockspaceId, vp->Size); // Match viewport size
    // Target layout: left hierarchy, center viewport with controls on top,
    // bottom asset browser; right strip for inspector/prefab editors.
    // Target layout: left hierarchy, center viewport with controls on top,
    // bottom asset browser; right strip for inspector/prefab editors.

    ImGuiID leftCenterNode, rightNode;
    // Split root: carve right strip (25% width) for inspectors
    // Params: (source_node, direction, size_ratio, out_id_primary, out_id_remaining)
    ImGui::DockBuilderSplitNode(m_dockspaceId, ImGuiDir_Right, 0.25f, &rightNode, &leftCenterNode); // Reserve right strip

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
m_dockLayoutBuilt = true; // Mark layout as built
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

    ImGui::SetNextWindowPos(safePos);  // Position dock host under main menu bar
    ImGui::SetNextWindowSize(safeSize); // Size dock host to fill viewport
    ImGui::SetNextWindowViewport(vp->ID); // Pin host to current viewport

    // Host window flags: prevent interactions and visuals on the host container
    // NoDocking: disallow docking into this window
    // NoTitleBar/NoResize/NoMove: make it a static, decoration-less host
    // NoBringToFrontOnFocus/NoNavFocus: keep focus behavior stable
    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);   // Square corners for host
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); // No border around host
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)); // No padding; dockspace fills fully
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent host background

    ImGui::Begin("MainDockSpaceHost", nullptr, hostFlags); // Begin invisible host window
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    m_dockspaceId = ImGui::GetID("MainDockSpace"); // Stable ID for this dockspace
    ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode; // Let central node pass content
    ImGui::DockSpace(m_dockspaceId, ImVec2(0.0f, 0.0f), dockFlags); // Create dockspace filling the host window

    _buildDockLayout(); // Build once on first frame or after resize
    ImGui::End(); // End host window
}

// Initialize fonts build atlas and set up editor panels and hooks
void LevelEditor::Initialize(GLFWwindow* pWin) {
    if (!pWin) return;

    auto& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ChildBorderSize = 0.75f; // Subtle child border for visual separation

    float textFontSize = m_config.FontSize;

    // Only load fonts if not already loaded
    if (!m_mainFont && io.Fonts->Fonts.empty()) {
        m_mainFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/Inter/static/Inter_24pt-Medium.ttf",
            textFontSize
        );
        if (!m_mainFont) {
            LOG_ERROR("Failed to load Inter Medium font");
            m_mainFont = io.Fonts->AddFontDefault(); // Fallback to default font
        }
    }
    else if (!m_mainFont) {
        m_mainFont = io.Fonts->Fonts[0]; // Reuse first font in atlas
    }

    if (!m_boldFont && io.Fonts->Fonts.size() < 2) {
        m_boldFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/Inter/static/Inter_24pt-ExtraBold.ttf",
            textFontSize
        );
        if (!m_boldFont) {
            LOG_ERROR("Failed to load Inter ExtraBold font");
            m_boldFont = io.Fonts->AddFontDefault(); // Fallback to default font
        }
    }
    else if (!m_boldFont) {
        m_boldFont = io.Fonts->Fonts[1]; // Reuse second font in atlas
    }

    static const ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };
    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = false;
    iconsConfig.PixelSnapH = true;
    iconsConfig.GlyphMinAdvanceX = 24.0f;
    iconsConfig.GlyphOffset = ImVec2(0, 0);

    float iconFontSize = 18.0f;
    if (!m_symbolsFont && io.Fonts->Fonts.size() < 3) {
        m_symbolsFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
            iconFontSize,
            &iconsConfig,
            iconRanges
        );
        if (!m_symbolsFont) {
            LOG_ERROR("Failed to load Material Symbols font");
            m_symbolsFont = io.Fonts->AddFontDefault(); // Fallback to default font
        }
    }
    else if (!m_symbolsFont) {
        m_symbolsFont = io.Fonts->Fonts[2]; // Reuse third font in atlas
    }

    // Build font atlas if we added new fonts
    if (io.Fonts->Fonts.size() > 0 && !io.Fonts->IsBuilt()) {
        io.Fonts->Build(); // Prepare font textures for rendering
    }

    m_playback.Initialize(m_mainFont, m_symbolsFont); // Set up playback panel
    m_assetBrowser.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world); // Set up asset browser
    m_editorCore.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world); // Set up core editor
    m_hierarchyWindow.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world, &m_editorCore); // Set up hierarchy
    m_inspector.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world); // Set up inspector

    m_assetBrowser.SetInspector(&m_inspector); // Link inspector for asset previews

    m_hierarchyWindow.OnSelectionChanged([this](EntityId id) {
        if (!m_world) { m_inspector.ClearSelection(); return; } // Clear when no world
        if (id == ECS::Entity::NPOS32) { m_inspector.ClearSelection(); return; } // Clear when no entity
        // Resolve the entity to the current generation before checking aliveness
        ECS::Entity e = m_world->Resolve(id);
        if (m_world->IsAlive(e)) m_inspector.InspectEntity(id); // Inspect when valid
        else m_inspector.ClearSelection(); // Clear when entity is dead
        });
}

// Process input and in world interactions for editor panels
void LevelEditor::Update() {
    // Auto-sync to active scene world if it changed (e.g., via File > New Scene)
    if (Engine::CORE) {
        auto& sm = Engine::CORE->GetSceneManager();
        auto* active = sm.GetActive();
        ECS::World* activeWorld = active ? &active->GetWorld() : nullptr;
        if (activeWorld != m_world) {
            SetWorld(activeWorld);
        }
    }

    m_playback.ProcessInput(); // Handle playback hotkeys and actions

    // Handle playback state transitions (no camera toggling)
    {
        auto current = m_playback.GetGameState();
        if (current != m_lastGameState) {
            if (current == Playback::GameState::Stopped) {
                // Seed conversion tracking sets to avoid double pixel->world conversion
                // on restored entities created during play.
                if (m_world) {
                    m_world->Each<ECS::Components::LocalTransform>([&](ECS::Entity e, ECS::Components::LocalTransform& /*tr*/) {
                        m_convertedPositions.insert(e.Index);
                    });
                    m_world->Each<ECS::Components::ShapeCircle2D>([&](ECS::Entity e, ECS::Components::ShapeCircle2D& /*circle*/) {
                        m_convertedCircles.insert(e.Index);
                    });
                    m_world->Each<ECS::Components::ShapeBox2D>([&](ECS::Entity e, ECS::Components::ShapeBox2D& /*box*/) {
                        m_convertedBoxes.insert(e.Index);
                    });
                }
                // Ensure simulation runs normally when stopped
                Time::TimeScale(1.0f);
            } else if (current == Playback::GameState::Paused) {
                // Pause simulation by zeroing time scale
                Time::TimeScale(0.0f);
            } else if (current == Playback::GameState::Playing) {
                // Resume simulation at normal speed
                Time::TimeScale(1.0f);
            }
            // Playing/Paused: keep existing camera behavior; no toggling here
            m_lastGameState = current;
        }
    }

    // Restore automatic pixel->world conversion so newly added entities
    // use world units consistent with renderer/camera.
    if (!IsPlaying() && m_world) {
        m_world->Each<ECS::Components::LocalTransform>([&](ECS::Entity e, ECS::Components::LocalTransform& tr) {
            if (m_convertedPositions.find(e.Index) == m_convertedPositions.end()) {
                tr.Position.X = graphicsConfig::PixelsToWorld(tr.Position.X);
                tr.Position.Y = graphicsConfig::PixelsToWorld(tr.Position.Y);
                m_convertedPositions.insert(e.Index);
            }
        });

        m_world->Each<ECS::Components::ShapeCircle2D>([&](ECS::Entity e, ECS::Components::ShapeCircle2D& circle) {
            if (m_convertedCircles.find(e.Index) == m_convertedCircles.end()) {
                circle.Radius = graphicsConfig::PixelsToWorld(circle.Radius);
                m_convertedCircles.insert(e.Index);
            }
        });

        m_world->Each<ECS::Components::ShapeBox2D>([&](ECS::Entity e, ECS::Components::ShapeBox2D& box) {
            if (m_convertedBoxes.find(e.Index) == m_convertedBoxes.end()) {
                box.HalfExtents.X = graphicsConfig::PixelsToWorld(box.HalfExtents.X);
                box.HalfExtents.Y = graphicsConfig::PixelsToWorld(box.HalfExtents.Y);
                m_convertedBoxes.insert(e.Index);
            }
        });
    }

    m_editorCore.HandleInWorldInteraction(); // Handle viewport interactions

    // Synchronize inspector selection with viewport picking when hovering the viewport
    if (m_editorCore.IsViewportHovered()) {
        static EntityId s_lastInspected = 0;
        EntityId picked = m_editorCore.GetSelectedEntityId();
        if (picked != s_lastInspected) {
            if (picked != 0) {
                m_inspector.InspectEntity(picked);
            } else {
                m_inspector.ClearSelection();
            }
            s_lastInspected = picked;
        }
    }
}

// Render dock space and editor panels with a fallback when world is missing
void LevelEditor::Render() {
    _renderDockSpace();

    if (m_world) {
        m_playback.Render(); // Draw playback controls
        m_assetBrowser.Render(); // Draw asset browser
        m_editorCore.ShowEditorWindows(); // Draw core editor windows
        m_hierarchyWindow.Render(); // Draw hierarchy tree
        // Inspector follows global FontGlobalScale; pass 1.0f (no local override)
        m_inspector.Render(1.0f);
    }
    else {
        m_playback.Render(); // Keep controls visible
        m_assetBrowser.Render(); // Keep assets visible
        m_editorCore.ShowEditorWindows(); // Keep editor windows visible
        m_hierarchyWindow.Render(); // Keep hierarchy visible

        ImGui::PushFont(m_mainFont);
        ImGui::Begin("Property Editor");
        ImGui::TextDisabled("No scene attached"); // Inform user about missing scene
        ImGui::PopFont();
        ImGui::End();
    }
}

// Update the world reference and propagate it to all editor panels
void LevelEditor::SetWorld(ECS::World* world) {
    m_world = world; // Store new world
    m_playback.SetWorld(world); // Update playback panel
    m_editorCore.SetWorld(world); // Update core editor
    m_hierarchyWindow.SetWorld(world); // Update hierarchy panel
    m_inspector.SetWorld(world); // Update inspector panel
    m_assetBrowser.SetWorld(world); // Update asset browser panel
    // When binding a scene for editing, keep current camera; no toggling
    // Reset conversion tracking for the new world
    m_convertedCircles.clear();
    m_convertedBoxes.clear();
    m_convertedPositions.clear();
}
