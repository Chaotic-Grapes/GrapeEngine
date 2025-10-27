/* Start Header *****************************************************************/
/*!
\file   LevelEditor.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the LevelEditor class which orchestrates all editor panels including
playback controls and asset browser.

Features:
- Material Symbols font loading for icon support
- Playback controls panel initialization and management
- Asset browser panel initialization and management
- Editor input processing and rendering coordination

References:
- Font configuration based on ImGui documentation (docs/FONTS.md)
- MergeMode technique from imgui.cpp ImFontConfig examples
- Font atlas building pattern from ImGui source (imgui_draw.cpp)
- Icon font integration adapted from community examples in ImGui discussions
*/
/* End Header *******************************************************************/

#include "../editor/LevelEditor.h"
#include "core/Logger.h"
#include <imgui.h>

// Constructor: initialize level editor with world and config
LevelEditor::LevelEditor(World* world, const LevelEditorConfig& config)
    : m_world(world), m_config(config), m_playback(world), m_assetBrowser() {
}

LevelEditor::~LevelEditor() {}

// Initialize ImGui fonts and editor panels
void LevelEditor::Initialize(GLFWwindow* pWin) {
    if (!pWin) return;
    auto& io = ImGui::GetIO();

    /// Load default font at custom size for text
    float textFontSize = 15.0f; 
    io.Fonts->AddFontDefault(); 
    io.Fonts->Clear();  // Clear auto-added default font to override size

    // Load default font at custom size
    ImFontConfig textConfig;
    textConfig.SizePixels = textFontSize;
    io.Fonts->AddFontDefault(&textConfig);

    // Define the range for Material Symbols
    // Reference: https://fonts.google.com/icons
    static const ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };

    // Configure font loading settings for Material Symbols
    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = true;            // Merge icons into default font so we don't need to switch fonts
    iconsConfig.PixelSnapH = true;           // Align icon pixels to grid for sharper rendering
    iconsConfig.GlyphMinAdvanceX = 24.0f;    // Minimum horizontal spacing for each icon
    iconsConfig.GlyphOffset = ImVec2(0, 5);  // Shift icons down 5 pixels to center them vertically in buttons

    // Load Material Symbols at smaller size
    float iconFontSize = 19.0f;  

    // Load Material Symbols font and merge it with the default font
    m_symbolsFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
        iconFontSize,
        &iconsConfig,
        iconRanges
    );

    // Checks
    if (m_symbolsFont == nullptr) {
        LOG_ERROR("Failed to load Material Symbols font");
    }
    else {
        LOG_INFO("Material Symbols font merged successfully");
    }

    // Build the font atlas (combines default font + Material Symbols)
    io.Fonts->Build();

    // Initialize playback controls & asset browser with symbols font
    m_playback.Initialize(m_symbolsFont);
    m_assetBrowser.Initialize(m_symbolsFont, m_world);
}

// Process input for all editor panels
void LevelEditor::Update() {
    m_playback.ProcessInput();
}

// Render all editor panels
void LevelEditor::Render() {
    if (!m_world) return;
    m_playback.Render();
    m_assetBrowser.Render();
}
