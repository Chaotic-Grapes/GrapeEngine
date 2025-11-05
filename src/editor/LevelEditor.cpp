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
    : m_world(world), m_config(config), m_playback(world), m_assetBrowser(), m_gameObjectEditor(world) {
}

LevelEditor::~LevelEditor() {}

void LevelEditor::Initialize(GLFWwindow* pWin) {
    if (!pWin) return;
    auto& io = ImGui::GetIO();

    io.Fonts->Clear();

    // 1. Load regular Inter as main font for text
    float textFontSize = m_config.FontSize;
    m_mainFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/Inter/static/Inter_24pt-Medium.ttf",
        textFontSize
    );

    if (!m_mainFont) {
        LOG_ERROR("Failed to load regular Inter font, using default");
        m_mainFont = io.Fonts->AddFontDefault();
    }

    // 2. Load bold Inter font for headers
    m_boldFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/Inter/static/Inter_24pt-ExtraBold.ttf",
        textFontSize
    );

    if (!m_boldFont) {
        LOG_ERROR("Failed to load bold Inter font");
        m_boldFont = io.Fonts->AddFontDefault();
    }

    // 3. Load Material Symbols as SEPARATE font
    static const ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };

    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = false;           // Keep them separate
    iconsConfig.PixelSnapH = true;
    iconsConfig.GlyphMinAdvanceX = 24.0f;
    iconsConfig.GlyphOffset = ImVec2(0, 0);  // No vertical offset

    float iconFontSize = 18.0f;
    m_symbolsFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
        iconFontSize,
        &iconsConfig,
        iconRanges
    );

    io.Fonts->Build();

    // Pass both fonts to components
    m_playback.Initialize(m_mainFont, m_symbolsFont);  // Update to take both fonts
    m_assetBrowser.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);
    m_entityEditor.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);
    m_hierarchyWindow.Initialize(m_mainFont, m_boldFont, m_symbolsFont, m_world);
}

// Process input for all editor panels
void LevelEditor::Update() {
    m_playback.ProcessInput();
    m_entityEditor.HandleInWorldInteraction();
}

// Render all editor panels
void LevelEditor::Render() {
    if (!m_world) return;
    m_playback.Render();
    m_assetBrowser.Render();
    m_gameObjectEditor.ShowEditorWindows();
}
