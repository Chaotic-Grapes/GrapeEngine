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
*/
/* End Header *******************************************************************/

#include "../editor/PlaybackControls.h"
#include "services/Input.h"
#include "services/UICommon.h"
#include "ecs/World.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

Playback::Playback(World* world)
    : m_world(world) {
}

Playback::~Playback() {}

void Playback::Initialize(ImFont* symbolsFont) {
    m_symbolsFont = symbolsFont;
}

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

void Playback::Render() {
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

    // Single play/pause toggle button
    // Shows play when stopped/paused
    if (m_gameState == GameState::Stopped || m_gameState == GameState::Paused) {
        button("\xEE\x80\xB7", true, GameState::Playing,
            m_gameState == GameState::Stopped ? "Game started" : "Game resumed");
    }
    // Shows pause when playing
    else {
        button("\xEE\x80\xB4", true, GameState::Paused, "Game paused");
    }
    ImGui::SameLine();

    // STOP: playing/paused > stopped
    button("\xEE\x81\x87", m_gameState != GameState::Stopped, GameState::Stopped, "Game stopped");
    ImGui::SameLine();

    // Step button (for step-by-step physics)
    // Only enabled when paused
    button("\xEE\x81\x84", m_gameState == GameState::Paused, GameState::Paused, "Stepping 1 physics frame", true);

    ImGui::End();
}

void Playback::_saveWorldState() {
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

void Playback::_restoreWorldState() {
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

bool Playback::IsPlaying() const {
    return m_gameState == GameState::Playing;
}

bool Playback::IsStepRequested() const {
    return m_stepRequested;
}

void Playback::ClearStepRequest() {
    m_stepRequested = false;
}
