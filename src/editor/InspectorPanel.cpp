/* Start Header *****************************************************************/
/*!
\file   InspectorPanel.cpp
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   15th November 2025
\brief
Implements the unified InspectorPanel that adapts to selection context.

FEATURES:
- Shape components mutually exclusive (adding one removes others)
- Transform always shows on all entities (added automatically via registry)
- "Save as Prefab" button exports entities to assets/prefabs/ directory
- Footer height optimized (1 line)
- Delete button positioned at full scrollable width edge
- Auto-save on prefab edit (no manual "Save Changes" button)
- Component registry eliminates repetitive component handling code
- Empty prefabs get default Transform component when opened
- Prefab dragging onto hierarchy creates entity with PrefabLink component
*/
/* End Header *******************************************************************/

#include "../editor/InspectorPanel.h"
#include "../editor/ComponentInspectorUI.h"
#include "../editor/EditorUIHelpers.h"
#include "../editor/ComponentRegistryUI.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
void InspectorPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
    m_componentUI.Initialize(mainFont, boldFont, symbolsFont);
}

void InspectorPanel::SetWorld(ECS::World* world) {
    m_world = world;
    ClearSelection();
}

// -------------------------------------------------------------------------
// Selection Management
// -------------------------------------------------------------------------
void InspectorPanel::InspectEntity(EntityId id) {
    m_entityId = id;
    if (!m_world) {
        m_mode = InspectionMode::None;
        return;
    }
    ECS::Entity e{ id, 0 };
    if (!m_world->IsAlive(e)) {
        m_mode = InspectionMode::None;
        return;
    }

    // Ensure all entities have Transform component (mandatory) - add immediately
    const auto* transformMeta = ComponentRegistryUI::Find("LocalTransform");
    if (transformMeta && !transformMeta->HasComponent(m_world, e)) {
        transformMeta->AddComponent(m_world, e, transformMeta->GetDefaults());
    }

    m_mode = InspectionMode::Entity;
}

void InspectorPanel::InspectPrefab(const std::string& path) {
    if (path.empty()) {
        m_statusMessage = "Failed: No prefab path";
        m_statusTimer = 3.0f;
        return;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot open prefab";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot open prefab file: " << path);
        return;
    }

    try {
        // Read file content
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Handle empty or whitespace-only files - create minimal valid structure
        if (content.empty() || content.find_first_not_of(" \t\n\r") == std::string::npos) {
            // Empty file gets default Transform component
            const auto* transformMeta = ComponentRegistryUI::Find("LocalTransform");
            nlohmann::json defaultTransform = transformMeta ? transformMeta->GetDefaults() : nlohmann::json::object();

            m_prefabData = nlohmann::json{
                {"Components", nlohmann::json::array({
                    {{"TypeName", "ECS::Components::LocalTransform"}, {"Data", defaultTransform}}
                })}
            };

            m_prefabPath = path;
            m_lastSavedPrefabHash = 0; // Force save on first edit
            m_mode = InspectionMode::Prefab;

            // Auto-save the default structure to disk
            _savePrefabData();

            m_statusMessage = "Opened empty prefab, added Transform";
            m_statusTimer = 2.0f;
            return;
        }

        // Parse existing JSON content
        m_prefabData = nlohmann::json::parse(content);

        // Debug logging
        LOG_INFO("Loaded prefab: " << path);
        LOG_INFO("Prefab JSON keys: " << m_prefabData.dump());

        // Ensure Components array exists
        if (!m_prefabData.contains("Components")) {
            LOG_WARNING("Prefab missing Components key, creating empty array");
            m_prefabData["Components"] = nlohmann::json::array();
        }

        // Validate Components is actually an array
        if (!m_prefabData["Components"].is_array()) {
            LOG_ERROR("Prefab Components is not an array, type: " << m_prefabData["Components"].type_name());
            m_statusMessage = "Failed: Invalid prefab format (Components not array)";
            m_statusTimer = 3.0f;
            m_mode = InspectionMode::None;
            return;
        }

        // Log component count
        LOG_INFO("Prefab has " << m_prefabData["Components"].size() << " components");

        m_prefabPath = path;
        m_lastSavedPrefabHash = std::hash<std::string>{}(m_prefabData.dump());
        m_mode = InspectionMode::Prefab;
    }
    catch (const std::exception& e) {
        m_statusMessage = "Failed: Invalid JSON in prefab";
        m_statusTimer = 3.0f;
        m_mode = InspectionMode::None;
        LOG_ERROR("Failed to parse prefab JSON: " << e.what());
    }
}

void InspectorPanel::ClearSelection() {
    m_mode = InspectionMode::None;
    m_entityId = 0;
    m_prefabPath.clear();
    m_prefabData = {};
    m_componentsToDelete.clear();
}

// -------------------------------------------------------------------------
// Main Rendering
// -------------------------------------------------------------------------
void InspectorPanel::Render(float fontScale) {
    ImGui::PushFont(m_mainFont);

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

    _renderStatusBar();

    ImGui::End();
    ImGui::PopFont();
}

// -------------------------------------------------------------------------
// Entity Inspector Implementation
// -------------------------------------------------------------------------
void InspectorPanel::_renderEntityInspector() {
    if (!m_world) {
        ImGui::TextDisabled("No entity selected");
        return;
    }

    ECS::Entity entity{ m_entityId, 0 };
    if (!m_world->IsAlive(entity)) {
        ImGui::TextDisabled("Entity invalid");
        return;
    }

    _renderEntityHeader(entity);
    _renderEntityComponents(entity);
    _renderAddComponentButton(entity);
}

void InspectorPanel::_renderEntityHeader(ECS::Entity entity) {
    // Get entity name
    const char* entityName = "Unnamed";
    if (m_world->Has<ECS::Components::Name>(entity)) {
        entityName = m_world->Get<ECS::Components::Name>(entity).Value;
    }

    ImGui::Text("Entity ");
    ImGui::SameLine();
    ImGui::TextDisabled("%s (ID: %u)", entityName, (unsigned)m_entityId);

    // Show prefab link if entity is a prefab instance
    if (m_world->Has<ECS::Components::PrefabLink>(entity)) {
        const auto& link = m_world->Get<ECS::Components::PrefabLink>(entity);
        ImGui::Separator();
        ImGui::Text("Prefab Instance");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", std::filesystem::path(link.prefabPath).filename().string().c_str());

        ImGui::SameLine();
        if (ImGui::Button("Open Prefab")) {
            InspectPrefab(link.prefabPath);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Opens the prefab template file for editing");
        }

        ImGui::Separator();
    }
    else {
        // Show drag-drop zone for adding prefab link
        ImGui::Separator();
        ImGui::Text("Prefab Link");
        ImGui::SameLine();
        ImGui::TextDisabled("None (drag .prefab here to link)");

        // BeginDragDropTarget creates an invisible rectangular region over the last drawn item
        // where drag-and-drop payloads can be received when the mouse releases
        if (ImGui::BeginDragDropTarget()) {
            // AcceptDragDropPayload checks if payload type matches and returns data if mouse released
            // returns nullptr if no valid payload or wrong type
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                // Payload->Data is void* pointer to dragged string data
                // cast to const char* to read as C-string
                std::string droppedPath = static_cast<const char*>(payload->Data);
                if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                    // Add PrefabLink component to entity with the prefab path
                    // this connects the entity to the prefab template file
                    m_world->Add<ECS::Components::PrefabLink>(entity, droppedPath);
                    m_statusMessage = "Prefab linked to entity";
                    m_statusTimer = 2.0f;
                }
                else {
                    m_statusMessage = "Not a prefab: drop a .prefab file";
                    m_statusTimer = 2.0f;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::Separator();
    }
}

void InspectorPanel::_renderEntityComponents(ECS::Entity entity) {
    // Footer needs 2 lines: one for button row, one for status message
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    // Add padding for better layout
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("EntityComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Serialize entity to JSON for unified editing
    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

    if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
        ImGui::Dummy(ImVec2(0, 4));

        // Render components using registry
        // match components from JSON with registry entries
        for (auto& componentEntry : entityJson["Components"]) {
            // Validate component entry structure before accessing fields
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
            if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

            std::string typeName = componentEntry["TypeName"];
            const auto* meta = ComponentRegistryUI::Find(typeName);

            if (meta) {
                auto& data = componentEntry["Data"];
                _renderComponentSection(meta->DisplayName, meta->TypeName, data,
                    [this, meta](nlohmann::json& d) { meta->RenderUI(m_componentUI, d); },
                    meta->CanDelete);
                ImGui::Dummy(ImVec2(0, 4));
            }
        }

        // Process component deletions
        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromEntity(type);
        }
        m_componentsToDelete.clear();

        // Write modified JSON back to entity components
        for (const auto& componentEntry : entityJson["Components"]) {
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
            if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

            std::string typeName = componentEntry["TypeName"];
            const auto* meta = ComponentRegistryUI::Find(typeName);
            if (meta) {
                meta->ApplyToEntity(m_world, entity, componentEntry["Data"]);
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void InspectorPanel::_renderAddComponentButton(ECS::Entity entity) {
    ImGui::Separator();

    // Add Component button
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }

    // Save as Prefab button next to Add Component
    ImGui::SameLine();
    if (ImGui::Button("Save as Prefab")) {
        _saveEntityAsPrefab(entity);
    }

    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

        // Render all components from registry
        for (const auto& meta : ComponentRegistryUI::GetAll()) {
            _renderComponentMenuItem(meta.DisplayName.c_str(), meta.TypeName.c_str());
        }

        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------------------
// Prefab Inspector Implementation
// -------------------------------------------------------------------------
void InspectorPanel::_renderPrefabInspector() {
    if (m_prefabPath.empty()) {
        ImGui::TextDisabled("No prefab selected");
        return;
    }

    _renderPrefabHeader();
    _renderPrefabComponents();
    _renderPrefabActions();
}

void InspectorPanel::_renderPrefabHeader() {
    ImGui::Text("Editing Template");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", std::filesystem::path(m_prefabPath).filename().string().c_str());

    ImGui::Separator();
    ImGui::TextWrapped("Changes to this prefab will auto-save and update ALL instances");
    ImGui::Separator();
}

void InspectorPanel::_renderPrefabComponents() {
    // Footer needs 2 lines: one for buttons, one for status message
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("PrefabComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (m_prefabData.contains("Components") && m_prefabData["Components"].is_array()) {
        ImGui::Dummy(ImVec2(0, 4));

        int componentsRendered = 0;

        // SORT PREFAB COMPONENTS: Transform first, then alphabetical (same as EntitySerializer)
        auto& componentsArray = m_prefabData["Components"];
        std::sort(componentsArray.begin(), componentsArray.end(),
            [](const nlohmann::json& a, const nlohmann::json& b) {
                std::string nameA = a.value("TypeName", "");
                std::string nameB = b.value("TypeName", "");
                // Transform always comes first
                if (nameA == "LocalTransform") return true;
                if (nameB == "LocalTransform") return false;
                // Everything else alphabetical
                return nameA < nameB;
            });

        // Render in sorted order
        for (auto& componentEntry : componentsArray) {
            // Validate component entry structure before accessing fields
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string())
                continue;

            std::string typeName = componentEntry["TypeName"];
            const auto* meta = ComponentRegistryUI::Find(typeName);

            if (meta) {
                // Ensure Data field exists and is an object
                if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object())
                    componentEntry["Data"] = nlohmann::json::object();

                auto& dataObj = componentEntry["Data"];
                _renderComponentSection(meta->DisplayName, meta->TypeName, dataObj,
                    [this, meta](nlohmann::json& d) { meta->RenderUI(m_componentUI, d); },
                    meta->CanDelete);
                ImGui::Dummy(ImVec2(0, 4));
            }
        }

        // Process component deletions
        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromPrefab(type);
        }
        m_componentsToDelete.clear();

        // Auto-save prefab on any change
        _savePrefabData();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void InspectorPanel::_renderPrefabActions() {
    ImGui::Separator();

    // Add Component button with default width
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }

    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

        // Render all components from registry
        for (const auto& meta : ComponentRegistryUI::GetAll()) {
            _renderComponentMenuItem(meta.DisplayName.c_str(), meta.TypeName.c_str());
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Apply to All Instances button with default width
    if (ImGui::Button("Apply to All Instances")) {
        _applyPrefabToInstances();
    }
}

// -------------------------------------------------------------------------
// Component Menu Item Rendering
// -------------------------------------------------------------------------
void InspectorPanel::_renderComponentMenuItem(const char* displayName, const char* componentType) {
    bool hasComponent = false;

    if (m_mode == InspectionMode::Entity) {
        hasComponent = _entityHasComponent(m_entityId, componentType);
    }
    else if (m_mode == InspectionMode::Prefab) {
        hasComponent = _prefabHasComponent(componentType);
    }

    if (hasComponent) ImGui::BeginDisabled();

    if (ImGui::Selectable(displayName)) {
        if (m_mode == InspectionMode::Entity) _addComponentToEntity(componentType);
        else if (m_mode == InspectionMode::Prefab) _addComponentToPrefab(componentType);
    }

    if (hasComponent) ImGui::EndDisabled();

    if (hasComponent && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Component already added");
    }
}

// -------------------------------------------------------------------------
// Entity Component Management
// -------------------------------------------------------------------------
void InspectorPanel::_addComponentToEntity(const std::string& componentType) {
    if (!m_world) return;
    ECS::Entity entity{ m_entityId, 0 };
    if (!m_world->IsAlive(entity)) return;

    // Adding a shape component removes other shapes (mutually exclusive)
    if (componentType == "ShapeCircle2D" || componentType == "ShapeBox2D" || componentType == "ShapeLine2D") {
        auto* circle = ComponentRegistryUI::Find("ShapeCircle2D");
        auto* box = ComponentRegistryUI::Find("ShapeBox2D");
        auto* line = ComponentRegistryUI::Find("ShapeLine2D");
        if (circle) circle->RemoveComponent(m_world, entity);
        if (box) box->RemoveComponent(m_world, entity);
        if (line) line->RemoveComponent(m_world, entity);
    }

    // Get component from registry and add it
    const auto* meta = ComponentRegistryUI::Find(componentType);
    if (meta) {
        meta->AddComponent(m_world, entity, meta->GetDefaults());
    }
}

void InspectorPanel::_removeComponentFromEntity(const std::string& componentType) {
    if (!m_world) return;
    ECS::Entity entity{ m_entityId, 0 };
    if (!m_world->IsAlive(entity)) return;

    if (componentType == "LocalTransform") return; // Can't delete transform

    const auto* meta = ComponentRegistryUI::Find(componentType);
    if (meta) {
        meta->RemoveComponent(m_world, entity);
        m_statusMessage = std::string("Removed ") + componentType;
        m_statusTimer = 2.0f;
    }
}

bool InspectorPanel::_entityHasComponent(EntityId id, const std::string& componentType) {
    ECS::Entity entity{ id, 0 };
    if (!m_world->IsAlive(entity)) return false;

    const auto* meta = ComponentRegistryUI::Find(componentType);
    return meta ? meta->HasComponent(m_world, entity) : false;
}

// -------------------------------------------------------------------------
// Prefab Component Management
// -------------------------------------------------------------------------
void InspectorPanel::_addComponentToPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) {
        m_prefabData["Components"] = nlohmann::json::array();
    }

    if (_prefabHasComponent(componentType)) return;

    // Adding a shape component removes other shapes (mutually exclusive)
    if (componentType == "ShapeCircle2D" || componentType == "ShapeBox2D" || componentType == "ShapeLine2D") {
        _removeComponentFromPrefab("ShapeCircle2D");
        _removeComponentFromPrefab("ShapeBox2D");
        _removeComponentFromPrefab("ShapeLine2D");
    }

    // Get defaults from registry
    const auto* meta = ComponentRegistryUI::Find(componentType);
    if (!meta) return;

    nlohmann::json data = meta->GetDefaults();
    m_prefabData["Components"].push_back({ {"TypeName", meta->FullTypeName}, {"Data", data} });

    // Auto-save after adding component
    _savePrefabData();
}

void InspectorPanel::_removeComponentFromPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components") || !m_prefabData["Components"].is_array()) return;

    auto& components = m_prefabData["Components"];
    for (auto it = components.begin(); it != components.end(); it++) {
        // Validate component entry structure before accessing fields
        if (!(*it).contains("TypeName") || !(*it)["TypeName"].is_string()) continue;

        std::string typeName = (*it)["TypeName"];
        // Match both short and full names
        if (typeName == componentType || typeName == "ECS::Components::" + componentType) {
            components.erase(it);
            m_statusMessage = std::string("Removed ") + componentType;
            m_statusTimer = 2.0f;
            // Auto-save after removing component
            _savePrefabData();
            return;
        }
    }
}

bool InspectorPanel::_prefabHasComponent(const std::string& componentType) {
    if (!m_prefabData.contains("Components") || !m_prefabData["Components"].is_array()) return false;

    for (const auto& comp : m_prefabData["Components"]) {
        // Validate component entry structure before accessing fields
        if (!comp.contains("TypeName") || !comp["TypeName"].is_string()) continue;

        std::string typeName = comp["TypeName"];
        // Match both short and full names
        if (typeName == componentType || typeName == "ECS::Components::" + componentType) return true;
    }
    return false;
}

// -------------------------------------------------------------------------
// Prefab Data Management
// -------------------------------------------------------------------------
void InspectorPanel::_loadPrefabData() {
    // Already handled in InspectPrefab
}

void InspectorPanel::_savePrefabData() {
    if (m_prefabPath.empty()) return;

    // Check if data actually changed using hash
    size_t currentHash = std::hash<std::string>{}(m_prefabData.dump());
    if (currentHash == m_lastSavedPrefabHash) {
        // No changes, skip save
        return;
    }

    std::ofstream file(m_prefabPath);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot write to prefab file";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot write to prefab file: " << m_prefabPath);
        return;
    }

    file << m_prefabData.dump(4);
    file.close();

    m_lastSavedPrefabHash = currentHash;
}

void InspectorPanel::_saveEntityAsPrefab(ECS::Entity entity) {
    if (!m_world || !m_world->IsAlive(entity)) return;

    // Get entity name for filename
    std::string entityName = "Entity";
    if (m_world->Has<ECS::Components::Name>(entity)) {
        entityName = m_world->Get<ECS::Components::Name>(entity).Value;
        // Sanitize name for filesystem
        std::replace_if(entityName.begin(), entityName.end(),
            [](char c) { return !std::isalnum(c) && c != '_' && c != '-'; }, '_');
    }

    // Create prefabs directory if it doesn't exist
    std::filesystem::path prefabDir = "assets/prefabs";
    std::filesystem::create_directories(prefabDir);

    // Generate unique filename
    std::filesystem::path prefabPath = prefabDir / (entityName + ".prefab");
    int suffix = 1;
    while (std::filesystem::exists(prefabPath)) {
        prefabPath = prefabDir / (entityName + "_" + std::to_string(suffix) + ".prefab");
        suffix++;
    }

    // Serialize entity to JSON using EntitySerializer
    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

    // Extract just the Components array for the prefab
    nlohmann::json prefabData;
    if (entityJson.contains("Components")) {
        prefabData["Components"] = entityJson["Components"];
    }
    else {
        prefabData["Components"] = nlohmann::json::array();
    }

    // Write to file
    std::ofstream file(prefabPath);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot create prefab file";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot create prefab file: " << prefabPath);
        return;
    }

    file << prefabData.dump(4);
    file.close();

    m_statusMessage = "Saved as " + prefabPath.filename().string();
    m_statusTimer = 3.0f;
    LOG_INFO("Entity saved as prefab: " << prefabPath);
}


void InspectorPanel::_applyPrefabToInstances() {
    if (!m_world) return;

    // First save the prefab
    _savePrefabData();

    // Find all entities with PrefabLink pointing to this prefab
    int count = 0;
    m_world->Each<ECS::Components::PrefabLink>([&](ECS::Entity entity, ECS::Components::PrefabLink& link) {
        if (link.prefabPath == m_prefabPath) {
            _applyPrefabDataToEntity(entity);
            count++;
        }
        });

    m_statusMessage = "Applied to " + std::to_string(count) + " instance(s)";
    m_statusTimer = 2.0f;
}

void InspectorPanel::_applyPrefabDataToEntity(ECS::Entity entity) {
    if (!m_prefabData.contains("Components") || !m_prefabData["Components"].is_array()) return;

    // Apply each component from prefab data to the entity
    for (const auto& componentEntry : m_prefabData["Components"]) {
        // Validate component entry structure before accessing fields
        if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
        if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

        std::string typeName = componentEntry["TypeName"];
        const auto* meta = ComponentRegistryUI::Find(typeName);
        if (meta) {
            meta->ApplyToEntity(m_world, entity, componentEntry["Data"]);
        }
    }
}

// -------------------------------------------------------------------------
// Status Bar
// -------------------------------------------------------------------------
void InspectorPanel::_renderStatusBar() {
    if (m_statusTimer > 0.0f) {
        ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
            ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        ImGui::Separator();
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());
        m_statusTimer -= ImGui::GetIO().DeltaTime;
    }
}

// Note: Template function _renderComponentSection is defined in header and instantiated on use