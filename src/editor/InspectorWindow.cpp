/* Start Header *****************************************************************/
/*!
\file   InspectorWindow.cpp
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Implements the unified InspectorWindow that adapts to selection context:
- Entity mode: Shows components of a selected entity instance
- Prefab mode: Shows components of a prefab template (editing the .prefab file)
*/
/* End Header *******************************************************************/

#include "../editor/InspectorWindow.h"
#include "../editor/ComponentInspectorUI.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

// Store font references and pass them down to ComponentUI for consistent styling
// ComponentUI is the shared rendering system for all component types
void InspectorWindow::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
    m_componentUI.Initialize(mainFont, boldFont, symbolsFont);
}

void InspectorWindow::Render(float fontScale) {
    ImGui::PushFont(m_mainFont);

    // Window title changes based on mode
    // "Prefab Editor" when editing the template file, "Property Editor" for entity instances
    const char* windowTitle = (m_mode == InspectionMode::Prefab) ? "Prefab Editor" : "Property Editor";
    ImGui::Begin(windowTitle);
    // Follow global ImGui::GetIO().FontGlobalScale for consistent font sizing across all windows
    // Do not apply per-window font scaling here to avoid inconsistencies

    // Render different content based on inspection mode
    if (m_mode == InspectionMode::None) {
        ImGui::TextDisabled("No selection");
    }
    else if (m_mode == InspectionMode::Entity) {
        _renderEntityCore();
    }
    else if (m_mode == InspectionMode::Prefab) {
        _renderPrefabInspector();
    }

    // Status message toast appears at bottom of window
    // Red text for errors, green for success, automatically fades after a few seconds
    if (m_statusTimer > 0.0f) {
        ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
            ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)   // Red for errors
            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);  // Green for success
        ImGui::Separator();
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());
        m_statusTimer -= ImGui::GetIO().DeltaTime;
    }

    ImGui::End();
    ImGui::PopFont();
}

// Switch to inspecting an entity instance in the scene
// If ID is 0, we clear the selection instead
void InspectorWindow::InspectEntity(EntityId id) {
    m_inspectedEntityId = id;
    // Treat any alive entity (including index 0) as valid selection; otherwise clear
    if (!m_world) {
        m_mode = InspectionMode::None;
        return;
    }
    ECS::Entity e{ id, 0 };
    m_mode = m_world->IsAlive(e) ? InspectionMode::Entity : InspectionMode::None;
}

// Switch to inspecting a prefab template (the .prefab file itself)
// Loads the JSON from disk so we can edit the template directly
// Any changes here will affect ALL instances of this prefab in the scene
void InspectorWindow::InspectPrefab(const std::string& prefabPath) {
    // Make sure we actually got a path
    if (prefabPath.empty()) {
        m_statusMessage = "Failed: No prefab path";           // Show error in UI
        m_statusTimer = 3.0f;                                 // Keep message visible for 3 seconds
        return;
    }

    // Try to open the prefab file for reading
    std::ifstream file(prefabPath);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot open prefab";       // Show error in UI
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot open prefab file: " << prefabPath); // Log detailed error
        return;
    }

    try {
        // Parse the JSON content of the prefab
        m_prefabData = nlohmann::json::parse(file);
        file.close();

        // Keep track of the prefab path we are inspecting
        m_inspectedPrefabPath = prefabPath;

        // Hash the initial content so we can detect changes later
        // This prevents writing to disk if nothing actually changed
        // Also prevents unnecessary updates on all instances in the scene
        m_lastSavedPrefabHash = std::hash<std::string>{}(m_prefabData.dump());

        // Switch mode to prefab inspection
        m_mode = InspectionMode::Prefab;
    }
    catch (const std::exception& e) {
        // If parsing failed, reset mode
        m_mode = InspectionMode::None;
        LOG_ERROR("Failed to parse prefab JSON: " << e.what());
    }
}

// Clears current selection
void InspectorWindow::ClearSelection() {
    m_mode = InspectionMode::None;
    m_inspectedEntityId = 0;
    m_inspectedPrefabPath.clear();
    m_prefabData = {};
    m_componentsToDelete.clear();
}

// Shows all components of a selected entity instance
void InspectorWindow::_renderEntityCore() {
    // Nothing to render if no entity selected
    if (!m_world) {
        ImGui::TextDisabled("No entity selected");
        return;
    }

    // Selected ID does not correspond to a valid entity
    ECS::Entity entity{ m_inspectedEntityId, 0 };
    if (!m_world->IsAlive(entity)) {
        ImGui::TextDisabled("Entity invalid");
        return;
    }

    // Get entity name from Name component
    const char* entityName = "Unnamed";
    if (m_world->Has<ECS::Components::Name>(entity)) {
        entityName = m_world->Get<ECS::Components::Name>(entity).Value;
    }

    // Header: Show entity name and ID
    ImGui::Text("Entity ");
    ImGui::SameLine();
    ImGui::TextDisabled("%s (ID: %u)", entityName, (unsigned)m_inspectedEntityId);

    // If entity has PrefabLink component, it means this entity was created from a prefab
    // Show which prefab it came from and offer to open that prefab for editing
    if (m_world->Has<ECS::Components::PrefabLink>(entity)) {
        const auto& link = m_world->Get<ECS::Components::PrefabLink>(entity);
        ImGui::Separator();
        ImGui::Text("Prefab Instance");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", std::filesystem::path(link.prefabPath).filename().string().c_str());

        // "Open" button switches inspector to Prefab Editor mode
        // This lets us edit the template that this instance came from
        ImGui::SameLine();
        if (ImGui::Button("Open Prefab")) {
            InspectPrefab(link.prefabPath);
        }

        // Tooltip explaining the relationship between instance and template
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Opens the prefab template file for editing.\nChanges to prefab will update ALL instances.");
        }

        ImGui::Separator();
    }
    else {
        // Entity is NOT a prefab instance yet
        // Show drag-drop zone where we can drop a .prefab file to convert this into an instance
        ImGui::Separator();
        ImGui::Text("Prefab Link");
        ImGui::SameLine();
        ImGui::TextDisabled("None (drag .prefab here to link)");

        // Accept drag-drop of .prefab file to add PrefabLink component
        // This turns a regular entity into a prefab instance
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string droppedPath = static_cast<const char*>(payload->Data);
                if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                    // Add PrefabLink component to mark this as a prefab instance
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

    // Use scrollable child region for components list
    // Always show both scrollbars to prevent annoying layout jitter when content changes size
    // ImGuiWindowFlags childFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar;
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    // Add more padding inside the child to widen the content area visually
    // and provide a comfortable right margin for the action icon
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));

    // Enable horizontal scrollbar (only appears when content exceeds width)
    ImGui::BeginChild("EntityComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Entity -> JSON (for unified editing)
    // Uses EntitySerializer (iterates registered components into JSON)
    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

    if (entityJson.contains("Components")) {
        ImGui::Dummy(ImVec2(0, 4));  // Top padding

        // ALWAYS render Transform first
        // Transform can't be deleted, so we pass false for the canDelete parameter
        for (auto& componentEntry : entityJson["Components"]) {
            if (componentEntry["TypeName"] == "ECS::Components::LocalTransform") {
                _renderComponentSection("Transform", "LocalTransform", componentEntry["Data"],
                    [this](nlohmann::json& d) { m_componentUI.RenderLocalTransform(d); }, false);
                ImGui::Dummy(ImVec2(0, 4));
                break;
            }
        }

        // Render remaining components vertically, one section per line
        // Place Camera3D near the top (right after Transform) for quick access
        const std::vector<std::string> orderedTypes = {
            "ECS::Components::Camera3D",
            "ECS::Components::SpriteRenderer2D",
            "ECS::Components::Rigidbody2D",
            "ECS::Components::CircleCollider2D",
            "ECS::Components::BoxCollider2D",
            "ECS::Components::ShapeCircle2D",
            "ECS::Components::ShapeBox2D",
            "ECS::Components::ShapeLine2D"
        };

        for (const auto& type : orderedTypes) {
            for (auto& componentEntry : entityJson["Components"]) {
                if (componentEntry["TypeName"] != type) continue;
                auto& data = componentEntry["Data"];

                if (type == "ECS::Components::SpriteRenderer2D") {
                    _renderComponentSection("Sprite Renderer", "SpriteRenderer2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderSpriteRenderer2D(d); });
                }
                else if (type == "ECS::Components::Rigidbody2D") {
                    _renderComponentSection("Rigidbody 2D", "Rigidbody2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderRigidbody2D(d); });
                }
                else if (type == "ECS::Components::CircleCollider2D") {
                    _renderComponentSection("Circle Collider 2D", "CircleCollider2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderCircleCollider2D(d); });
                }
                else if (type == "ECS::Components::BoxCollider2D") {
                    _renderComponentSection("Box Collider 2D", "BoxCollider2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderBoxCollider2D(d); });
                }
                else if (type == "ECS::Components::ShapeCircle2D") {
                    _renderComponentSection("Shape Circle", "ShapeCircle2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderShapeCircle2D(d); });
                }
                else if (type == "ECS::Components::ShapeBox2D") {
                    _renderComponentSection("Shape Box", "ShapeBox2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderShapeBox2D(d); });
                }
                else if (type == "ECS::Components::ShapeLine2D") {
                    _renderComponentSection("Shape Line", "ShapeLine2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderShapeLine2D(d); });
                }
                else if (type == "ECS::Components::Camera3D") {
                    _renderComponentSection("Camera 3D", "Camera3D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderCamera3D(d); });
                }

                ImGui::Dummy(ImVec2(0, 4));
                break; // stop scanning once we rendered this type
            }
        }

        // Apply component deletions that were queued from delete button clicks
        // Queue instead of deleting immediately to avoid iterator invalidation
        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromEntity(type);
        }
        m_componentsToDelete.clear();

        // Write modified JSON data back to the actual live entity components
        // This is where UI changes actually get applied to the game state
    // The UI modifies the JSON, then we deserialize that JSON back into the real components
        for (const auto& componentEntry : entityJson["Components"]) {
            std::string typeName = componentEntry["TypeName"];


            // JSON -> Entity: apply edited data
            // For each entry: match `TypeName`, call `from_json(Data, component)`
            // Adds component when absent; updates fields when present
            // Lambda helper to apply JSON data to the correct component type
            // Returns true if this was the right type, false to keep trying other types
            auto applyComponent = [&]<typename T>(const std::string & name) {
                if (typeName == name) {
                    if (m_world->Has<T>(entity)) {
                        auto& comp = m_world->Get<T>(entity);
                        from_json(componentEntry["Data"], comp);
                    }
                    return true;
                }
                return false;
            };

            // Try each component type
            if (applyComponent.operator() < ECS::Components::LocalTransform > ("ECS::Components::LocalTransform")) continue;
            if (applyComponent.operator() < ECS::Components::SpriteRenderer2D > ("ECS::Components::SpriteRenderer2D")) continue;
            if (applyComponent.operator() < ECS::Components::Rigidbody2D > ("ECS::Components::Rigidbody2D")) continue;
            if (applyComponent.operator() < ECS::Components::CircleCollider2D > ("ECS::Components::CircleCollider2D")) continue;
            if (applyComponent.operator() < ECS::Components::BoxCollider2D > ("ECS::Components::BoxCollider2D")) continue;
            if (applyComponent.operator() < ECS::Components::ShapeCircle2D > ("ECS::Components::ShapeCircle2D")) continue;
            if (applyComponent.operator() < ECS::Components::ShapeBox2D > ("ECS::Components::ShapeBox2D")) continue;
            if (applyComponent.operator() < ECS::Components::ShapeLine2D > ("ECS::Components::ShapeLine2D")) continue;
        }
    }

    // Do not clear selection on generic clicks inside the inspector.
    // This prevents accidental deselection when clicking labels or whitespace
    // near interactive widgets (e.g. checkboxes, drags) and improves usability.

    // Capture scroll extent for stable overlay positioning next frame
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Bottom section: "Add Component" button lives outside the scroll region
    // This keeps it always visible at the bottom, even when scrolling through lots of components
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }
    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

        // List all available component types (order aligned with inspector)
        _renderComponentMenuItem("Transform", "LocalTransform");
        _renderComponentMenuItem("SpriteRenderer", "SpriteRenderer2D");
        _renderComponentMenuItem("Rigidbody2D", "Rigidbody2D");
        _renderComponentMenuItem("CircleCollider2D", "CircleCollider2D");
        _renderComponentMenuItem("BoxCollider2D", "BoxCollider2D");
        _renderComponentMenuItem("ShapeCircle2D", "ShapeCircle2D");
        _renderComponentMenuItem("ShapeBox2D", "ShapeBox2D");
        _renderComponentMenuItem("ShapeLine2D", "ShapeLine2D");
        _renderComponentMenuItem("Camera 3D", "Camera3D");
        ImGui::EndPopup();
    }

    // Removed: Save As Prefab functionality
}

// Shows components of the prefab TEMPLATE (editing the .prefab file directly)
// Any changes made here will automatically propagate to ALL instances when we hit "Apply"
void InspectorWindow::_renderPrefabInspector() {
    if (m_inspectedPrefabPath.empty()) {
        ImGui::TextDisabled("No prefab selected");
        return;
    }

    // Header: Show prefab filename
    ImGui::Text("Editing Prefab Template");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", std::filesystem::path(m_inspectedPrefabPath).filename().string().c_str());

    ImGui::Separator();
    // Warning to make it crystal clear that editing the template affects all instances
    ImGui::TextWrapped("Changes to this prefab will update ALL instances in the scene.");
    ImGui::Separator();

    // Use scrollable child for component list
    // Keep both vertical and horizontal scrollbars always visible to prevent jitter
    // ImGuiWindowFlags childFlags = ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar;
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 3;

    // Padding inside the child for visual spacing (match entity inspector padding)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));

    // Enable horizontal scrollbar (only appears when content exceeds width)
    ImGui::BeginChild("PrefabComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Render all components in the prefab JSON
    if (m_prefabData.contains("Components") && m_prefabData["Components"].is_array()) {
        ImGui::Dummy(ImVec2(0, 4));  // Top padding

        // ALWAYS render Transform first
        for (auto& componentEntry : m_prefabData["Components"]) {
            // Defensive checks
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string())
                continue;
            if (componentEntry["TypeName"] == "ECS::Components::LocalTransform") {
                if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object())
                    componentEntry["Data"] = nlohmann::json::object();
                auto& dataObj = componentEntry["Data"];
                _renderComponentSection("Transform", "LocalTransform", dataObj,
                    [this](nlohmann::json& d) { m_componentUI.RenderLocalTransform(d); }, false);
                ImGui::Dummy(ImVec2(0, 4));
                break;
            }
        }

        // Render remaining prefab components vertically, one section per line
        // Move Camera3D near the top (just after Transform) for better visibility
        const std::vector<std::string> orderedTypes = {
            "ECS::Components::Camera3D",
            "ECS::Components::SpriteRenderer2D",
            "ECS::Components::Rigidbody2D",
            "ECS::Components::CircleCollider2D",
            "ECS::Components::BoxCollider2D",
            "ECS::Components::ShapeCircle2D",
            "ECS::Components::ShapeBox2D",
            "ECS::Components::ShapeLine2D"
        };

        for (const auto& type : orderedTypes) {
            for (auto& componentEntry : m_prefabData["Components"]) {
                if (componentEntry["TypeName"] != type) continue;
                auto& data = componentEntry["Data"];

                if (type == "ECS::Components::SpriteRenderer2D") {
                    _renderComponentSection("Sprite Renderer", "SpriteRenderer2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderSpriteRenderer2D(d); });
                }
                else if (type == "ECS::Components::Rigidbody2D") {
                    _renderComponentSection("Rigidbody 2D", "Rigidbody2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderRigidbody2D(d); });
                }
                else if (type == "ECS::Components::CircleCollider2D") {
                    _renderComponentSection("Circle Collider 2D", "CircleCollider2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderCircleCollider2D(d); });
                }
                else if (type == "ECS::Components::BoxCollider2D") {
                    _renderComponentSection("Box Collider 2D", "BoxCollider2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderBoxCollider2D(d); });
                }
                else if (type == "ECS::Components::ShapeCircle2D") {
                    _renderComponentSection("Shape Circle", "ShapeCircle2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderShapeCircle2D(d); });
                }
                else if (type == "ECS::Components::ShapeBox2D") {
                    _renderComponentSection("Shape Box", "ShapeBox2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderShapeBox2D(d); });
                }
                else if (type == "ECS::Components::ShapeLine2D") {
                    _renderComponentSection("Shape Line", "ShapeLine2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderShapeLine2D(d); });
                }
                else if (type == "ECS::Components::Camera3D") {
                    _renderComponentSection("Camera 3D", "Camera3D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderCamera3D(d); });
                }

                ImGui::Dummy(ImVec2(0, 4));
                break; // Stop scanning once we rendered this type
            }
        }

        // Process component deletions
        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromPrefab(type);
        }
        m_componentsToDelete.clear();
    }

    // Clicking empty space in the PrefabComponents child clears current selection
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered()) {
        ClearSelection();
    }

    // Capture scroll extent for stable overlay positioning next frame
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Bottom buttons (outside scroll region)
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }

    // Begin "Add Component" popup menu
    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

        // Render menu items for each component type
        _renderComponentMenuItem("Transform", "LocalTransform");
        _renderComponentMenuItem("Sprite Renderer", "SpriteRenderer2D");
        _renderComponentMenuItem("Rigidbody 2D", "Rigidbody2D");
        _renderComponentMenuItem("Circle Collider 2D", "CircleCollider2D");
        _renderComponentMenuItem("Box Collider 2D", "BoxCollider2D");
        _renderComponentMenuItem("Shape Circle", "ShapeCircle2D");
        _renderComponentMenuItem("Shape Box", "ShapeBox2D");
        _renderComponentMenuItem("Shape Line", "ShapeLine2D");
        _renderComponentMenuItem("Camera 3D", "Camera3D");
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Removed: Apply Changes/save template functionality
}

// Add component to an entity instance
void InspectorWindow::_addComponentToEntity(const std::string& componentType) {
    if (!m_world) return;
    ECS::Entity entity{ m_inspectedEntityId, 0 };
    if (!m_world->IsAlive(entity)) return;

    // Transform is special, every entity has one by default and it has no configurable defaults
    if (componentType == "LocalTransform") {
        if (!m_world->Has<ECS::Components::LocalTransform>(entity)) {
            m_world->Add<ECS::Components::LocalTransform>(entity);
        }
        return;
    }

    // Get default component data from our defaults table
    nlohmann::json defaults = _getDefaultComponentData(componentType);

    // Add component to entity and apply the default values
    // Use a lambda pattern to handle all component types uniformly
    auto applyDefault = [&]<typename T>(const std::string & name) {
        if (componentType == name) {
            if (!m_world->Has<T>(entity)) {
                m_world->Add<T>(entity);
            }
            if (m_world->Has<T>(entity)) {
                auto& comp = m_world->Get<T>(entity);
                from_json(defaults, comp);
            }
            return true;
        }
        return false;
    };

    // Try each component type
    if (applyDefault.operator() < ECS::Components::LocalTransform > ("LocalTransform")) return;
    if (applyDefault.operator() < ECS::Components::SpriteRenderer2D > ("SpriteRenderer2D")) return;
    if (applyDefault.operator() < ECS::Components::Rigidbody2D > ("Rigidbody2D")) return;
    if (applyDefault.operator() < ECS::Components::CircleCollider2D > ("CircleCollider2D")) return;
    if (applyDefault.operator() < ECS::Components::BoxCollider2D > ("BoxCollider2D")) return;
    if (applyDefault.operator() < ECS::Components::ShapeCircle2D > ("ShapeCircle2D")) return;
    if (applyDefault.operator() < ECS::Components::ShapeBox2D > ("ShapeBox2D")) return;
    if (applyDefault.operator() < ECS::Components::ShapeLine2D > ("ShapeLine2D")) return;
}

// Remove component from entity instance
void InspectorWindow::_removeComponentFromEntity(const std::string& componentType) {
    if (!m_world) return;
    ECS::Entity entity{ m_inspectedEntityId, 0 };
    if (!m_world->IsAlive(entity)) return;

    // Generic lambda to remove component T if type matches
    auto remove = [&]<typename T>(const std::string & name) {
        // Check if current component matches type
        if (componentType == name) {
            // Remove component from entity
            m_world->Remove<T>(entity);
            m_statusMessage = std::string("Removed ") + componentType + " from entity";
            m_statusTimer = 2.0f;
            return true;
        }
        return false;
    };

    // Transform cannot be removed, every entity needs one
    if (componentType == "LocalTransform") return;

    // Try removing each component type
    if (remove.operator() < ECS::Components::SpriteRenderer2D > ("SpriteRenderer2D")) return;
    if (remove.operator() < ECS::Components::Rigidbody2D > ("Rigidbody2D")) return;
    if (remove.operator() < ECS::Components::CircleCollider2D > ("CircleCollider2D")) return;
    if (remove.operator() < ECS::Components::BoxCollider2D > ("BoxCollider2D")) return;
    if (remove.operator() < ECS::Components::ShapeCircle2D > ("ShapeCircle2D")) return;
    if (remove.operator() < ECS::Components::ShapeBox2D > ("ShapeBox2D")) return;
    if (remove.operator() < ECS::Components::ShapeLine2D > ("ShapeLine2D")) return;
}

// Add component to prefab template JSON
void InspectorWindow::_addComponentToPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) {
        m_prefabData["Components"] = nlohmann::json::array();
    }

    // Don't add duplicates, check if this component type already exists
    if (_prefabHasComponent(componentType)) return;

    // Get default data for component + add component to JSON
    nlohmann::json data = _getDefaultComponentData(componentType);
    m_prefabData["Components"].push_back({ {"TypeName", componentType}, {"Data", data} });
}

// Remove component from prefab template JSON
void InspectorWindow::_removeComponentFromPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return;

    auto& components = m_prefabData["Components"];
    // Find component of given type
    for (auto it = components.begin(); it != components.end(); it++) {
        // Remove it from JSON
        if ((*it)["TypeName"] == componentType) {
            components.erase(it);
            m_statusMessage = std::string("Component removed: ") + componentType;
            m_statusTimer = 2.0f;
            return;
        }
    }
}

// Check if entity has component
bool InspectorWindow::_entityHasComponent(EntityId id, const std::string& componentType) {
    ECS::Entity entity{ id, 0 };
    if (!m_world->IsAlive(entity)) return false;

    // Lambda to check component type
    // Return true if entity has this component
    auto has = [&]<typename T>(const std::string & name) {
        if (componentType == name) return m_world->Has<T>(entity);
        return false;
    };

    // Check if the entity has the specified component type; returning true if found, false otherwise
    if (has.operator() < ECS::Components::LocalTransform > ("LocalTransform")) return true;
    if (has.operator() < ECS::Components::SpriteRenderer2D > ("SpriteRenderer2D")) return true;
    if (has.operator() < ECS::Components::Rigidbody2D > ("Rigidbody2D")) return true;
    if (has.operator() < ECS::Components::CircleCollider2D > ("CircleCollider2D")) return true;
    if (has.operator() < ECS::Components::BoxCollider2D > ("BoxCollider2D")) return true;
    if (has.operator() < ECS::Components::ShapeCircle2D > ("ShapeCircle2D")) return true;
    if (has.operator() < ECS::Components::ShapeLine2D > ("ShapeLine2D")) return true;

    return false;
}

// Check if prefab JSON contains a component of a given type
bool InspectorWindow::_prefabHasComponent(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return false; // If prefab has no "Components" array, return false
    for (const auto& comp : m_prefabData["Components"]) {   // Iterate through each component in the prefab
        if (comp["TypeName"] == componentType) return true;     // If component type matches, return true
    }
    return false;                                           // No matching component found, return false
}

// Get default JSON data for a component type
// Keys MUST match EntitySerializer.h's NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE macros EXACTLY
// If the keys don't match, deserialization will silently fail or crash
nlohmann::json InspectorWindow::_getDefaultComponentData(const std::string& componentType) {
    // LocalTransform: Position, Rotation, Scale
    if (componentType == "LocalTransform") {
        return {
            {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
            {"Rotation", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 1.0f}}},
            {"Scale", {{"X", 1.0f}, {"Y", 1.0f}, {"Z", 1.0f}}}
        };
    }
    // SpriteRenderer2D: TextureId, Color, Tiling, Offset, Width, Height
    else if (componentType == "SpriteRenderer2D") {
        return {
            {"TextureId", 0},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Tiling", {{"X", 1.0f}, {"Y", 1.0f}}},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Width", 0},
            {"Height", 0}
        };
    }
    // Rigidbody2D: Mass, InverseMass, LinearDamping, AngularDamping, GravityScale, Flags
    else if (componentType == "Rigidbody2D") {
        return {
            {"Mass", 1.0f},
            {"InverseMass", 1.0f},
            {"LinearDamping", 0.0f},
            {"AngularDamping", 0.0f},
            {"GravityScale", 1.0f},
            {"Flags", 0}
        };
    }
    // CircleCollider2D: Radius, Offset, LayerMask, Flags
    else if (componentType == "CircleCollider2D") {
        return {
            {"Radius", 0.5f},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"LayerMask", 0xFFFFFFFFu},
            {"Flags", 0}
        };
    }
    // Camera3D: projection settings
    else if (componentType == "Camera3D") {
        return {
            {"UsePerspective", false},
            {"FOV", 45.0f},
            {"NearPlane", 0.1f},
            {"FarPlane", 100.0f},
            {"OrthoSize", 10.0f},
            {"AspectRatio", 16.0f/9.0f},
            {"Active", false}
        };
    }
    // BoxCollider2D: HalfExtents, Offset, Rotation, LayerMask, Flags
    else if (componentType == "BoxCollider2D") {
        return {
            {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Rotation", 0.0f},
            {"LayerMask", 0xFFFFFFFFu},
            {"Flags", 0}
        };
    }
    // ShapeCircle2D: Radius, Offset, Color, Thickness, Filled
    else if (componentType == "ShapeCircle2D") {
        return {
            {"Radius", 0.5f},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Thickness", 1.0f},
            {"Filled", false}
        };
    }
    // ShapeBox2D: HalfExtents, Offset, Color, Thickness, Filled
    else if (componentType == "ShapeBox2D") {
        return {
            {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Thickness", 1.0f},
            {"Filled", false}
        };
    }
    // ShapeLine2D: A, B, Color, Thickness
    else if (componentType == "ShapeLine2D") {
        return {
            {"A", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"B", {{"X", 1.0f}, {"Y", 0.0f}}},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Thickness", 1.0f}
        };
    }

    return {};
}

// Renders a collapsible inspector section for a component, including its editable fields and an optional delete button.
template <typename T>
void InspectorWindow::_renderComponentSection(const std::string& headerName, const std::string& componentType,
    nlohmann::json& data, T renderContent, bool canDelete) {

    // Render a framed, full-width collapsing header so the bar spans content width
    bool nodeOpen = ImGui::CollapsingHeader(
        headerName.c_str(),
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth
    );

    // Render delete/action row below the header only when expanded.
    // Save and restore the cursor so component content doesn't get pushed down.
    if (nodeOpen) {
        const char* deleteIcon = "\xEE\xA1\xB2";

        // Capture current cursor before drawing the icon row
        ImVec2 contentCursorPos = ImGui::GetCursorPos();

        // Compute button width and align to the visible right edge of the current row,
        // taking horizontal scrolling into account
        ImGui::PushFont(m_symbolsFont);
        float iconWidth = ImGui::CalcTextSize(deleteIcon).x;
        float btnWidth = iconWidth + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::PopFont();
        float rightVisible = ImGui::GetWindowContentRegionMax().x + ImGui::GetScrollX();
        ImGui::SetCursorPosX(rightVisible - btnWidth);

        // Disabled scope prevents removal when not allowed (e.g., Transform)
        if (!canDelete) ImGui::BeginDisabled();
        ImGui::PushFont(m_symbolsFont);
        // Style overrides: make icon button look flat; tint text red when deletable, gray when not
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, canDelete ? ImVec4(0.7f, 0.2f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        bool clicked = ImGui::SmallButton((std::string(deleteIcon) + "##Delete" + componentType).c_str());
        // Restore the four style colors overridden above
        ImGui::PopStyleColor(4);
        ImGui::PopFont();
        if (!canDelete) ImGui::EndDisabled();

        // Allow tooltip when disabled to explain why deletion isn't possible
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(canDelete ? "Remove Component" : "Transform cannot be removed");
        }

        if (clicked && canDelete) {
            m_componentsToDelete.push_back(componentType);
        }

        // No artificial padding; icon should hug the visible right edge

        // Restore cursor so component content renders at the original position
        ImGui::SetCursorPos(contentCursorPos);
        // Add a small vertical gap between the header and the first field label
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
        // Render component content directly under the header after the spacer
        renderContent(data);
    }
    // Note: Avoid double-rendering to prevent duplicated UI rows
}

// Render menu item for adding component
void InspectorWindow::_renderComponentMenuItem(const char* displayName, const char* componentType) {
    bool hasComponent = false;

    // Check if the component already exists (either on entity or in prefab)
    if (m_mode == InspectionMode::Entity) {
        hasComponent = _entityHasComponent(m_inspectedEntityId, componentType);
    }
    else if (m_mode == InspectionMode::Prefab) {
        hasComponent = _prefabHasComponent(componentType);
    }

    // Gray out the menu item if component already added
    if (hasComponent) ImGui::BeginDisabled();

    // More stylistic stuff
    if (ImGui::Selectable(displayName)) {
        if (m_mode == InspectionMode::Entity) _addComponentToEntity(componentType);
        else if (m_mode == InspectionMode::Prefab) _addComponentToPrefab(componentType);
    }

    if (hasComponent) ImGui::EndDisabled();

    if (hasComponent && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Component already added");
    }
}
