/* Start Header *****************************************************************/
/*!
\file   LevelEditor.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the LevelEditor class which orchestrates all editor panels.
- Playback controls: Play/pause/step simulation
- Asset browser: File system navigation and prefab creation
- Hierarchy window: Entity tree view with parent-child relationships
- Inspector window: Unified component editor (adapts to entity/prefab selection)
*/
/* End Header *******************************************************************/

#include "../editor/LevelEditor.h"
#include "core/Logger.h"
#include <imgui.h>
#include <imgui_internal.h>

// Constructor: Initialize level editor with world reference and configuration
LevelEditor::LevelEditor(ECS::World* world, const LevelEditorConfig& config)
    : m_world(world), m_config(config), m_playback(world), m_symbolsFont(nullptr), 
    m_mainFont(nullptr), m_boldFont(nullptr), m_assetBrowser(), m_editorCore(), 
    m_hierarchyWindow(), m_inspector() {
}

LevelEditor::~LevelEditor() {}

void LevelEditor::_buildDockLayout() {
    if (m_dockLayoutBuilt) return;

    // Create a full-screen DockSpace and initialize layout once
    ImGuiViewport* vp = ImGui::GetMainViewport();

    // Safety check to prevent negative sizes
    if (vp->Size.x <= 0 || vp->Size.y <= 0) return;

    // Reset and rebuild
    ImGui::DockBuilderRemoveNode(m_dockspaceId);
    ImGui::DockBuilderAddNode(m_dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderSetNodeSize(m_dockspaceId, vp->Size);

    // First: split off right column (25% instead of 33%)
    ImGuiID leftCenterNode, rightNode;
    ImGui::DockBuilderSplitNode(m_dockspaceId, ImGuiDir_Right, 0.25f, &rightNode, &leftCenterNode);

    // Split left + center vertically: top 65%, bottom 35% (for Asset Browser)
    ImGuiID topSection, assetBrowserNode;
    ImGui::DockBuilderSplitNode(leftCenterNode, ImGuiDir_Up, 0.65f, &topSection, &assetBrowserNode);

    // Split top section into left (33.3% of 75% = 25% total) and center (66.6% of 75% = 50% total)
    ImGuiID leftTopNode, centerTopSection;
    ImGui::DockBuilderSplitNode(topSection, ImGuiDir_Left, 0.333f, &leftTopNode, &centerTopSection);

    // Split center top section so Game Controls sits at TOP (~15%) and Viewport below (~85%)
    ImGuiID centerControlsNode, centerViewportNode;
    // Using ImGuiDir_Up: out_node_at_dir = TOP (Controls), remainder = BOTTOM (Viewport)
    ImGui::DockBuilderSplitNode(centerTopSection, ImGuiDir_Up, 0.154f, &centerControlsNode, &centerViewportNode);
    // Keep the tab bar visible for Game Controls so users can access the tab

    // The remainder from the last split (centerViewportNode) stays the central node
    // We intentionally rely on DockBuilderSplitNode semantics to avoid multiple central nodes

    // Dock windows
    ImGui::DockBuilderDockWindow("Hierarchy", leftTopNode);
    ImGui::DockBuilderDockWindow("Game Controls", centerControlsNode);
    ImGui::DockBuilderDockWindow("Viewport", centerViewportNode);
    ImGui::DockBuilderDockWindow("Asset Browser", assetBrowserNode); // Spans left + center bottom
    ImGui::DockBuilderDockWindow("Prefab Editor", rightNode);
    ImGui::DockBuilderDockWindow("Property Editor", rightNode);

    ImGui::DockBuilderFinish(m_dockspaceId);
    m_dockLayoutBuilt = true;
}

void LevelEditor::_renderDockSpace() {
    // Safety check
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp->Size.x <= 0 || vp->Size.y <= 0) return;

    // Offset dockspace host below the global main menu bar to avoid overlapping tabs
    const float topOffset = ImGui::GetFrameHeight();

    // Calculate safe sizes that aren't negative
    ImVec2 safePos(vp->Pos.x, vp->Pos.y + topOffset);
    ImVec2 safeSize(vp->Size.x, std::max(1.0f, vp->Size.y - topOffset)); // Ensure at least 1px height

    ImGui::SetNextWindowPos(safePos);
    ImGui::SetNextWindowSize(safeSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent black
    ImGui::Begin("MainDockSpaceHost", nullptr, hostFlags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    m_dockspaceId = ImGui::GetID("MainDockSpace");
    ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode; // Show scene through central node
    ImGui::DockSpace(m_dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);

    _buildDockLayout();
    ImGui::End();
}

void LevelEditor::Initialize(GLFWwindow* pWin) {
    if (!pWin) return;

    auto& io = ImGui::GetIO();

    // Configure global ImGui style for editor panels
    ImGuiStyle& style = ImGui::GetStyle();

    // Keep window/popup borders at theme defaults
    // Only tune child window borders for cleaner nested panels
    style.ChildBorderSize = 0.75f;  // Thin borders on BeginChild regions

    // Clear existing fonts (important for hot-reload scenarios)
    io.Fonts->Clear();

    // Load three fonts: regular text, bold headers, and Material Symbols icons
    // 1. REGULAR FONT: Inter Medium for body text
    float textFontSize = m_config.FontSize;
    m_mainFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/Inter/static/Inter_24pt-Medium.ttf",
        textFontSize
    );

    if (!m_mainFont) {
        LOG_ERROR("Failed to load regular Inter font, using default");
        m_mainFont = io.Fonts->AddFontDefault();
    }

    // 2. BOLD FONT: Inter ExtraBold for headers and emphasis
    m_boldFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/Inter/static/Inter_24pt-ExtraBold.ttf",
        textFontSize
    );

    if (!m_boldFont) {
        LOG_ERROR("Failed to load bold Inter font");
        m_boldFont = io.Fonts->AddFontDefault();
    }

    // 3. ICON FONT: Material Symbols for UI icons (NOT merged with text)
    // Why separate? Allows us to switch fonts with PushFont/PopFont for icons
    static const ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };  // Private Use Area

    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = false;           // Separate font (not merged)
    iconsConfig.PixelSnapH = true;           // Crisp icon rendering
    iconsConfig.GlyphMinAdvanceX = 24.0f;    // Minimum icon width
    iconsConfig.GlyphOffset = ImVec2(0, 0);  // No vertical offset needed

    float iconFontSize = 18.0f;  // Slightly smaller than text
    m_symbolsFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
        iconFontSize,
        &iconsConfig,
        iconRanges
    );

    // Build font atlas (required after adding fonts)
    io.Fonts->Build();

    // Pass fonts to each panel for consistent styling
    m_playback.Initialize(m_mainFont, m_symbolsFont);
    m_assetBrowser.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);
    m_editorCore.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);
    m_hierarchyWindow.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world, &m_editorCore);
    m_inspector.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);

    // 1. Asset browser -> Inspector connection
    // When user selects prefab in asset browser, open it in inspector
    m_assetBrowser.SetInspector(&m_inspector);

    // 2. Hierarchy -> Inspector connection
    // When user selects entity in hierarchy, show its components in inspector
    m_hierarchyWindow.OnSelectionChanged([this](EntityId id) {
        if (id) {
            // Entity selected: show entity inspector mode
            m_inspector.InspectEntity(id);
        }
        else {
            // Nothing selected: clear inspector
            m_inspector.ClearSelection();
        }
        });
}

// Process input for all editor panels each frame
void LevelEditor::Update() {
    // Process playback controls (play/pause/step)
    m_playback.ProcessInput();

    // Handle entity manipulation in viewport (move/rotate/scale)
    m_editorCore.HandleInWorldInteraction();
}

// Render all editor panels each frame
void LevelEditor::Render() {
    if (!m_world) return;

    // Render dockspace first
    _renderDockSpace();

    // Render panels in order
    m_playback.Render();                 // Top toolbar with play controls
    m_assetBrowser.Render();             // Left panel: file browser
    m_editorCore.ShowEditorWindows();    // Viewport tools
    m_hierarchyWindow.Render();          // Left panel: entity tree
    m_inspector.Render(1.0f);            // Right panel: component editor (unified entity/prefab)
}
