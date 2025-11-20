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
- State change callbacks for external coordination

Reference:
- ImGui UI layout and button styling (imgui.h)
*/
/* End Header *******************************************************************/

#include "../editor/PlaybackControls.h"
#include "services/Input.h"
#include "services/Time.h"
#include "ecs/World.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include <unordered_map>
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
void Playback::Initialize(ImFont* mainFont, ImFont* symbolsFont, float toolbarHeight) {
    m_mainFont = mainFont;
    m_symbolsFont = symbolsFont;
    m_toolbarHeight = toolbarHeight;
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
            _changeState(GameState::Playing);
            LOG_INFO("Game started (Ctrl+P)");
        }
        else {
            // Simulate stop button
            _restoreWorldState();
            _changeState(GameState::Stopped);
            LOG_INFO("Game stopped (Ctrl+P)");
        }
    }

    // Pause/Resume: Ctrl + Shift + P
    if (Input::IsKeyPressed(GLFW_KEY_P) && Input::IsKeyDown(GLFW_KEY_LEFT_CONTROL) &&
        Input::IsKeyDown(GLFW_KEY_LEFT_SHIFT))
    {
        if (m_gameState == GameState::Playing) {
            _changeState(GameState::Paused);
            LOG_INFO("Game paused (Ctrl+Shift+P)");
        }
        else if (m_gameState == GameState::Paused) {
            _changeState(GameState::Playing);
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
    // Toolbar window flags: NoTitleBar removes title bar, NoScrollbar prevents scrolling,
    // NoResize prevents manual resizing, NoCollapse prevents collapsing
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | 
                             ImGuiWindowFlags_NoScrollbar | 
                             ImGuiWindowFlags_NoResize | 
                             ImGuiWindowFlags_NoCollapse;
    
    // Set fixed height for toolbar from config
    ImGui::SetNextWindowSize(ImVec2(-1, m_toolbarHeight), ImGuiCond_Always);
    
    // Use default ImGui tab styling; no local overrides
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
    ImGui::Begin("Game Controls", nullptr, flags);

    // Follow global FontGlobalScale (no local override)

    // If the buttons have been clicked, they get grayed out
    auto button = [&](const char* icon, bool shouldBeEnabled, GameState newState, const char* logMsg,
        bool isStepButton = false, const char* tooltip = nullptr, ImVec2 size = ImVec2(35, 20))
        {
            // Disabled scope: gray-out and block clicks when the control shouldn't be active
            if (!shouldBeEnabled) ImGui::BeginDisabled();

            // ImGui::Button returns true if the user clicks it during this frame.
            // Style adjustments for padding and font scaling
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(-5, -5));
            bool clicked = ImGui::Button(icon, size);
            
            // Show tooltip on hover
            if (tooltip && ImGui::IsItemHovered()) {
                ImGui::PushFont(m_mainFont);

                ImGui::BeginTooltip();
                ImGui::Text("%s", tooltip);
                ImGui::Dummy(ImVec2(0, 20));

                // Show current state (resume is essentially == playing)
                ImGui::Text("State: %s",
                            m_gameState == GameState::Stopped ? "STOPPED" : m_gameState == GameState::Paused ? "PAUSED"
                                                                                                             : "PLAYING");
                ImGui::EndTooltip();

                ImGui::PopFont();
            }

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
                    _changeState(newState);
                }
                LOG_INFO(logMsg);
            }

            ImGui::PopStyleVar();
        };

    // Center buttons horizontally within available content region (Y unchanged)
    {
        float btnWidth = 35.0f;
        float totalWidth = btnWidth * 3.0f;                    // Total width = 3 buttons + 2 spaces between them
        float availWidth = ImGui::GetContentRegionAvail().x;   // Get width of available space in current ImGui window/content region
        float startX = (availWidth - totalWidth) * 0.5f;       // Compute starting X position so buttons are centered
        startX = std::max(startX, 0.0f);                       // If available width < total width, don't use negative starting pos

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + startX);
    }

    // Single play/pause toggle button
    // Shows play when stopped/paused
    if (m_gameState == GameState::Stopped || m_gameState == GameState::Paused) {
        ImGui::PushFont(m_symbolsFont);
        button("\xEE\x80\xB7", HasValidWorld(), GameState::Playing,
            m_gameState == GameState::Stopped ? "Game started" : "Game resumed", false,
            m_gameState == GameState::Stopped ? "Play (Ctrl+P)" : "Resume (Ctrl+Shift+P)");
        ImGui::PopFont();
    }
    // Shows pause when playing
    else {
        ImGui::PushFont(m_symbolsFont);
        button("\xEE\x80\xB4", HasValidWorld(), GameState::Paused, "Game paused", false, "Pause (Ctrl+Shift+P)");
        ImGui::PopFont();
    }
    ImGui::SameLine();

    // STOP: playing/paused > stopped
    ImGui::PushFont(m_symbolsFont);
    button("\xEE\x81\x87", m_gameState != GameState::Stopped, GameState::Stopped, "Game stopped", false, "Stop (Ctrl+P)");
    ImGui::PopFont();
    ImGui::SameLine();

    // Step button (for step-by-step physics)
    // Only enabled when paused
    ImGui::PushFont(m_symbolsFont);
    button("\xEE\x81\x84", m_gameState == GameState::Paused, GameState::Paused, "Stepping 1 physics frame", true, "Step (Alt+P)");
    ImGui::PopFont();

    ImGui::PopStyleVar();
    ImGui::End();
}

// Save the current world into a JSON snapshot.
// Skips editor camera entities to keep the restore clean.
void Playback::_saveWorldState() {
    if (!HasValidWorld()) return;

    LOG_INFO("Saving world state.");

    nlohmann::json worldJson = nlohmann::json::object();
    nlohmann::json entitiesArray = nlohmann::json::array();
    nlohmann::json hierarchyArray = nlohmann::json::array();
    
    // Map to track entity index in save order
    std::unordered_map<uint32_t, size_t> entityToSaveIndex;
    size_t entityCount = 0;

    // Helper to recursively save entities in hierarchy order (parents before children)
    std::function<void(ECS::Entity)> saveRecursive = [&](ECS::Entity entity) {
        if (m_world->Has<ECS::Components::Name>(entity)) {
            const auto& name = m_world->Get<ECS::Components::Name>(entity);
            // Skip editor camera
            if (std::strcmp(name.Value, "EditorCamera") == 0 ||
                std::strcmp(name.Value, "Editor Camera") == 0) {
                return;
            }
        }

        // Track this entity's position in save order
        entityToSaveIndex[entity.Index] = entityCount;

        // Save entity components (WITHOUT Parent component - we'll rebuild hierarchy separately)
        auto entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
        
        // Remove Parent component from serialization if present
        if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
            auto& components = entityJson["Components"];
            components.erase(
                std::remove_if(components.begin(), components.end(),
                    [](const nlohmann::json& comp) {
                        return comp.contains("TypeName") && comp["TypeName"] == "Parent";
                    }),
                components.end()
            );
        }
        
        entitiesArray.push_back(entityJson);
        ++entityCount;

        // Save all children recursively
        m_world->ForChildren(entity, [&](ECS::Entity child) {
            saveRecursive(child);
        });
    };

    // Start with all root entities (those without parents)
    m_world->Each([&](ECS::Entity entity) {
        if (m_world->ParentOf(entity).IsNull()) {
            saveRecursive(entity);
        }
    });

    // Now save hierarchy relationships separately (as child->parent index mappings)
    m_world->Each<ECS::Parent>([&](ECS::Entity child, const ECS::Parent& parent) {
        // Skip editor camera
        if (m_world->Has<ECS::Components::Name>(child)) {
            const auto& name = m_world->Get<ECS::Components::Name>(child);
            if (std::strcmp(name.Value, "EditorCamera") == 0 ||
                std::strcmp(name.Value, "Editor Camera") == 0) {
                return;
            }
        }
        
        auto childIt = entityToSaveIndex.find(child.Index);
        auto parentIt = entityToSaveIndex.find(parent.ParentEntity.Index);
        
        if (childIt != entityToSaveIndex.end() && parentIt != entityToSaveIndex.end()) {
            nlohmann::json hierarchyEntry;
            hierarchyEntry["child"] = childIt->second;
            hierarchyEntry["parent"] = parentIt->second;
            hierarchyArray.push_back(hierarchyEntry);
        }
    });

    worldJson["entities"] = entitiesArray;
    worldJson["hierarchy"] = hierarchyArray;
    m_savedWorldState = worldJson;
    
    LOG_INFO("Saved " << entityCount << " entities with hierarchy in order");
}

void Playback::_restoreWorldState() {
    // Restore the world from the previously saved snapshot.
    // Keeps editor cameras and rebuilds runtime entities.
    if (!HasValidWorld() || m_savedWorldState.is_null()) {
        LOG_WARNING("No saved state to restore");
        return;
    }

    // Check for new format (object with entities and hierarchy)
    if (!m_savedWorldState.is_object() || !m_savedWorldState.contains("entities")) {
        LOG_ERROR("Saved world state has invalid format.");
        return;
    }

    LOG_INFO("Restoring world state.");

    // Destroy all current entities (except editor camera)
    std::vector<ECS::Entity> entitiesToDestroy;
    m_world->Each([&](ECS::Entity entity) {
        if (m_world->Has<ECS::Components::Name>(entity)) {
            const auto& name = m_world->Get<ECS::Components::Name>(entity);
            // Skip editor camera
            if (std::strcmp(name.Value, "EditorCamera") == 0 ||
                std::strcmp(name.Value, "Editor Camera") == 0) {
                return;
            }
        }
        entitiesToDestroy.push_back(entity);
    });

    for (auto entity : entitiesToDestroy) {
        m_world->Destroy(entity);
    }

    // Extract entities and hierarchy arrays
    const auto& entitiesArray = m_savedWorldState["entities"];
    const auto& hierarchyArray = m_savedWorldState.contains("hierarchy") && m_savedWorldState["hierarchy"].is_array() 
        ? m_savedWorldState["hierarchy"] 
        : nlohmann::json::array();

    // First pass: Create all entities (without Parent components - those were stripped)
    std::vector<ECS::Entity> restoredEntities;
    restoredEntities.reserve(entitiesArray.size());

    for (const auto& entityJson : entitiesArray) {
        ECS::Entity newEntity = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);
        restoredEntities.push_back(newEntity);
    }

    // Second pass: Rebuild hierarchy from saved relationships
    for (const auto& hierarchyEntry : hierarchyArray) {
        if (!hierarchyEntry.contains("child") || !hierarchyEntry.contains("parent")) {
            continue;
        }
        
        size_t childIndex = hierarchyEntry["child"].get<size_t>();
        size_t parentIndex = hierarchyEntry["parent"].get<size_t>();
        
        if (childIndex < restoredEntities.size() && parentIndex < restoredEntities.size()) {
            ECS::Entity child = restoredEntities[childIndex];
            ECS::Entity parent = restoredEntities[parentIndex];
            m_world->Attach(child, parent);
        }
    }

    LOG_INFO("Restored " << restoredEntities.size() << " entities with hierarchy.");
}

// Internal state change handler that manages time scale and callbacks
void Playback::_changeState(GameState newState) {
    if (m_gameState == newState) return;

    GameState oldState = m_gameState;
    m_gameState = newState;

    // Handle time scale changes based on state
    switch (newState) {
    case GameState::Stopped:
        Time::TimeScale(1.0f);
        break;
    case GameState::Paused:
        Time::TimeScale(0.0f);
        break;
    case GameState::Playing:
        Time::TimeScale(1.0f);
        break;
    }

    // Notify external listeners about state change
    if (m_onStateChanged) {
        m_onStateChanged(oldState, newState);
    }
}

// Register callback for state change events
void Playback::OnStateChanged(std::function<void(GameState, GameState)> callback) {
    m_onStateChanged = callback;
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

// Get current game state
Playback::GameState Playback::GetGameState() const {
    return m_gameState;
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
