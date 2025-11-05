/* Start Header *****************************************************************/
/*!
\file   PrefabEditor.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implements prefab editing functionality for the asset browser.

Features:
- Prefab instantiation with entity creation
- JSON-based component editing
- Real-time prefab instance updates
- Component management with default values
- Generic rendering system using lambdas

References:
- ImGui styling and layout functions
- nlohmann/json for data serialization
- Entity component system for prefab instances
*/
/* End Header *******************************************************************/

#include "../editor/PrefabEditor.h"
#include "../editor/ComponentInspectorUI.h"  // Use shared component UI
#include "../editor/InspectorWindow.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include <fstream>

void PrefabEditor::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
}

// Render the prefab editor window if currently editing
void PrefabEditor::RenderEditor(float fontScale, std::string& statusMessage, float& statusTimer) {
    if (m_inspector) {
        // Unified inspector owns rendering; nothing to do here
        return;
    }
    if (m_editingPrefab) {
        _showPrefabEditor(fontScale, statusMessage, statusTimer);
    }
}

// Load and instantiate selected prefab into the level
void PrefabEditor::_loadPrefab(const std::string& prefabPath, std::string& statusMessage, float& statusTimer) {
    // Ensure prefab is selected
    if (prefabPath.empty()) {
        LOG_WARNING("No prefab selected");
        return;
    }

    // Ensure valid world reference exists
    if (!m_world) {
        LOG_ERROR("Cannot load prefab: No world reference");
        return;
    }

    // Try to open the selected prefab file
    std::ifstream file(prefabPath);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open file: " << prefabPath);
    }
    else {
        try {
            // Parse prefab JSON
            auto entityJson = nlohmann::json::parse(file);
            file.close();

            // Deserialize creates the entity internally
            auto entity = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);

            // Tag entity with prefab link so we can update it later when prefab changes
            entity.AddComponent<Component::PrefabLink>(prefabPath);

            LOG_INFO("Loaded prefab: " << std::filesystem::path(prefabPath).filename().string());
            statusMessage = "Prefab loaded successfully";
            statusTimer = 3.0f;
        }
        catch (const std::exception& e) {
            // Handle JSON parsing or deserialization errors
            LOG_ERROR("Failed to parse prefab file: " << e.what());
            statusMessage = "Failed to load prefab";
            statusTimer = 3.0f;
        }
    }
}

// Open prefab for editing (loads JSON into memory and sets flag to show editor window)
void PrefabEditor::_editPrefab(const std::string& prefabPath, std::string& statusMessage, float& statusTimer) {
    // Forward to Inspector when available
    if (m_inspector) {
        if (prefabPath.empty()) { LOG_WARNING("No prefab selected to edit"); return; }
        m_inspector->InspectPrefab(prefabPath);
        statusMessage = "Opened in Inspector";
        statusTimer = 2.0f;
        m_editingPrefab = false; // Inspector now owns the UI
        m_editingPrefabPath = prefabPath;
        m_prefabData = {}; // Clear legacy buffer
        return;
    }

    // Legacy fallback
    if (prefabPath.empty()) { LOG_WARNING("No prefab selected to edit"); return; }
    std::ifstream file(prefabPath);
    if (!file.is_open()) { LOG_ERROR("Cannot open prefab file: " << prefabPath); statusMessage = "Failed to open prefab"; statusTimer = 3.0f; return; }
    try { m_prefabData = nlohmann::json::parse(file); m_editingPrefabPath = prefabPath; m_editingPrefab = true; file.close(); LOG_INFO("Opened prefab for editing: " << std::filesystem::path(prefabPath).filename().string()); }
    catch (const std::exception& e) { LOG_ERROR("Failed to parse prefab: " << e.what()); statusMessage = "Failed to parse prefab"; statusTimer = 3.0f; m_editingPrefab = false; }
}

// Find all entities using this prefab and update them with new prefab data
void PrefabEditor::_updatePrefabInstances() {
    // Safety checks
    if (!m_world || m_editingPrefabPath.empty()) return;
    if (!m_prefabData.contains("Components")) return;

    int updatedCount = 0;  // Track successful updates
    auto allEntities = m_world->GetEntityManager().GetAllEntities();

    // Check each entity to see if it uses this prefab
    for (auto entityId : allEntities) {
        auto entity = m_world->GetEntityManager().GetEntity(entityId);

        // Check if entity is linked to this prefab
        auto* prefabLink = entity.GetComponent<Component::PrefabLink>();
        if (!prefabLink || prefabLink->prefabPath != m_editingPrefabPath) {
            continue;  // Skip if not linked to this prefab
        }

        // Update this prefab instance with new data
        if (_updateEntityFromPrefab(entity)) {
            updatedCount++;
        }
    }

    LOG_INFO("Updated " << updatedCount << " prefab instances");
}

// Update single entity's components from prefab data (synchronizes instance with prefab)
bool PrefabEditor::_updateEntityFromPrefab(Entity& entity) {
    try {
        // Iterate through each component defined in the prefab JSON
        // The prefab JSON structure is: { "Components": [ { "Type": "Transform", "Data": {...} }, ... ] }
        for (const auto& componentEntry : m_prefabData["Components"]) {
            std::string typeName = componentEntry["Type"];  // Get component type name (e.g. "Transform")

            // Helper lambda that handles the repetitive pattern ([&] captures everything by reference)
            // We check if component matches type name; if so, get the actual component via GetComponent<T>()
            // Then call from_json which reads JSON data and updates component's fields
            auto updateComponent = [&]<typename T>(const std::string & name) {
                if (typeName == name) {
                    if (auto* component = entity.GetComponent<T>()) {
                        from_json(componentEntry["Data"], *component);
                    }
                    return true;
                }
                return false;
            };

            // Special-case Transform to tolerate missing keys (e.g., ParentId)
            if (typeName == "Transform") {
                if (!entity.HasComponent<Component::Transform>()) {
                    entity.AddComponent<Component::Transform>();
                }
                if (auto* t = entity.GetComponent<Component::Transform>()) {
                    const auto& d = componentEntry["Data"];
                    // Position
                    if (d.contains("Position")) {
                        const auto& p = d["Position"];
                        t->Position.X = p.value("X", t->Position.X);
                        t->Position.Y = p.value("Y", t->Position.Y);
                    }
                    // Rotation
                    t->Rotation = d.value("Rotation", t->Rotation);
                    // Scale
                    if (d.contains("Scale")) {
                        const auto& s = d["Scale"];
                        t->Scale.X = s.value("X", t->Scale.X);
                        t->Scale.Y = s.value("Y", t->Scale.Y);
                    }
                    // ParentId (optional in prefab JSON)
                    t->ParentId = d.value("ParentId", t->ParentId);
                }
                continue;
            }
            
            // For other component types, call the lambda with the type + JSON type name
            // Updates TexturePath, Color, FlipX, etc.
            if (updateComponent.operator() < Component::SpriteRenderer > ("SpriteRenderer")) continue;
            // Updates Mass, Velocity, BodyType, etc.
            if (updateComponent.operator() < Component::Rigidbody2D > ("Rigidbody2D")) continue;
            // Updates Radius, Offset, IsTrigger, etc.
            if (updateComponent.operator() < Component::CircleCollider2D > ("CircleCollider2D")) continue;
            // Updates Size, Offset, IsTrigger, etc.
            if (updateComponent.operator() < Component::BoxCollider2D > ("BoxCollider2D")) continue;
            // Updates Type, FillColor, Radius, etc.
            if (updateComponent.operator() < Component::ShapeRenderer2D > ("ShapeRenderer2D")) continue;
            // Updates Start, End, Thickness, etc.
            if (updateComponent.operator() < Component::LineRenderer > ("LineRenderer")) continue;
        }

        LOG_INFO("Updated entity " << entity.GetId() << " from prefab");
        return true;  // Success
    }
    catch (const std::exception& e) {
        // JSON parsing or component update failed
        LOG_ERROR("Failed to update entity " << entity.GetId() << ": " << e.what());
        return false;
    }
}

// Generic component section renderer: wraps content in a collapsing header
// Lambda function allows flexible rendering of any component's properties
template <typename T>
void PrefabEditor::_renderComponentSection(const std::string& headerName, const std::string& componentType,
    nlohmann::json& data, T renderContent, bool canDelete) {
    // Collapsing header (click triangle to expand/collapse)
    bool nodeOpen = ImGui::CollapsingHeader(headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    // Store previous cursor position before we move it for the delete button
    ImVec2 originalCursorPos = ImGui::GetCursorPos();

    // Position delete button at right edge of window
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 50);
    if (!canDelete) ImGui::BeginDisabled();

    // Style the delete button: icon font + transparent background + red text (if canDelete, else gray)
    ImGui::PushFont(m_symbolsFont);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));                     // Transparent
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));  // Subtle hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));   // Subtle click
    ImGui::PushStyleColor(ImGuiCol_Text, canDelete ? ImVec4(0.7f, 0.2f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

    // Render delete icon button
    if (ImGui::SmallButton(("\xEE\xA1\xB2\##Delete" + componentType).c_str())) {
        // Queue component for deletion (can't delete while iterating)
        m_componentsToDelete.push_back(componentType);
    }

    // Restore style and font state
    ImGui::PopStyleColor(4);  // Pop all 4 color style overrides
    ImGui::PopFont();

    // Re-enable UI if disabled earlier
    if (!canDelete) ImGui::EndDisabled();

    // Show tooltip on hover (even if button disabled)
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip(canDelete ? "Remove Component" : "Transform cannot be removed");
    }

    // Restore original cursor position so content renders in correct place
    ImGui::SetCursorPos(originalCursorPos);

    // If the section is expanded, call the provided lambda
    // to render the component's editable fields (e.g. position, rotation, etc.)
    if (nodeOpen) {
        renderContent(data);
    }
}

// Check if prefab already has a specific component type
bool PrefabEditor::_prefabHasComponent(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return false;

    // Iterate through component array and check Type field
    for (const auto& component : m_prefabData["Components"]) {
        if (component["Type"] == componentType) {
            return true;
        }
    }
    return false;
}

// Get default component data by type
// This provides sensible starting values when adding a new component to a prefab
nlohmann::json PrefabEditor::_getDefaultComponentData(const std::string& componentType) {
    if (componentType == "Transform") {
        return {
            {"Position", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Rotation", 0.0f},
            {"Scale", {{"X", 1.0f}, {"Y", 1.0f}}}
        };
    }
    else if (componentType == "SpriteRenderer") {
        return {
            {"TexturePath", ""},
            {"Sprite", ""},
            {"Width", 0},
            {"Height", 0},
            {"Color", {{"R", 255.0f}, {"G", 255.0f}, {"B", 255.0f}, {"A", 255.0f}}},
            {"FlipX", false},
            {"FlipY", false},
            {"SortingOrder", 0},
            {"SortingLayerName", "Default"}
        };
    }
    else if (componentType == "Rigidbody2D") {
        return {
            {"Mass", 1.0f},
            {"LinearDamping", 0.0f},
            {"AngularDamping", 0.05f},
            {"GravityScale", 1.0f},
            {"BodyType", 0},
            {"FreezeRotation", false},
            {"LinearVelocity", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"AngularVelocity", 0.0f},
            {"Inertia", 0.0f},
            {"CenterOfMass", {{"X", 0.0f}, {"Y", 0.0f}}}
        };
    }
    else if (componentType == "CircleCollider2D") {
        return {
            {"IsTrigger", false},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Radius", 0.5f},
            {"Layer", 0}
        };
    }
    else if (componentType == "BoxCollider2D") {
        return {
            {"IsTrigger", false},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Size", {{"X", 1.0f}, {"Y", 1.0f}}},
            {"Layer", 0}
        };
    }
    else if (componentType == "ShapeRenderer2D") {
        return {
            {"Type", 0},
            {"FillColor", {{"R", 255.0f}, {"G", 255.0f}, {"B", 255.0f}, {"A", 255.0f}}},
            {"OutlineColor", {{"R", 0.0f}, {"G", 0.0f}, {"B", 0.0f}, {"A", 255.0f}}},
            {"OutlineThickness", 1.0f},
            {"Size", {{"X", 100.0f}, {"Y", 100.0f}}},
            {"Radius", 50.0f},
            {"Points", nlohmann::json::array()},
            {"Closed", true}
        };
    }
    else if (componentType == "LineRenderer") {
        return {
            {"Start", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"End", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Thickness", 1.0f},
            {"Color", {{"R", 255.0f}, {"G", 255.0f}, {"B", 255.0f}, {"A", 255.0f}}}
        };
    }
    return nlohmann::json::object();
}

// Add a new component to the prefab with default values
void PrefabEditor::_addComponentToPrefab(const std::string& componentType) {
    // Prevent duplicate components
    if (_prefabHasComponent(componentType)) {
        LOG_WARNING("Component " << componentType << " already exists in prefab");
        return;
    }

    // Ensure the "Components" array exists in the prefab JSON
    if (!m_prefabData.contains("Components")) {
        m_prefabData["Components"] = nlohmann::json::array();
    }

    // Create a new JSON entry for the component with default data
    nlohmann::json newComponent;
    newComponent["Type"] = componentType;
    newComponent["Data"] = _getDefaultComponentData(componentType);

    // Add the new component to the prefab's component list
    m_prefabData["Components"].push_back(newComponent);
    LOG_INFO("Added " << componentType << " to prefab");
}

// Remove a component from the prefab
void PrefabEditor::_removeComponentFromPrefab(const std::string& componentType, std::string& statusMessage, float& statusTimer) {
    // Ensure the prefab actually has a "Components" array before proceeding
    if (!m_prefabData.contains("Components")) return;

    auto& components = m_prefabData["Components"];

    // Search for the component with the matching type
    for (auto it = components.begin(); it != components.end(); it++) {
        if ((*it)["Type"] == componentType) {
            // Remove the component from the array
            components.erase(it);
            LOG_INFO("Removed " << componentType << " from prefab");
            statusMessage = "Component removed: " + componentType;
            statusTimer = 3.0f;
            return;
        }
    }

    LOG_WARNING("Component " << componentType << " not found in prefab");
}

// Render add component menu item with duplicate check and tooltip
void PrefabEditor::_renderComponentMenuItem(const char* displayName, const char* componentType) {
    bool hasComponent = _prefabHasComponent(componentType);
    if (hasComponent) ImGui::BeginDisabled();

    if (ImGui::Selectable(displayName)) {
        _addComponentToPrefab(componentType);
    }

    if (hasComponent) ImGui::EndDisabled();
    if (hasComponent && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Component already added");
    }
}

// Display prefab editor window with property editing
void PrefabEditor::_showPrefabEditor(float fontScale, std::string& statusMessage, float& statusTimer) {
    // Window with close button (X) that sets m_editingPrefab to false
    if (ImGui::Begin("Prefab Editor", &m_editingPrefab)) {
        ImGui::SetWindowFontScale(fontScale);

        // Header: Display which prefab we're editing
        ImGui::Text("Prefab");
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::TextDisabled("%s", m_editingPrefabPath.empty() ? "None" :
            std::filesystem::path(m_editingPrefabPath).filename().string().c_str());

        // Render all components in the prefab
        // We use component-specific render functions for each type
        if (m_prefabData.contains("Components")) {
            for (auto& componentEntry : m_prefabData["Components"]) {
                std::string componentType = componentEntry["Type"];
                auto& data = componentEntry["Data"];

                // Dispatch to appropriate component renderer
                // Now using shared ComponentUI functions instead of duplicates!
                if (componentType == "Transform") {
                    _renderComponentSection("Transform", "Transform", data,
                        [](nlohmann::json& d) { ComponentUI::RenderTransform(d); }, false);
                }
                else if (componentType == "SpriteRenderer") {
                    _renderComponentSection("Sprite Renderer", "SpriteRenderer", data,
                        [](nlohmann::json& d) { ComponentUI::RenderSpriteRenderer(d); });
                }
                else if (componentType == "Rigidbody2D") {
                    _renderComponentSection("Rigidbody2D", "Rigidbody2D", data,
                        [](nlohmann::json& d) { ComponentUI::RenderRigidbody2D(d); });
                }
                else if (componentType == "CircleCollider2D") {
                    _renderComponentSection("CircleCollider2D", "CircleCollider2D", data,
                        [](nlohmann::json& d) { ComponentUI::RenderCircleCollider2D(d); });
                }
                else if (componentType == "BoxCollider2D") {
                    _renderComponentSection("BoxCollider2D", "BoxCollider2D", data,
                        [](nlohmann::json& d) { ComponentUI::RenderBoxCollider2D(d); });
                }
                else if (componentType == "ShapeRenderer2D") {
                    _renderComponentSection("ShapeRenderer2D", "ShapeRenderer2D", data,
                        [](nlohmann::json& d) { ComponentUI::RenderShapeRenderer2D(d); });
                }
                else if (componentType == "LineRenderer") {
                    _renderComponentSection("LineRenderer", "LineRenderer", data,
                        [](nlohmann::json& d) { ComponentUI::RenderLineRenderer(d); });
                }
            }

            // Process component deletions (queued during rendering)
            for (const auto& componentType : m_componentsToDelete) {
                _removeComponentFromPrefab(componentType, statusMessage, statusTimer);
            }
            m_componentsToDelete.clear();
        }

        ImGui::Separator();

        // Add Component button and popup menu
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponentMenu");
        }

        if (ImGui::BeginPopup("AddComponentMenu")) {
            ImGui::PushFont(m_boldFont);
            ImGui::Text("Components");
            ImGui::PopFont();
            ImGui::Separator();

            // List all available component types
            // Each item checks if already added and disables/shows tooltip accordingly
            _renderComponentMenuItem("Transform", "Transform");
            _renderComponentMenuItem("Sprite Renderer", "SpriteRenderer");
            _renderComponentMenuItem("Rigidbody 2D", "Rigidbody2D");
            _renderComponentMenuItem("Circle Collider 2D", "CircleCollider2D");
            _renderComponentMenuItem("Box Collider 2D", "BoxCollider2D");
            _renderComponentMenuItem("Shape Renderer 2D", "ShapeRenderer2D");
            _renderComponentMenuItem("Line Renderer", "LineRenderer");

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        // Apply button: saves modified JSON to file and updates all prefab instances in the world
        if (ImGui::Button("Apply to All Instances")) {
            std::ofstream file(m_editingPrefabPath);
            if (file.is_open()) {
                // Save to build/assets
                file << m_prefabData.dump(2);  // Pretty print with 2-space indent
                file.close();

                // Also save to source assets folder (../assets)
                std::string sourceAssetsPath = m_editingPrefabPath;
                if (sourceAssetsPath.find("assets") != std::string::npos) {
                    std::filesystem::path relativePath = std::filesystem::relative(
                        std::filesystem::path(m_editingPrefabPath), "assets");
                    std::filesystem::path destPathSource = std::filesystem::path("..") / "assets" / relativePath;

                    std::ofstream fileSource(destPathSource);
                    if (fileSource.is_open()) {
                        fileSource << m_prefabData.dump(2);
                        fileSource.close();
                    }
                }

                // Synchronize all entities that were instantiated from this prefab
                _updatePrefabInstances();

                LOG_INFO("Saved prefab and updated instances");
                statusMessage = "Prefab updated successfully";
                statusTimer = 3.0f;
                m_editingPrefab = false;  // Close editor window after successful save
            }
        }

        ImGui::SameLine();

        // Cancel button: discard changes and close window without saving
        if (ImGui::Button("Cancel")) {
            m_editingPrefab = false;
        }
    }
    ImGui::End();
}
