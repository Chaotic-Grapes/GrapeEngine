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

#include "PlaybackControls.h"
#include "services/Input.h"
#include "services/Time.h"
#include "ecs/World.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include <unordered_map>
#include <unordered_set>
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

// Save the current world into a JSON snapshot that preserves entity IDs.
// Instead of serializing to flat arrays, we save each entity's state by ID.
// This allows us to restore in-place without destroying/recreating entities.
void Playback::_saveWorldState() {
    if (!HasValidWorld()) return;

    LOG_INFO("Saving world state.");

    nlohmann::json worldJson = nlohmann::json::object();
    nlohmann::json entitiesMap = nlohmann::json::object(); // Changed from array to object
    
    size_t entityCount = 0;

    // Save all entities (except editor camera) with their IDs as keys
    m_world->Each([&](ECS::Entity entity) {
        // Skip editor camera
        if (m_world->Has<ECS::Components::CameraEditor3D>(entity)) {
            return;
        }

        // Serialize entity with all its components
        auto entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
        
        // Store by entity ID (as string key for JSON)
        std::string entityKey = std::to_string(entity.Index);
        entitiesMap[entityKey] = entityJson;
        ++entityCount;
    });

    worldJson["entities"] = entitiesMap;
    m_savedWorldState = worldJson;
    
    LOG_INFO("Saved " << entityCount << " entities with IDs preserved");
}

void Playback::_restoreWorldState() {
    // Restore the world from the previously saved snapshot.
    // This version preserves entity IDs by restoring component values in-place
    // instead of destroying and recreating entities.
    if (!HasValidWorld() || m_savedWorldState.is_null()) {
        LOG_WARNING("No saved state to restore");
        return;
    }

    // Check for new format (object with entities map)
    if (!m_savedWorldState.is_object() || !m_savedWorldState.contains("entities")) {
        LOG_ERROR("Saved world state has invalid format.");
        return;
    }

    LOG_INFO("Restoring world state.");

    const auto& entitiesMap = m_savedWorldState["entities"];
    
    // Track which entities existed in the snapshot
    std::unordered_set<uint32_t> snapshotEntityIds;
    for (auto it = entitiesMap.begin(); it != entitiesMap.end(); ++it) {
        uint32_t entityId = std::stoul(it.key());
        snapshotEntityIds.insert(entityId);
    }

    // First pass: Destroy any entities that were created during play mode
    // (entities that exist now but weren't in the snapshot)
    std::vector<ECS::Entity> entitiesToDestroy;
    m_world->Each([&](ECS::Entity entity) {
        // Skip editor camera
        if (m_world->Has<ECS::Components::CameraEditor3D>(entity)) {
            return;
        }
        
        // If this entity wasn't in the snapshot, it was created during play - destroy it
        if (snapshotEntityIds.find(entity.Index) == snapshotEntityIds.end()) {
            entitiesToDestroy.push_back(entity);
        }
    });

    for (auto entity : entitiesToDestroy) {
        m_world->Destroy(entity);
    }

    size_t restoredCount = 0;
    size_t recreatedCount = 0;

    // Second pass: Restore or recreate entities from snapshot
    for (auto it = entitiesMap.begin(); it != entitiesMap.end(); ++it) {
        uint32_t entityId = std::stoul(it.key());
        const auto& entityJson = it.value();
        
        ECS::Entity entity = m_world->Resolve(entityId);
        
        if (m_world->IsAlive(entity)) {
            // Entity still exists - restore its state in-place
            // This preserves the entity ID and prevents reference breakage
            _restoreEntityState(entity, entityJson);
            ++restoredCount;
        }
        else {
            // Entity was destroyed during play - recreate it with the same ID
            // This can happen if an entity was explicitly destroyed by game logic
            entity = _recreateEntityWithId(entityId, entityJson);
            if (m_world->IsAlive(entity)) {
                ++recreatedCount;
            }
        }
    }

    LOG_INFO("Restored " << restoredCount << " entities, recreated " << recreatedCount << " entities.");
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

// Helper: Restore an entity's state in-place from a JSON snapshot
void Playback::_restoreEntityState(ECS::Entity entity, const nlohmann::json& entityJson) {
    if (!entityJson.contains("Components") || !entityJson["Components"].is_array()) {
        return;
    }

    // Strategy: Deserialize each component from the snapshot
    // The EntitySerializer's deserializers use Set/Add appropriately
    
    const auto& componentsArray = entityJson["Components"];
    
    // Restore each component from the snapshot using the EntitySerializer registry
    for (const auto& componentJson : componentsArray) {
        if (!componentJson.contains("TypeName") || !componentJson.contains("Data")) {
            continue;
        }
        
        std::string typeName = componentJson["TypeName"];
        const auto& componentData = componentJson["Data"];
        
        // Use the EntitySerializer's registry to deserialize components
        // This leverages the existing REGISTER_COMPONENT_SERIALIZER infrastructure
        auto& registry = Serialization::EntitySerializer::Registry();
        for (const auto& [tid, info] : registry) {
            if (info.Name == typeName) {
                try {
                    // The deserializer will use Set or Add as appropriate
                    info.Deserialize(*m_world, entity, componentData);
                } 
                catch (const std::exception& ex) {
                    LOG_ERROR("Failed to restore component " << typeName << ": " << ex.what());
                }
                break;
            }
        }
    }
}

// Helper: Recreate an entity with a specific ID from a JSON snapshot
ECS::Entity Playback::_recreateEntityWithId(uint32_t targetId, const nlohmann::json& entityJson) {
    // This is a fallback for when an entity was destroyed during play
    // We need to create a new entity but try to reuse the same ID if possible
    
    // For now, just create a new entity and deserialize into it
    // The ID might be different, but this is rare (only if entity was destroyed)
    ECS::Entity newEntity = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);
    
    // Note: Ideally we'd want to reserve the specific ID, but the World class
    // doesn't expose that functionality. This is a limitation we can address later
    // by extending the World API with a CreateWithId() method.
    
    return newEntity;
}
