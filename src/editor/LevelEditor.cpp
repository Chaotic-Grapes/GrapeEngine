#include "../editor/LevelEditor.h"
#include "services/Input.h"
#include "ecs/World.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include "services/UICommon.h"
 
LevelEditor::LevelEditor(World* world, const LevelEditorConfig& config)
    : m_config(config), m_world(world) {}

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
}

void LevelEditor::ProcessInput() {
    // Play/Stop: Ctrl + P
    if (Input::IsKeyPressed(GLFW_KEY_P) && Input::IsKeyDown(GLFW_KEY_LEFT_CONTROL) &&
        !Input::IsKeyDown(GLFW_KEY_LEFT_SHIFT) && !Input::IsKeyDown(GLFW_KEY_LEFT_ALT))
    {
        if (m_gameState == GameState::Stopped) {
            // Simulate play button
            _saveWorldState();
            m_gameState = GameState::Playing;
            LOG_INFO("Game started (Ctrl+P)");
        }
        else {
            // Simulate stop button
            _restoreWorldState();
            m_gameState = GameState::Stopped;
            LOG_INFO("Game stopped (Ctrl+P)"); 
        }
    }

    // Pause/Resume: Ctrl + Shift + P
    if (Input::IsKeyPressed(GLFW_KEY_P) && Input::IsKeyDown(GLFW_KEY_LEFT_CONTROL) &&
        Input::IsKeyDown(GLFW_KEY_LEFT_SHIFT))
    {
        if (m_gameState == GameState::Playing) {
            m_gameState = GameState::Paused;
            LOG_INFO("Game paused (Ctrl+Shift+P)");
        }
        else if (m_gameState == GameState::Paused) {
            m_gameState = GameState::Playing;
            LOG_INFO("Game resumed (Ctrl+Shift+P)");
        }
    }

    // Step: Alt + P
    if (Input::IsKeyPressed(GLFW_KEY_P) && Input::IsKeyDown(GLFW_KEY_LEFT_ALT)) {
        if (m_gameState == GameState::Paused) {
            m_stepRequested = true;
            LOG_INFO("Stepping 1 physics frame (Alt+P)");
        }
    }
}

void LevelEditor::Render() {
    if (!m_world) return;
    _showPlaybackControls(); // Play/Stop, Pause/Resume, Step
}

void LevelEditor::_showPlaybackControls() {
    // Use config values
    UICommon::ApplyLayout(UICommon::WindowId::EDITOR_PLAYBACK);
    ImGui::Begin("Game Controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    // If mouse is over window then show tooltips (with keyboard shortcuts)
    if (ImGui::IsWindowHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Play/Stop - Ctrl+P");
        ImGui::Text("Pause/Resume - Ctrl+Shift+P");
        ImGui::Text("Step - Alt+P");
        ImGui::Dummy(ImVec2(0, 20));

        // Show current state (resume is essentially == playing)
        ImGui::Text("State: %s",
            m_gameState == GameState::Stopped ? "STOPPED" :
            m_gameState == GameState::Paused ? "PAUSED" :
            "PLAYING"
        );
        ImGui::EndTooltip();
    }
    // Buttons 
    // If the buttons have been clicked, they get grayed out
    auto button = [&](const char* icon, bool shouldBeEnabled, GameState newState, const char* logMsg,
        bool isStepButton = false, ImVec2 size = ImVec2(100, 40))
        {
            if (!shouldBeEnabled) ImGui::BeginDisabled();
            bool clicked = ImGui::Button(icon, size);
            if (!shouldBeEnabled) ImGui::EndDisabled();
            if (clicked) {
                if (isStepButton) {
                    // Special case: STEP just sets flag
                    m_stepRequested = true;
                }
                else {
                    // Normal case: change state
                    if (newState == GameState::Playing && m_gameState == GameState::Stopped) {
                        _saveWorldState();
                    }
                    if (newState == GameState::Stopped) {
                        _restoreWorldState();
                    }
                    m_gameState = newState;
                }
                LOG_INFO(logMsg);
            }
        };

    // Play and stop buttons
    // PLAY: stopped > playing; STOP: playing/paused > stopped
    button("\xEE\x80\xB7##play", m_gameState == GameState::Stopped, GameState::Playing, "Game started");
    ImGui::SameLine();
    button("\xEE\x81\x87##stop", m_gameState != GameState::Stopped, GameState::Stopped, "Game stopped");
    ImGui::SameLine();

    // Pause and resume buttons
    // PAUSE: playing > paused; RESUME: paused > playing
    button("\xEE\x80\xB4##pause", m_gameState == GameState::Playing, GameState::Paused, "Game paused");
    ImGui::SameLine();
    button("\xEE\x80\xB7##resume", m_gameState == GameState::Paused, GameState::Playing, "Game resumed");
    ImGui::SameLine();

    // Step button (for step-by-step physics)
    // Scenario where step button gets grayed out: when playing/resumed/stopped
    button("\xEE\x81\x84##step", m_gameState == GameState::Paused, GameState::Paused, "Stepping 1 physics frame", true);

    ImGui::End();
}

void LevelEditor::_saveWorldState() {
    if (!HasValidWorld()) return;

    LOG_INFO("Saving world state...");

    // Get all entities and serialize them
    auto entities = m_world->GetEntityManager().GetAllEntities();
    nlohmann::json worldJson = nlohmann::json::array();

    for (const auto& entityId : entities) {
        auto entity = m_world->GetEntityManager().GetEntity(entityId);
        auto entityJson = Serialization::EntitySerializer::SerializeEntity(entity);
        worldJson.push_back(entityJson);
    }

    m_savedWorldState = worldJson;
    LOG_INFO("Saved " << entities.size() << " entities");
}

void LevelEditor::_restoreWorldState() {
    if (!HasValidWorld() || m_savedWorldState.empty()) {
        LOG_WARNING("No saved state to restore");
        return;
    }

    LOG_INFO("Restoring world state...");

    // Delete all current entities
    m_world->GetEntityManager().DestroyAllEntities();

    // Recreate from saved JSON
    for (const auto& entityJson : m_savedWorldState) {
        Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);
    }

    LOG_INFO("World restored");
}

bool LevelEditor::IsPlaying() const {
    return m_gameState == GameState::Playing;
}

bool LevelEditor::IsStepRequested() const {
    return m_stepRequested;
}

void LevelEditor::ClearStepRequest() {
    m_stepRequested = false;
}
