/* Start Header *****************************************************************/
/*!
\file   InspectorPanel.cpp
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   15th November 2025

\brief
Implementation of the editor panel for viewing and editing game entities and
prefab assets.

This file contains the runtime logic for the inspector panel including
selection handling, JSON conversion, component rendering, property editing,
component addition and removal, prefab creation, prefab synchronization and
updating linked instances. All component UI is forwarded to ComponentWidgets
through a unified system shared by both entities and prefab templates.
*/
/* End Header *******************************************************************/

#include "InspectorPanel.h"
#include "ComponentPropertyEditor.h"
#include "ComponentWidgets.h"
#include "EditorComponentRegistry.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include "EditorFileMenu.h"
#include "core/ProjectPaths.h"
#include "UndoSystem.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "EditorStyle.h"

namespace {
    // Helper template function to safely add components during deserialization
    // Checks if the component type matches expected name before adding
    template <typename T>
    bool AddComponentIfMatch(ECS::World* world, ECS::Entity instance, const std::string& typeName,
        const std::string& expectedName, nlohmann::json& compData)
    {
        // If the json component name does not match what this function handles we skip it
        // This avoids adding the wrong component type to the entity
        if (typeName != expectedName) return false;

        // Only add the component if the entity does not already have it
        // world Has<T> checks if this entity already contains a component of type T
        if (!world->Has<T>(instance)) {
            // Add<T> attaches the component to the entity and returns a reference to it
            auto& c = world->Add<T>(instance);
            // from_json fills the new component using values from the json 
            // This allows prefabs and saved scenes to restore component state exactly
            from_json(compData, c);
        }
        // true means this template handled the component successfully
        return true;
    }

    // Helper to check if an entity ID is protected from editing
    // Returns true if entity should NOT be inspected or modified
    bool IsProtectedEntity(ECS::World* world, EntityId entityId) {
        if (!world)
            return false;
        
        // Resolve the entity from its ID
        ECS::Entity entity = world->Resolve(entityId);

        // Not alive, cannot be protected
        if (!world->IsAlive(entity))
            return false;

        // Protect editor cameras from modification
        if (world->Has<ECS::Components::CameraEditor3D>(entity))
            return true;

        return false;
    }

    void MarkSceneDirtyIfNeeded(EditorFileMenu* fileMenu) {
        if (fileMenu) {
            fileMenu->MarkSceneDirty();
        }
    }
}

// -------------------------------------------------------------------------
// Lifecycle Management
// -------------------------------------------------------------------------

// Set up fonts and world pointer so the inspector can render UI and talk to ECS
void InspectorPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;

    // Forward fonts into the smaller ComponentUI helper so it can draw fields
    m_componentUI.Initialize(mainFont, boldFont, symbolsFont);
}

// Update the world context and clear any stale selection from a previous world
void InspectorPanel::SetWorld(ECS::World* world) {
    m_world = world;
    ClearSelection();
}

// -------------------------------------------------------------------------
// Selection Management
// -------------------------------------------------------------------------

// Switch inspector into entity mode and validate the entity we want to inspect
void InspectorPanel::InspectEntity(EntityId id) {
    // Block inspection of protected system entities
    if (IsProtectedEntity(m_world, id)) {
        m_mode = InspectionMode::None;
        m_entityId = 0; // Clear selection
        return;
    }

    m_entityId = id;

    // If we do not have a world there is nothing to inspect
    if (!m_world) {
        m_mode = InspectionMode::None;
        return;
    }

    // Wrap the ID into an ECS::Entity handle (use Resolve() instead of hardcoding generation 0)
    ECS::Entity e = m_world->Resolve(id);
    if (!m_world->IsAlive(e)) {
        // Entity might have been deleted so we reset the mode
        m_mode = InspectionMode::None;
        return;
    }

    // Ensure all entities have Transform component (mandatory)
    // We use the registry to check and auto add defaults when missing
    const auto* transformMeta = ComponentRegistryUI::Find("LocalTransform");
    if (transformMeta && !transformMeta->HasComponent(m_world, e)) {
        transformMeta->AddComponent(m_world, e, transformMeta->GetDefaults());
    }

    m_mode = InspectionMode::Entity;
}

// Load a prefab file from disk and switch into prefab editing mode
void InspectorPanel::InspectPrefab(const std::string& path) {
    // Usual checks
    if (path.empty()) {
        m_statusMessage = "Failed: No prefab path";
        m_statusTimer = 3.0f;
        return;
    }

    // Try to open the prefab JSON file
    std::ifstream file(path);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot open prefab";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot open prefab file: " << path);
        return;
    }

    try {
        // Read entire file into a string so we can detect empty files
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Handle empty files
        // If the file is blank we create a prefab that at least has a Transform
        if (content.empty() || content.find_first_not_of(" \t\n\r") == std::string::npos) {
            const auto* transformMeta = ComponentRegistryUI::Find("LocalTransform");
            nlohmann::json defaultTransform = transformMeta ? transformMeta->GetDefaults() : nlohmann::json::object();

            // Build a minimal prefab JSON structure with a single Transform component
            m_prefabData = nlohmann::json{
                {"Components", nlohmann::json::array({
                    {{"TypeName", "ECS::Components::LocalTransform"}, {"Data", defaultTransform}}
                })}
            };

            // The file was empty so we are generating a new prefab structure
            // Set lastSavedPrefabHash to 0 so the next save is forced
            // This establishes a valid on-disk baseline for hash comparison
            m_prefabPath = path;
            m_lastSavedPrefabHash = 0;
            m_mode = InspectionMode::Prefab;

            // Immediately write this new prefab back to disk
            _savePrefabData();

            m_statusMessage = "Opened empty prefab, added Transform";
            m_statusTimer = 2.0f;
            return;
        }

        // Non empty file path
        // Parse JSON and set up state for editing
        m_prefabData = nlohmann::json::parse(content);
        m_prefabPath = path;

        /*
        Take the entire prefab JSON, convert it to a string
        Hash the string (turn it into a number), then store that number

        This hash acts like a fingerprint of the prefab at the moment we saved it
        Next time we try to save, we compute a new hash and compare it with this one
        If the hashes match, it means nothing changed, so we skip rewriting the file
        If they differ, it means the prefab was modified, so we save again
        */
        m_lastSavedPrefabHash = std::hash<std::string>{}(m_prefabData.dump());
        m_mode = InspectionMode::Prefab;
    }
    catch (const std::exception& e) {
        // Any parse or other exception means the JSON is invalid
        m_statusMessage = "Failed: Invalid JSON in prefab";
        m_statusTimer = 3.0f;
        m_mode = InspectionMode::None;
        LOG_ERROR("Failed to parse prefab JSON: " << e.what());
    }
}

// Reset inspector so nothing is selected and state is clean
void InspectorPanel::ClearSelection() {
    m_mode = InspectionMode::None;
    m_entityId = 0;
    m_prefabPath.clear();
    m_prefabData = {};
    m_componentsToDelete.clear();
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

// Draw the inspector window based on current mode (none, entity, prefab)
void InspectorPanel::Render() {
    // Use main editor font for the whole inspector window
    ImGui::PushFont(m_mainFont);

    // Window name changes depending on what we are editing
    const char* windowTitle = (m_mode == InspectionMode::Prefab) ? "Prefab Editor" : "Property Editor";
    ImGui::Begin(windowTitle);

    if (m_mode == InspectionMode::None) {
        ImGui::TextDisabled("No selection");
    }
    else if (m_mode == InspectionMode::Entity) {
        _renderEntityInspector();
    }
    else if (m_mode == InspectionMode::Prefab) {
        _renderPrefabInspector();
    }

    // Always draw status bar at the bottom to show feedback messages
    _renderStatusBar();

    ImGui::End();
    ImGui::PopFont();
}

// -------------------------------------------------------------------------
// Entity Inspector Implementation
// -------------------------------------------------------------------------

// Top level entry for entity mode
// Draws header, components and Add Component area for the selected entity
void InspectorPanel::_renderEntityInspector() {
    if (!m_world) {
        ImGui::TextDisabled("No entity selected");
        return;
    }

    ECS::Entity entity = m_world->Resolve(m_entityId);
    if (!m_world->IsAlive(entity)) {
        ImGui::TextDisabled("Entity invalid");
        return;
    }

    _renderEntityHeader(entity);
    _renderEntityComponents(entity);
    _renderAddComponentButton(entity);
}

// Draw entity name, ID and prefab link information at the top of the inspector
void InspectorPanel::_renderEntityHeader(ECS::Entity entity) {
    // Try to read the Name component for display
    const char* entityName = "Unnamed";
    if (m_world->Has<ECS::Components::Name>(entity)) {
        entityName = m_world->Get<ECS::Components::Name>(entity).Value;
    }

    // Show the basic header line: Entity <Name> (ID)
    ImGui::Text("Entity ");
    ImGui::SameLine();
    ImGui::TextDisabled("%s (ID: %u)", entityName, (unsigned)m_entityId);

    // If this entity came from a prefab show the link and an Open Prefab button
    if (m_world->Has<ECS::Components::PrefabLink>(entity)) {
        const auto& link = m_world->Get<ECS::Components::PrefabLink>(entity);
        ImGui::Separator();
        ImGui::Text("Prefab Instance");

        // Show just the filename (not the full path)
        ImGui::SameLine();
        ImGui::TextDisabled("%s", std::filesystem::path(link.prefabPath).filename().string().c_str());

        // Button to open the original prefab template for editing
        ImGui::SameLine();
        if (ImGui::Button("Open Prefab")) {
            // Switch inspector into prefab mode using the linked path
            InspectPrefab(link.prefabPath);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Opens the prefab template file for editing");
        }

        ImGui::Separator();
    }
    // If NOT a prefab instance, show an empty prefab slot with drag - drop
    else {
        ImGui::Separator();
        ImGui::Text("Prefab Link");
        ImGui::SameLine();
        ImGui::TextDisabled("None (drag .prefab here to link)");

        // Allow drag and drop of assets onto this row
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                // payload->Data is a char* containing the file path
                std::string droppedPath = static_cast<const char*>(payload->Data);

                // Only allow .prefab files to be dropped here
                if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                    // Add a PrefabLink component so we can later sync it
                    m_world->Add<ECS::Components::PrefabLink>(entity, droppedPath);
                    m_statusMessage = "Prefab linked to entity";
                    m_statusTimer = 2.0f;
                }
                else {
                    m_statusMessage = "Not a prefab: drop a .prefab file";
                    m_statusTimer = 2.0f;
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
                const char* data = static_cast<const char*>(payload->Data);
                const char* end = data + payload->DataSize;
                while (data < end) {
                    std::string path(data);
                    data += path.size() + 1;
                    if (path.empty()) continue;
                    if (std::filesystem::path(path).extension() != ".prefab") continue;
                    m_world->Add<ECS::Components::PrefabLink>(entity, path);
                    m_statusMessage = "Prefab linked to entity";
                    m_statusTimer = 2.0f;
                    break;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::Separator();
    }
}

// Render all components on an entity using JSON as a temporary editable buffer
void InspectorPanel::_renderEntityComponents(ECS::Entity entity) {
    // Footer needs 2 lines: one for button row, one for status message
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    // Use smaller padding inside the scrolling region for components
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("EntityComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Convert the entity into JSON so we can use the same UI path as prefabs
    // UI edits this JSON, then we sync the edits back into the ECS
    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

    // Make sure the JSON has a component list we can iterate
    if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
        ImGui::Dummy(ImVec2(0, 4));

        bool wasEdited = false;

        static nlohmann::json editStartState;
        static bool isEditing = false;

        // Capture initial state when starting to edit
        if (!m_editState.isEditing) {
            // Check if any ImGui widget is active (isit being edited?)
            if (ImGui::IsAnyItemActive()) {
                m_editState.isEditing = true;
                m_editState.entityId = entity.Index;

                // Capture initial transform state
                if (m_world->Has<ECS::Components::LocalTransform>(entity)) {
                    const auto& lt = m_world->Get<ECS::Components::LocalTransform>(entity);
                    m_editState.startPosition = lt.Position;
                    m_editState.startRotation = lt.Rotation;
                    m_editState.startScale = lt.Scale;
                }
            }
        }

        // First pass: draw every component using registry metadata
        for (auto& componentEntry : entityJson["Components"]) {
            // Basic validation
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
            if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;
            std::string typeName = componentEntry["TypeName"];

            // Look up metadata which tells us how to draw this component
            const auto* meta = ComponentRegistryUI::Find(typeName);
            if (meta) {
                auto& data = componentEntry["Data"];

                size_t hashBefore = std::hash<std::string>{}(data.dump());

                // UI renderer callback: InspectorPanel calls this and forwards to ComponentWidgets
                // via the ComponentUI helper to draw the actual fields
                _renderComponentSection(meta->DisplayName, meta->TypeName, data,
                    [this, meta](nlohmann::json& d) { meta->RenderUI(m_componentUI, d); }, meta->CanDelete);

                size_t hashAfter = std::hash<std::string>{}(data.dump());

                if (hashBefore != hashAfter) {
                    if (!isEditing) {
                        editStartState = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
                        isEditing = true;
                    }
                    wasEdited = true;
                }

                ImGui::Dummy(ImVec2(0, 4));
            }
        }

        // Process deferred deletions after UI loop so we don't mutate while iterating
        // Also remove deleted components from the JSON buffer so they are not re-applied below
        for (const auto& type : m_componentsToDelete) {
            // Pull out Components array (early-continue instead of nesting)
            if (!entityJson.contains("Components")) continue;
            if (!entityJson["Components"].is_array()) continue;

            auto& comps = entityJson["Components"];

            // Manual iterator loop because we may erase while iterating
            for (auto it = comps.begin(); it != comps.end(); ) {
                // Bail early if no valid TypeName
                bool hasTypeName = it->contains("TypeName") && (*it)["TypeName"].is_string();
                if (!hasTypeName) {
                    it++;
                    continue;
                }

                std::string tn = (*it)["TypeName"];

                // Check short or fully-qualified names
                bool matches = (tn == type) || (tn == "ECS::Components::" + type);
                if (matches) {
                    it = comps.erase(it);   // Erase returns next iterator
                    continue;               // Do not increment manually
                }

                // Nothing erased
                it++;
            }

            // Remove actual ECS component last
            _removeComponentFromEntity(type);
        }

        m_componentsToDelete.clear();

        // Second pass: push any edited JSON values back into ECS components
        for (const auto& componentEntry : entityJson["Components"]) {
            // Validate again; same same
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
            if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

            std::string typeName = componentEntry["TypeName"];
            const auto* meta = ComponentRegistryUI::Find(typeName);
            // Apply edited JSON to the actual ECS component
            if (meta) {
                meta->ApplyToEntity(m_world, entity, componentEntry["Data"]);
            }
        }

        // Record undo when editing finishes
        if (m_editState.isEditing && !ImGui::IsAnyItemActive()) {
            // Editing just finished - record the change
            if (m_undoSystem && m_world->Has<ECS::Components::LocalTransform>(entity)) {
                const auto& lt = m_world->Get<ECS::Components::LocalTransform>(entity);

                // Only record if something actually changed
                bool posChanged = (m_editState.startPosition != lt.Position);
                bool rotChanged = (m_editState.startRotation != lt.Rotation);
                bool scaleChanged = (m_editState.startScale != lt.Scale);

                if (posChanged || rotChanged || scaleChanged) {
                    m_undoSystem->RecordTransformChange(
                        entity.Index,
                        m_editState.startPosition, m_editState.startRotation, m_editState.startScale,
                        lt.Position, lt.Rotation, lt.Scale
                    );
                    LOG_DEBUG("[Inspector] Recorded transform change for undo");
                }
            }

            m_editState.isEditing = false;
        }

        // MARK SCENE AS DIRTY if anything was edited
        if (wasEdited) {
            MarkSceneDirtyIfNeeded(m_fileMenu);
        }
    }


    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// Renders the Add Component button row at the bottom of the inspector
void InspectorPanel::_renderAddComponentButton(ECS::Entity entity) {
    // Button to open the Add Component popup menu
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }

    // Button to save this entity and its components as a prefab
    ImGui::SameLine();
    if (ImGui::Button("Save as Prefab")) {
        _saveEntityAsPrefab(entity);
    }

    // Popup menu listing all available components from the registry
    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

        // Get registry and create sorted list
        const auto& registry = ComponentRegistryUI::GetAll();
        std::vector<size_t> sortedIndices;
        for (size_t i = 0; i < registry.size(); ++i) {
            sortedIndices.push_back(i);
        }

        // Sort alphabetically by DisplayName
        std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b) {
            return registry[a].DisplayName < registry[b].DisplayName;
            });

        // Iterate over sorted components
        for (size_t idx : sortedIndices) {
            const auto& meta = registry[idx];

            // Check if the entity already has this component
            bool hasComponent = meta.HasComponent(m_world, entity);

            // If entity already has the component, disable this menu entry
            if (hasComponent) {
                ImGui::BeginDisabled();
            }

            // Draw the menu item (internal helper attaches the component)
            _renderComponentMenuItem(meta.DisplayName.c_str(), meta.TypeName.c_str());

            // Re-enable UI after drawing a disabled item
            if (hasComponent) {
                ImGui::EndDisabled();
            }
        }
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------------------
// Prefab Inspector Implementation
// -------------------------------------------------------------------------

// Top level entry for prefab mode
// Draws prefab header, components and prefab specific actions
void InspectorPanel::_renderPrefabInspector() {
    _renderPrefabHeader();
    _renderPrefabComponents();
    _renderPrefabActions();
}

// Show which prefab file we are editing and warn that it affects all instances
void InspectorPanel::_renderPrefabHeader() {
    ImGui::Text("Editing Template");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", std::filesystem::path(m_prefabPath).filename().string().c_str());

    ImGui::Separator();
    ImGui::TextWrapped("Changes to this prefab will auto-save and update ALL instances");
    ImGui::Separator();
}

// Render all component definitions stored inside the prefab JSON
void InspectorPanel::_renderPrefabComponents() {
    // Footer needs 2 lines: one for buttons, one for status message
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    // Use smaller padding inside the scrolling region for components
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("PrefabComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Prefab JSON must have a Components array or there is nothing to draw
    if (!m_prefabData.contains("Components") || !m_prefabData["Components"].is_array()) {
        ImGui::TextDisabled("No components in prefab");
        ImGui::EndChild();
        ImGui::PopStyleVar();
        return;
    }

    ImGui::Dummy(ImVec2(0, 4));
    auto& components = m_prefabData["Components"];

    // SORT COMPONENTS: Transform first, then alphabetical by TypeName
    // Create a sorted list of indices so we don't modify the actual JSON array order
    std::vector<size_t> sortedIndices;
    for (size_t i = 0; i < components.size(); i++) {
        sortedIndices.push_back(i);
    }

    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b) {
        // Get type names for comparison
        std::string typeA = components[a].value("TypeName", "");
        std::string typeB = components[b].value("TypeName", "");

        // Helper to identify Name
        auto isName = [](const std::string& type) {
            return (type == "ECS::Components::Name" || type == "Name");
        };

        // Helper to identify Transform
        auto isTransform = [](const std::string& type) {
            return (type == "ECS::Components::LocalTransform" || type == "LocalTransform");
        };

        bool aIsName = isName(typeA);
        bool bIsName = isName(typeB);
        bool aIsTransform = isTransform(typeA);
        bool bIsTransform = isTransform(typeB);

        // Transform always first
        if (aIsTransform && !bIsTransform) return true;
        if (!aIsTransform && bIsTransform) return false;
        if (aIsTransform && bIsTransform) return false;

        // Name always second
        if (aIsName && !bIsName) return true;
        if (!aIsName && bIsName) return false;
        if (aIsName && bIsName) return false; 

        // Strip "ECS::Components::" prefix for cleaner alphabetical sorting
        auto stripPrefix = [](const std::string& name) -> std::string {
            const std::string prefix = "ECS::Components::";
            if (name.find(prefix) == 0) {
                return name.substr(prefix.length());
            }
            return name;
        };

        // Everything else alphabetical
        return stripPrefix(typeA) < stripPrefix(typeB);
    });

    // Draw each component in sorted order using metadata rules
    for (size_t idx : sortedIndices) {
        auto& componentEntry = components[idx];

        // Validate each JSON entry
        if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
        if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

        std::string typeName = componentEntry["TypeName"];
        const auto* meta = ComponentRegistryUI::Find(typeName);
        if (!meta) continue;

        // Render a collapsible UI section for this component
        // After any edit we call _savePrefabData so the file updates immediately
        auto& data = componentEntry["Data"];
        _renderComponentSection(meta->DisplayName, meta->TypeName, data,
            // UI renderer callback: InspectorPanel forwards JSON to ComponentWidgets
            // Prefabs do not have live ECS so we save as soon as data changes
            [this, meta](nlohmann::json& d) { meta->RenderUI(m_componentUI, d); _savePrefabData(); },
            meta->CanDelete
        );
        ImGui::Dummy(ImVec2(0, 4));
    }

    // Process deferred deletions
    for (const auto& componentType : m_componentsToDelete) {
        _removeComponentFromPrefab(componentType);
    }
    m_componentsToDelete.clear();

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// Renders the action row shown when editing a prefab
void InspectorPanel::_renderPrefabActions() {
    ImGui::Separator();

    // Button to open the Add Component popup for prefabs
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }

    // Popup list of components that can be added to the prefab
    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

        // Get registry and create sorted list
        const auto& registry = ComponentRegistryUI::GetAll();
        std::vector<size_t> sortedIndices;
        for (size_t i = 0; i < registry.size(); ++i) {
            sortedIndices.push_back(i);
        }

        // Sort alphabetically by DisplayName
        std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b) {
            return registry[a].DisplayName < registry[b].DisplayName;
            });

        // Iterate over sorted components
        for (size_t idx : sortedIndices) {
            const auto& meta = registry[idx];

            // Check if the prefab already defines this component
            bool hasComponent = _prefabHasComponent(meta.TypeName);

            // Disable menu entries for components that already exist in prefab
            if (hasComponent) {
                ImGui::BeginDisabled();
            }

            // Draw the menu item and handle adding the component when clicked
            _renderComponentMenuItem(meta.DisplayName.c_str(), meta.TypeName.c_str());

            if (hasComponent) {
                ImGui::EndDisabled();
            }
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // This pushes all current prefab data onto every instance in the scene
    if (ImGui::Button("Apply to All Instances")) {
        _applyPrefabToInstances();
    }
}

// -------------------------------------------------------------------------
// Component Menu Management
// -------------------------------------------------------------------------

// Draws a single menu item inside the Add Component popup
void InspectorPanel::_renderComponentMenuItem(const char* displayName, const char* componentType) {
    if (ImGui::MenuItem(displayName)) {

        // If we are editing a live entity
        if (m_mode == InspectionMode::Entity) {
            _addComponentToEntity(componentType);
        }
        // If we are editing a prefab template
        else if (m_mode == InspectionMode::Prefab) {
            _addComponentToPrefab(componentType);
        }
    }
}

// -------------------------------------------------------------------------
// Entity Component Management
// -------------------------------------------------------------------------

// Attach a new component to the currently selected entity
// We use metadata to create default JSON for the component and then apply it to the ECS
void InspectorPanel::_addComponentToEntity(const std::string& componentType) {
    if (!m_world) return;
    ECS::Entity entity = m_world->Resolve(m_entityId);
    if (!m_world->IsAlive(entity)) return;

    // Shape components are mutually exclusive
    if (componentType == "ShapeCircle2D" || componentType == "ShapeBox2D" || componentType == "ShapeLine2D") {
        _removeComponentFromEntity("ShapeCircle2D");
        _removeComponentFromEntity("ShapeBox2D");
        _removeComponentFromEntity("ShapeLine2D");
    }

    // Look up this component's metadata so we know how to create it
    const auto* meta = ComponentRegistryUI::Find(componentType);
    if (meta) {
        // Add to ECS with default JSON values
        meta->AddComponent(m_world, entity, meta->GetDefaults());
        m_statusMessage = std::string("Added ") + componentType;
        m_statusTimer = 3.0f;

        // MARK SCENE AS DIRTY
        MarkSceneDirtyIfNeeded(m_fileMenu);
    }
}

// Remove a component from the entity unless it is the Transform which must always exist
void InspectorPanel::_removeComponentFromEntity(const std::string& componentType) {
    if (!m_world) return;
    ECS::Entity entity = m_world->Resolve(m_entityId);
    if (!m_world->IsAlive(entity)) return;

    // Transform cannot be removed
    if (componentType == "LocalTransform") return;

    // // Look up this component's metadata
    const auto* meta = ComponentRegistryUI::Find(componentType);
    if (meta) {
        // Remove from ECS
        meta->RemoveComponent(m_world, entity);
        m_statusMessage = std::string("Removed ") + componentType;
        m_statusTimer = 3.0f;

        // MARK SCENE AS DIRTY
        MarkSceneDirtyIfNeeded(m_fileMenu);
    }
}

// Check whether an entity has a component using metadata rules
bool InspectorPanel::_entityHasComponent(EntityId id, const std::string& componentType) {
    ECS::Entity entity = m_world->Resolve(id);
    if (!m_world->IsAlive(entity)) return false;

    // Self-explanatory
    const auto* meta = ComponentRegistryUI::Find(componentType);
    return meta ? meta->HasComponent(m_world, entity) : false;
}

// -------------------------------------------------------------------------
// Prefab Component Management
// -------------------------------------------------------------------------

// Add a component entry to the prefab JSON
// Prefabs are stored and edited entirely through JSON so we modify the data directly
void InspectorPanel::_addComponentToPrefab(const std::string& componentType) {
    // Ensure Components array exists
    if (!m_prefabData.contains("Components")) {
        m_prefabData["Components"] = nlohmann::json::array();
    }

    // No duplicates allowed
    if (_prefabHasComponent(componentType)) return;

    // Shape components are mutually exclusive
    if (componentType == "ShapeCircle2D" || componentType == "ShapeBox2D" || componentType == "ShapeLine2D") {
        _removeComponentFromPrefab("ShapeCircle2D");
        _removeComponentFromPrefab("ShapeBox2D");
        _removeComponentFromPrefab("ShapeLine2D");
    }

    // Look up metadata for defaults and full type name
    const auto* meta = ComponentRegistryUI::Find(componentType);
    if (!meta) return;

    // Append new component entry into prefab JSON
    m_prefabData["Components"].push_back({ {"TypeName", meta->FullTypeName}, {"Data", meta->GetDefaults()} });

    // Prefab data changed so we sync the file right away
    _savePrefabData();
}

// Removes a component entry from the prefab JSON
// Prefabs store components as JSON objects so we search the Components array by TypeName
// Some entries store the short name while others store the fully qualified ECS type so we check for both
void InspectorPanel::_removeComponentFromPrefab(const std::string& componentType) {
    // Prefab must have a valid Components array
    if (!m_prefabData.contains("Components") || !m_prefabData["Components"].is_array()) return;
    auto& components = m_prefabData["Components"];

    // Iterate over each component entry to find a matching TypeName
    for (auto it = components.begin(); it != components.end(); it++) {
        // Validate entry
        if (!(*it).contains("TypeName") || !(*it)["TypeName"].is_string()) continue;

        // Match short name or fully qualified name
        std::string typeName = (*it)["TypeName"];
        if (typeName == componentType || typeName == "ECS::Components::" + componentType) {
            // Remove the component from the prefab JSON
            components.erase(it);
            m_statusMessage = std::string("Removed ") + componentType;
            m_statusTimer = 2.0f;
            // Prefab changed so we save immediately
            _savePrefabData();
            return;
        }
    }
}

// Checks whether the prefab JSON already contains a component of this type
bool InspectorPanel::_prefabHasComponent(const std::string& componentType) {
    // Must have a Components array to search
    if (!m_prefabData.contains("Components") || !m_prefabData["Components"].is_array()) return false;

    // Search each component entry
    for (const auto& comp : m_prefabData["Components"]) {
        // Validate type name
        if (!comp.contains("TypeName") || !comp["TypeName"].is_string()) continue;
        std::string typeName = comp["TypeName"];
        // Match short name or fully qualified name
        if (typeName == componentType || typeName == "ECS::Components::" + componentType) return true;
    }
    return false;
}

// -------------------------------------------------------------------------
// Prefab Data Management
// -------------------------------------------------------------------------

// Saves the current prefab JSON to disk
// We hash the JSON before saving so we avoid rewriting the file when nothing changed
void InspectorPanel::_savePrefabData() {
    // Must have a valid prefab path to write to
    if (m_prefabPath.empty()) return;

    // Turn the entire prefab JSON into a string and hash it
    size_t currentHash = std::hash<std::string>{}(m_prefabData.dump());

    // If the hash matches our previous saved version the file is already up to date
    if (currentHash == m_lastSavedPrefabHash) return;

    // Try writing the prefab JSON to the file
    std::ofstream file(m_prefabPath);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot write to prefab file";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot write to prefab file: " << m_prefabPath);
        return;
    }

    // Pretty print with indent of 4
    file << m_prefabData.dump(4);
    file.close();

    // Update the stored hash so we know this version is saved
    m_lastSavedPrefabHash = currentHash;
}

// Saves a live entity as a new prefab file
// We convert the entity into JSON and write only the Components array
void InspectorPanel::_saveEntityAsPrefab(ECS::Entity entity) {
    if (!m_world || !m_world->IsAlive(entity)) return;

    // Pick a base name for the prefab file
    std::string entityName = "Entity";
    if (m_world->Has<ECS::Components::Name>(entity)) {
        entityName = m_world->Get<ECS::Components::Name>(entity).Value;

        // Replace characters that are illegal or unsafe in file paths
        std::replace_if(entityName.begin(), entityName.end(),
            [](char c) { return !std::isalnum(c) && c != '_' && c != '-'; }, '_');
    }

    // Ensure prefab directory exists under the active project assets
    std::filesystem::path prefabDir = std::filesystem::path(Engine::ProjectPaths::GetAssetsPath()) / "Prefabs";
    std::filesystem::create_directories(prefabDir);

    // Pick a file name that does not overwrite an existing prefab
    std::filesystem::path prefabPath = prefabDir / (entityName + ".prefab");
    int suffix = 1;

    // If file already exists: Name.prefab, Name_1.prefab, Name_2.prefab, so on and so forth
    // If file doesn't exist: Name.prefab
    while (std::filesystem::exists(prefabPath)) {
        prefabPath = prefabDir / (entityName + "_" + std::to_string(suffix) + ".prefab");
        suffix++;
    }

    // Convert entity to JSON and extract just the Components list
    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
    nlohmann::json prefabData;
    prefabData["Components"] = entityJson.contains("Components") ? entityJson["Components"] : nlohmann::json::array();

    // Write the prefab file
    std::ofstream file(prefabPath);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot create prefab file";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot create prefab file: " << prefabPath);
        return;
    }

    // Same same
    file << prefabData.dump(4);
    file.close();

    m_statusMessage = "Saved as " + prefabPath.filename().string() + " in Assets\\Prefabs";
    m_statusTimer = 3.0f;
    LOG_INFO("Entity saved as prefab: " << prefabPath);
}

// Applies the current prefab JSON to every entity that is linked to this prefab
void InspectorPanel::_applyPrefabToInstances() {
    if (!m_world) return;

    // Make sure the prefab file on disk is up to date
    _savePrefabData();

    // Iterate over every entity that has a PrefabLink component
    int count = 0;
    m_world->Each<ECS::Components::PrefabLink>([&](ECS::Entity entity, ECS::Components::PrefabLink& link) {
        // Apply only to instances that match the prefab we are editing
        if (link.prefabPath == m_prefabPath) {
            _applyPrefabDataToEntity(entity);
            // Increment count for status message
            count++;
        }
        });

    m_statusMessage = "Applied to " + std::to_string(count) + " instance(s)";
    m_statusTimer = 2.0f;
}

// Applies all component data from the prefab JSON to one entity instance
// This overwrites any local edits so the instance stays in sync with the prefab
void InspectorPanel::_applyPrefabDataToEntity(ECS::Entity entity) {
    // Prefab must have a valid Components array
    if (!m_prefabData.contains("Components") || !m_prefabData["Components"].is_array()) return;

    // For each component in the prefab assign its JSON back into the ECS entity
    for (const auto& componentEntry : m_prefabData["Components"]) {
        // Basic validation
        if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
        if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

        std::string typeName = componentEntry["TypeName"];
        const auto* meta = ComponentRegistryUI::Find(typeName);

        // Use metadata to load the JSON into the live ECS component
        if (meta) {
            meta->ApplyToEntity(m_world, entity, componentEntry["Data"]);
        }
    }
}

// -------------------------------------------------------------------------
// Status Management
// -------------------------------------------------------------------------

// Renders the status message bar at the bottom of the inspector
// Shows short success or error messages that fade out over time
void InspectorPanel::_renderStatusBar() {
    if (m_statusTimer > 0.0f) {
        // Pick color based on whether the message contains "Failed"
        ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
            ? EditorStyle::DangerText
            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        ImGui::Separator();
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());

        // Countdown so the message disappears naturally
        m_statusTimer -= ImGui::GetIO().DeltaTime;
    }
}
