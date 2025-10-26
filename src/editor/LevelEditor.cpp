#include "../editor/LevelEditor.h"
#include "core/Logger.h"
#include <imgui.h>

LevelEditor::LevelEditor(World* world, const LevelEditorConfig& config)
    : m_world(world), m_config(config), m_playback(world) {
}

LevelEditor::~LevelEditor() {}

void LevelEditor::Initialize(GLFWwindow* pWin) {
    if (!pWin) return;
    auto& io = ImGui::GetIO();

    // Add default font first (required for merge mode to work)
    io.Fonts->AddFontDefault();

    // Define the range for Material Symbols (Private Use Area E000-F8FF where icons live)
    static const ImWchar iconRanges[] = { 0xE000, 0xF8FF, 0 };

    // Configure font loading settings for Material Symbols
    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = true;            // Merge icons into default font so we don't need to switch fonts
    iconsConfig.PixelSnapH = true;           // Align icon pixels to grid for sharper rendering
    iconsConfig.GlyphMinAdvanceX = 24.0f;    // Minimum horizontal spacing for each icon
    iconsConfig.GlyphOffset = ImVec2(0, 6);  // Shift icons down 6 pixels to center them vertically in buttons

    // Load Material Symbols font and merge it with the default font
    m_symbolsFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/Material_Symbols_Rounded/static/MaterialSymbolsRounded-Regular.ttf",
        m_config.FontSize * m_config.FontScale,
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

    // Initialize playback controls with symbols font
    m_playback.Initialize(m_symbolsFont);
}

void LevelEditor::Update() {
    m_playback.ProcessInput();
}

void LevelEditor::Render() {
    if (!m_world) return;
    m_playback.Render();
}
