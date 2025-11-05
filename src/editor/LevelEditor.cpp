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

// Constructor: Initialize level editor with world reference and configuration
LevelEditor::LevelEditor(World* world, const LevelEditorConfig& config)
    : m_world(world),
    m_config(config),
    m_playback(world),         // Playback controls (play/pause/step)
    m_assetBrowser(),           // File browser + prefab creation
    m_editorCore(),           // Entity manipulation tools
    m_hierarchyWindow(),        // Entity tree view
    m_inspector()               // Unified component inspector
{
}

LevelEditor::~LevelEditor() {}

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

    // Render panels in order
    m_playback.Render();                 // Top toolbar with play controls
    m_assetBrowser.Render();             // Left panel: file browser
    m_editorCore.ShowEditorWindows();    // Viewport tools
    m_hierarchyWindow.Render();          // Left panel: entity tree
    m_inspector.Render(1.0f);            // Right panel: component editor (unified entity/prefab)
}
