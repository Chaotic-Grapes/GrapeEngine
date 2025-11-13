/* Start Header *****************************************************************/
/*!
\file   PlaybackControls.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the PlaybackControls class for managing game playback state in the
level editor.

Features:
- Game state management with play/pause/stop/step functionality
- Keyboard shortcuts (Ctrl+P, Ctrl+Shift+P, Alt+P)
- World state serialization and restoration for play/stop
- ImGui UI with tooltips and Material Symbols icons
- Step-by-step physics frame execution for debugging

Reference:
- ImGui UI layout and button styling (imgui.h)
*/
/* End Header *******************************************************************/

#include "../editor/PlaybackControls.h"
#include "services/Input.h"
#include "ecs/World.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

// Constructor: stores pointer to the world for playback operations.
// Starts with default stopped state and no saved snapshot.
Playback::Playback(ECS::World* world)
    : m_world(world) {
}

Playback::~Playback() {}

// Initialize fonts used by the playback UI and Material Symbols.
// Keeps pointers for drawing icons on control buttons.
void Playback::Initialize(ImFont* mainFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_symbolsFont = symbolsFont;
}

// Process keyboard input for play/stop, pause/resume, and step.
// Maps Ctrl+P, Ctrl+Shift+P, and Alt+P into state changes/flags.
void Playback::ProcessInput() {
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

// Render the playback toolbar UI using ImGui.
// Centers buttons and shows tooltips with current state.
void Playback::Render() {
    // Window flags: NoResize prevents manual resizing; docking stays enabled by default
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize;
    // Use default ImGui tab styling; no local overrides
    ImGui::Begin("Game Controls", nullptr, flags);

    // Follow global FontGlobalScale (no local override)

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

    // If the buttons have been clicked, they get grayed out
    auto button = [&](const char* icon, bool shouldBeEnabled, GameState newState, const char* logMsg,
        bool isStepButton = false, ImVec2 size = ImVec2(100, 40))
        {
            // Disabled scope: gray-out and block clicks when the control shouldn't be active
            if (!shouldBeEnabled) ImGui::BeginDisabled();

            // ImGui::Button returns true if the user clicks it during this frame.
            bool clicked = ImGui::Button(icon, size);
            // Close the disabled block if it was opened
            if (!shouldBeEnabled) ImGui::EndDisabled();

            // Handle click logic
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

    // Center buttons horizontally within available content region (Y unchanged)
    {
        float btnWidth = 100.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;       // Default horizontal spacing between buttons from ImGui style
        float totalWidth = btnWidth * 3.0f + spacing * 2.0f;   // Total width = 3 buttons + 2 spaces between them
        float availWidth = ImGui::GetContentRegionAvail().x;   // Get width of available space in current ImGui window/content region
        float startX = (availWidth - totalWidth) * 0.5f;       // Compute starting X position so buttons are centered
        if (startX < 0.0f) startX = 0.0f;                      // If available width < total width, don't use negative starting pos
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);
    }

    // Single play/pause toggle button
    // Shows play when stopped/paused
    if (m_gameState == GameState::Stopped || m_gameState == GameState::Paused) {
        ImGui::PushFont(m_symbolsFont);
        button("\xEE\x80\xB7", true, GameState::Playing,
            m_gameState == GameState::Stopped ? "Game started" : "Game resumed", false, ImVec2(100, 40));
        ImGui::PopFont();
    }
    // Shows pause when playing
    else {
        ImGui::PushFont(m_symbolsFont);
        button("\xEE\x80\xB4", true, GameState::Paused, "Game paused", false, ImVec2(100, 40));
        ImGui::PopFont();
    }
    ImGui::SameLine();

    // STOP: playing/paused > stopped
    ImGui::PushFont(m_symbolsFont);
    button("\xEE\x81\x87", m_gameState != GameState::Stopped, GameState::Stopped, "Game stopped", false, ImVec2(100, 40));
    ImGui::PopFont();
    ImGui::SameLine();

    // Step button (for step-by-step physics)
    // Only enabled when paused
    ImGui::PushFont(m_symbolsFont);
    button("\xEE\x81\x84", m_gameState == GameState::Paused, GameState::Paused, "Stepping 1 physics frame", true, ImVec2(100, 40));
    ImGui::PopFont();

    ImGui::End();
}

// Save the current world into a JSON snapshot.
// Skips editor camera entities to keep the restore clean.
void Playback::_saveWorldState() {
    if (!HasValidWorld()) return;

    LOG_INFO("Saving world state.");

    nlohmann::json worldJson = nlohmann::json::array();
    size_t entityCount = 0;

    m_world->Each([&](ECS::Entity entity) {
        if (m_world->Has<ECS::Components::Name>(entity)) {
            const auto& name = m_world->Get<ECS::Components::Name>(entity);

            // Skip both variants of the editor camera name
            if (std::strcmp(name.Value, "EditorCamera") == 0 ||
                std::strcmp(name.Value, "Editor Camera") == 0)
            {
                return;
            }
        }

        auto entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
        worldJson.push_back(entityJson);
        ++entityCount;
        });

    m_savedWorldState = worldJson;
    LOG_INFO("Saved " << entityCount << " entities");
}

void Playback::_restoreWorldState() {
    // Restore the world from the previously saved snapshot.
    // Keeps editor cameras and rebuilds runtime entities.
    if (!HasValidWorld() || m_savedWorldState.empty()) {
        LOG_WARNING("No saved state to restore");
        return;
    }

    LOG_INFO("Restoring world state.");

    std::vector<ECS::Entity> allEntities;
    m_world->Each([&](ECS::Entity e) {
        if (m_world->Has<ECS::Components::Name>(e)) {
            const auto& name = m_world->Get<ECS::Components::Name>(e);

            // --- FIX: keep the editor camera, whichever way it's named ---
            if (std::strcmp(name.Value, "EditorCamera") == 0 ||
                std::strcmp(name.Value, "Editor Camera") == 0)
            {
                return; // keep editor camera
            }
        }
        allEntities.push_back(e);
        });

    for (const auto& e : allEntities) {
        m_world->Destroy(e);
    }

    for (const auto& entityJson : m_savedWorldState) {
        Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);
    }

    LOG_INFO("World restored");
}


// Query: Is the game currently in the Playing state?
bool Playback::IsPlaying() const {
    return m_gameState == GameState::Playing;
}

// Query: Was a single-step frame requested while paused?
bool Playback::IsStepRequested() const {
    return m_stepRequested;
}

// Clear the step request flag
// Clear any outstanding step request flag.
void Playback::ClearStepRequest() {
    m_stepRequested = false;
}

// Update the world reference safely when scenes change
// Update the bound world reference used by playback operations.
// Clears saved snapshot since it no longer matches the world.
void Playback::SetWorld(ECS::World* world) {
    m_world = world;
    m_gameState = GameState::Stopped;
    m_stepRequested = false;
    m_savedWorldState.clear();
}
