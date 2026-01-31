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
#include "services/TimeSystem.h"
#include "ecs/World.h"
#include "core/Logger.h"
#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "serialization/EntitySerializer.h"
#include "scripting/ScriptManager.h"
#include "helpers/EntityUtils.h"
#include "EditorECSUtils.h"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstring>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

// Forward declarations for managed component deserialization interop
extern "C" void WorldInterop_DeserializeComponentFromJson(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash, const char* jsonStr);

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
        if (m_editorState == EditorState::Edit) {
            // Simulate play button
            _saveWorldState();
            _changeState(EditorState::Play);
            LOG_INFO("Game started (Ctrl+P)");
        }
        else if (m_editorState == EditorState::Play || m_editorState == EditorState::Paused) {
            // Simulate stop button
            _restoreWorldState();
            _changeState(EditorState::Edit);
            LOG_INFO("Game stopped (Ctrl+P)");
        }
    }

    // Pause/Resume: Ctrl + Shift + P
    if (Input::IsKeyPressed(GLFW_KEY_P) && Input::IsKeyDown(GLFW_KEY_LEFT_CONTROL) &&
        Input::IsKeyDown(GLFW_KEY_LEFT_SHIFT))
    {
        if (m_editorState == EditorState::Play) {
            _changeState(EditorState::Paused);
            LOG_INFO("Game paused (Ctrl+Shift+P)");
        }
        else if (m_editorState == EditorState::Paused) {
            _changeState(EditorState::Play);
            LOG_INFO("Game resumed (Ctrl+Shift+P)");
        }
    }

    // Step: Alt + P
    if (Input::IsKeyPressed(GLFW_KEY_P) && Input::IsKeyDown(GLFW_KEY_LEFT_ALT)) {
        if (m_editorState == EditorState::Paused) {
            _changeState(EditorState::Step);
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
    auto button = [&](const char* icon, bool shouldBeEnabled, EditorState newState, const char* logMsg,
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

                // Show current state
                ImGui::Text("State: %s",
                            m_editorState == EditorState::Edit ? "EDIT" : m_editorState == EditorState::Paused ? "PAUSED"
                                                                                                             : m_editorState == EditorState::Play ? "PLAY" : "STEP");
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
                    if (newState == EditorState::Play && m_editorState == EditorState::Edit) {
                        _saveWorldState();
                    }
                    if (newState == EditorState::Edit) {
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
    // Shows play when in Edit or Paused state
    if (m_editorState == EditorState::Edit || m_editorState == EditorState::Paused || m_editorState == EditorState::Step) {
        ImGui::PushFont(m_symbolsFont);
        button("\xEE\x80\xB7", HasValidWorld(), EditorState::Play,
            m_editorState == EditorState::Edit ? "Game started" : "Game resumed", false,
            m_editorState == EditorState::Edit ? "Play (Ctrl+P)" : "Resume (Ctrl+Shift+P)");
        ImGui::PopFont();
    }
    // Shows pause when playing
    else {
        ImGui::PushFont(m_symbolsFont);
        button("\xEE\x80\xB4", HasValidWorld(), EditorState::Paused, "Game paused", false, "Pause (Ctrl+Shift+P)");
        ImGui::PopFont();
    }
    ImGui::SameLine();

    // STOP: playing/paused > edit
    ImGui::PushFont(m_symbolsFont);
    button("\xEE\x81\x87", m_editorState != EditorState::Edit, EditorState::Edit, "Game stopped", false, "Stop (Ctrl+P)");
    ImGui::PopFont();
    ImGui::SameLine();

    // Step button (for step-by-step physics)
    // Enabled when paused or stepping (so you can step multiple frames)
    ImGui::PushFont(m_symbolsFont);
    button("\xEE\x81\x84", m_editorState == EditorState::Paused || m_editorState == EditorState::Step, EditorState::Step, "Stepping 1 physics frame", true, "Step (Alt+P)");
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
        if (Editor::ECSUtils::HasComponent(m_world, entity, "CameraEditor3D")) {
            return;
        }

        // Serialize entity with all its components
        auto entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
        
        // Store by entity ID (as string key for JSON)
        std::string entityKey = std::to_string(entity.Index);
        entitiesMap[entityKey] = entityJson;
        ++entityCount;
    });

    // Save hierarchy relationships separately
    nlohmann::json hierarchyArray = nlohmann::json::array();
    m_world->Each([&](ECS::Entity entity) {
        if (Editor::ECSUtils::HasComponent(m_world, entity, "CameraEditor3D")) {
            return;
        }
        
        ECS::Entity parent = m_world->ParentOf(entity);
        if (!parent.IsNull() && m_world->IsAlive(parent)) {
            // Skip if parent is editor camera
            if (Editor::ECSUtils::HasComponent(m_world, parent, "CameraEditor3D")) {
                return;
            }
            
            nlohmann::json hierarchyEntry;
            hierarchyEntry["child"] = entity.Index;
            hierarchyEntry["parent"] = parent.Index;
            hierarchyArray.push_back(hierarchyEntry);
        }
    });

    worldJson["entities"] = entitiesMap;
    worldJson["hierarchy"] = hierarchyArray;
    m_savedWorldState = worldJson;
    
    LOG_INFO("Saved " << entityCount << " entities with hierarchy preserved");
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
        if (Editor::ECSUtils::HasComponent(m_world, entity, "CameraEditor3D")) {
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

    // Second pass: Detach all entities from hierarchy (will restore later)
    m_world->Each([&](ECS::Entity entity) {
        if (Editor::ECSUtils::HasComponent(m_world, entity, "CameraEditor3D")) {
            return;
        }
        m_world->Detach(entity);
    });

    size_t restoredCount = 0;
    size_t recreatedCount = 0;

    // Third pass: Restore or recreate entities from snapshot
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

    // Fourth pass: Restore hierarchy relationships
    if (m_savedWorldState.contains("hierarchy") && m_savedWorldState["hierarchy"].is_array()) {
        const auto& hierarchyArray = m_savedWorldState["hierarchy"];
        for (const auto& hierarchyEntry : hierarchyArray) {
            if (!hierarchyEntry.contains("child") || !hierarchyEntry.contains("parent")) {
                continue;
            }
            
            uint32_t childId = hierarchyEntry["child"].get<uint32_t>();
            uint32_t parentId = hierarchyEntry["parent"].get<uint32_t>();
            
            ECS::Entity child = m_world->Resolve(childId);
            ECS::Entity parent = m_world->Resolve(parentId);
            
            if (m_world->IsAlive(child) && m_world->IsAlive(parent)) {
                m_world->Attach(child, parent);
            }
        }
    }

    LOG_INFO("Restored " << restoredCount << " entities, recreated " << recreatedCount << " entities with hierarchy.");
}

// Internal state change handler that manages time scale and callbacks
void Playback::_changeState(EditorState newState) {
    if (m_editorState == newState) return;

    EditorState oldState = m_editorState;
    m_editorState = newState;

    // Handle time scale changes based on state
    switch (newState) {
    case EditorState::Edit:
        TimeSystem::Instance().SetTimeScale(1.0);
        break;
    case EditorState::Paused:
        TimeSystem::Instance().SetTimeScale(0.0);
        break;
    case EditorState::Play:
        TimeSystem::Instance().SetTimeScale(1.0);
        break;
    case EditorState::Step:
        TimeSystem::Instance().SetTimeScale(0.0);  // Don't advance time, single frame only
        break;
    }

    // Notify external listeners about state change
    if (m_onStateChanged) {
        m_onStateChanged(oldState, newState);
    }
}

// Register callback for state change events
void Playback::OnStateChanged(std::function<void(EditorState, EditorState)> callback) {
    m_onStateChanged = callback;
}

// Query: Is the game currently in the Playing state?
bool Playback::IsPlaying() const {
    return m_editorState == EditorState::Play;
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

// Get current editor state
EditorState Playback::GetEditorState() const {
    return m_editorState;
}

// Update the world reference safely when scenes change
// Update the bound world reference used by playback operations.
// Clears saved snapshot since it no longer matches the world.
void Playback::SetWorld(ECS::World* world) {
    m_world = world;
    m_editorState = EditorState::Edit;
    m_stepRequested = false;
    m_savedWorldState.clear();
}

// Helper: Restore an entity's state in-place from a JSON snapshot
void Playback::_restoreEntityState(ECS::Entity entity, const nlohmann::json& entityJson) {
    if (!entityJson.contains("Components") || !entityJson["Components"].is_array()) {
        return;
    }

    // Helper to normalize type names by stripping ECS::Components:: prefix
    auto normalizeTypeName = [](const std::string& typeName) {
        constexpr const char* kPrefix = "ECS::Components::";
        if (typeName.rfind(kPrefix, 0) == 0) {
            return typeName.substr(std::strlen(kPrefix));
        }
        return typeName;
    };

    const auto& componentsArray = entityJson["Components"];
    
    // Build a set of ComponentTypeIds that should exist after restore
    std::unordered_set<ECS::ComponentTypeId> snapshotComponentIds;
    auto& registry = Serialization::EntitySerializer::Registry();
    
    // Map component names from snapshot to ComponentTypeIds
    for (const auto& comp : componentsArray) {
        if (comp.contains("TypeName")) {
            std::string typeName = comp["TypeName"].get<std::string>(); // Original type name from snapshot
            std::string normalizedTypeName = normalizeTypeName(typeName); // Normalized name without prefix
            uint32_t hash = Editor::ECSUtils::FNV1aHash(normalizedTypeName.c_str()); // Compute hash
            ECS::ComponentTypeId id = ECS::ComponentRegistry::GetComponentIdFromHash(hash); // Lookup ID by hash
            if (id != ECS::NULL_COMPONENT_ID) {
                snapshotComponentIds.insert(id); // Track that this component should exist
            }
        }
    }
    
    // Get entity's current archetype to see what components it has
    const auto* location = m_world->LocationOf(entity);
    if (location && location->ArchetypePtr) {
        const auto& currentComponents = location->ArchetypePtr->GetComponents();
        
        // Check each component on the entity to see if it should be removed
        std::vector<ECS::ComponentTypeId> componentsToRemove;
        for (const auto& compInfo : currentComponents) {
            // Skip if this component is in the snapshot (we want to keep it)
            if (snapshotComponentIds.find(compInfo.Id) != snapshotComponentIds.end()) {
                continue;
            }
            
            // Note: Hierarchy relationships (Parent component) are handled separately
            // in the restore process, so we don't need to explicitly skip them here
            
            // This component exists on entity but not in snapshot - mark for removal
            componentsToRemove.push_back(compInfo.Id);
        }
        
        // Remove components that exist on entity but not in snapshot
        for (ECS::ComponentTypeId id : componentsToRemove) {
            m_world->RemoveById(entity, id);
        }
    }
    
    // Restore each component from the snapshot using the EntitySerializer registry
    for (const auto& componentJson : componentsArray) {
        if (!componentJson.contains("TypeName") || !componentJson.contains("Data")) {
            continue;
        }
        
        std::string typeName = componentJson["TypeName"];
        std::string normalizedTypeName = normalizeTypeName(typeName);
        const auto& componentData = componentJson["Data"];
        
        // First, try to deserialize using the C++ EntitySerializer registry
        bool foundInCppRegistry = false;
        for (const auto& [typeHash, info] : registry) {
            if (info.Name == typeName || info.Name == normalizedTypeName) {
                try {
                    // The deserializer will use Set or Add as appropriate
                    info.Deserialize(*m_world, entity, componentData);
                } 
                catch (const std::exception& ex) {
                    LOG_ERROR("Failed to restore component " << typeName << ": " << ex.what());
                }
                foundInCppRegistry = true;
                break;
            }
        }
        
        // If not found in C++ registry, try managed components via interop
        if (!foundInCppRegistry) {
            // Look up the component in the native ComponentRegistry
            auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
            for (ECS::ComponentTypeId id : allIds) {
                const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
                if (nativeMeta.TypeHash == 0 || !nativeMeta.IsManaged) continue;
                
                std::string nativeName = ECS::ComponentRegistry::GetComponentNameFromHash(nativeMeta.TypeHash);
                if (nativeName == typeName || nativeName == normalizedTypeName) {
                    // Found the managed component by name
                    // Check if the component exists on the entity
                    if (!m_world->HasById(entity, id)) {
                        // Component doesn't exist, add it with zero-initialized data
                        std::vector<uint8_t> buffer(nativeMeta.Size, 0);
                        m_world->AddComponentById(entity, id, buffer.data(), nativeMeta.Size);
                    }
                    
                    // Deserialize the JSON into the component via interop
                    std::string jsonStr = componentData.dump();
                    try {
                        WorldInterop_DeserializeComponentFromJson(
                            m_world,
                            ECS::EntityUtils::Pack(entity),
                            nativeMeta.TypeHash,
                            jsonStr.c_str()
                        );
                        LOG_DEBUG("Restored managed component " << typeName << " on entity " << entity.Index);
                    }
                    catch (const std::exception& ex) {
                        LOG_ERROR("Failed to deserialize managed component " << typeName << ": " << ex.what());
                    }
                    break;
                }
            }
        }
    }
}

// Helper: Recreate an entity with a specific ID from a JSON snapshot
ECS::Entity Playback::_recreateEntityWithId(uint32_t targetId, const nlohmann::json& entityJson) {
    // This is a fallback for when an entity was destroyed during play
    // Use CreateWithId to preserve the exact entity ID from the snapshot
    
    ECS::Entity newEntity = m_world->CreateWithId(targetId);
    
    // Deserialize components into the newly created entity
    if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
        auto normalizeTypeName = [](const std::string& typeName) {
            constexpr const char* kPrefix = "ECS::Components::";
            if (typeName.rfind(kPrefix, 0) == 0) {
                return typeName.substr(std::strlen(kPrefix));
            }
            return typeName;
        };

        auto& registry = Serialization::EntitySerializer::Registry();
        for (const auto& comp : entityJson["Components"]) {
            if (!comp.contains("TypeName") || !comp.contains("Data")) {
                continue;
            }
            
            std::string typeName = comp["TypeName"].get<std::string>();
            std::string normalizedTypeName = normalizeTypeName(typeName);
            const auto& componentData = comp["Data"];
            
            // First, try to deserialize using the C++ EntitySerializer registry
            bool foundInCppRegistry = false;
        for (const auto& [typeHash, info] : registry) {
            if (info.Name == typeName || info.Name == normalizedTypeName) {
                try {
                    info.Deserialize(*m_world, newEntity, componentData);
                    } 
                    catch (const std::exception& ex) {
                        LOG_ERROR("Failed to restore component " << typeName << ": " << ex.what());
                    }
                    foundInCppRegistry = true;
                    break;
                }
            }
            
            // If not found in C++ registry, try managed components via interop
            if (!foundInCppRegistry) {
                // Look up the component in the native ComponentRegistry
                auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
                for (ECS::ComponentTypeId id : allIds) {
                    const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
                    if (nativeMeta.TypeHash == 0 || !nativeMeta.IsManaged) continue;
                    
                    std::string nativeName = ECS::ComponentRegistry::GetComponentNameFromHash(nativeMeta.TypeHash);
                    if (nativeName == typeName || nativeName == normalizedTypeName) {
                        // Found the managed component by name
                        // Add component with zero-initialized data
                        std::vector<uint8_t> buffer(nativeMeta.Size, 0);
                        m_world->AddComponentById(newEntity, id, buffer.data(), nativeMeta.Size);
                        
                        // Deserialize the JSON into the component via interop
                        std::string jsonStr = componentData.dump();
                        try {
                            WorldInterop_DeserializeComponentFromJson(
                                m_world,
                                ECS::EntityUtils::Pack(newEntity),
                                nativeMeta.TypeHash,
                                jsonStr.c_str()
                            );
                            LOG_DEBUG("Restored managed component " << typeName << " on recreated entity " << newEntity.Index);
                        }
                        catch (const std::exception& ex) {
                            LOG_ERROR("Failed to deserialize managed component " << typeName << ": " << ex.what());
                        }
                        break;
                    }
                }
            }
        }
    }
    
    return newEntity;
}
