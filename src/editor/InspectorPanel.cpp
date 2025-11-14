/* Start Header *****************************************************************/
/*!
\file   InspectorPanel.cpp
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Implements the unified InspectorPanel that adapts to selection context:
- Entity mode: Shows components of a selected entity instance
- Prefab mode: Shows components of a prefab template (editing the .prefab file)

RESTORED FEATURES:
- Proper component rendering with delete buttons
- Full "Add Component" menu with all component types
- Entity-prefab linking with "Open Prefab" button
- Working prefab inspector with save/apply functionality
- Status messages and proper UI layout
- Hash-based change tracking for prefabs
- Proper width and padding
- Footer positioning fixed
*/
/* End Header *******************************************************************/

#include "../editor/InspectorPanel.h"
#include "../editor/ComponentInspectorUI.h"
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
    m_mode = m_world->IsAlive(e) ? InspectionMode::Entity : InspectionMode::None;
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
        m_prefabData = nlohmann::json::parse(file);
        file.close();
        m_prefabPath = path;
        m_lastSavedPrefabHash = std::hash<std::string>{}(m_prefabData.dump());
        m_mode = InspectionMode::Prefab;
    }
    catch (const std::exception& e) {
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
            ImGui::SetTooltip("Opens the prefab template file for editing.\nChanges to prefab will update ALL instances.");
        }

        ImGui::Separator();
    }
    else {
        // Show drag-drop zone for adding prefab link
        ImGui::Separator();
        ImGui::Text("Prefab Link");
        ImGui::SameLine();
        ImGui::TextDisabled("None (drag .prefab here to link)");

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string droppedPath = static_cast<const char*>(payload->Data);
                if (std::filesystem::path(droppedPath).extension() == ".prefab") {
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
    // Calculate proper child height
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    // Add padding for better layout
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("EntityComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Serialize entity to JSON for unified editing
    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

    if (entityJson.contains("Components")) {
        ImGui::Dummy(ImVec2(0, 4));

        // ALWAYS render Transform first (cannot be deleted)
        for (auto& componentEntry : entityJson["Components"]) {
            if (componentEntry["TypeName"] == "ECS::Components::LocalTransform") {
                _renderComponentSection("Transform", "LocalTransform", componentEntry["Data"],
                    [this](nlohmann::json& d) { m_componentUI.RenderLocalTransform(d); }, false);
                ImGui::Dummy(ImVec2(0, 4));
                break;
            }
        }

        // Render other components in preferred order
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
                break;
            }
        }

        // Process component deletions
        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromEntity(type);
        }
        m_componentsToDelete.clear();

        // Write modified JSON back to entity components
        for (const auto& componentEntry : entityJson["Components"]) {
            std::string typeName = componentEntry["TypeName"];

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

            if (applyComponent.operator() < ECS::Components::LocalTransform > ("ECS::Components::LocalTransform")) continue;
            if (applyComponent.operator() < ECS::Components::SpriteRenderer2D > ("ECS::Components::SpriteRenderer2D")) continue;
            if (applyComponent.operator() < ECS::Components::Rigidbody2D > ("ECS::Components::Rigidbody2D")) continue;
            if (applyComponent.operator() < ECS::Components::CircleCollider2D > ("ECS::Components::CircleCollider2D")) continue;
            if (applyComponent.operator() < ECS::Components::BoxCollider2D > ("ECS::Components::BoxCollider2D")) continue;
            if (applyComponent.operator() < ECS::Components::ShapeCircle2D > ("ECS::Components::ShapeCircle2D")) continue;
            if (applyComponent.operator() < ECS::Components::ShapeBox2D > ("ECS::Components::ShapeBox2D")) continue;
            if (applyComponent.operator() < ECS::Components::ShapeLine2D > ("ECS::Components::ShapeLine2D")) continue;
            if (applyComponent.operator() < ECS::Components::Camera3D > ("ECS::Components::Camera3D")) continue;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void InspectorPanel::_renderAddComponentButton(ECS::Entity entity) {
    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }

    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

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
    ImGui::Text("Editing Prefab Template");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", std::filesystem::path(m_prefabPath).filename().string().c_str());

    ImGui::Separator();
    ImGui::TextWrapped("Changes to this prefab will update ALL instances in the scene");
    ImGui::Separator();
}

void InspectorPanel::_renderPrefabComponents() {
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 3;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("PrefabComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (m_prefabData.contains("Components") && m_prefabData["Components"].is_array()) {
        ImGui::Dummy(ImVec2(0, 4));

        // ALWAYS render Transform first
        for (auto& componentEntry : m_prefabData["Components"]) {
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

        // Render other components in preferred order
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
                break;
            }
        }

        // Process component deletions
        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromPrefab(type);
        }
        m_componentsToDelete.clear();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void InspectorPanel::_renderPrefabActions() {
    ImGui::Separator();

    // Add Component button
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }

    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

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

    // Save Changes button
    if (ImGui::Button("Save Changes")) {
        _savePrefabData();
    }

    ImGui::SameLine();

    // Apply to All Instances button
    if (ImGui::Button("Apply to All Instances")) {
        _savePrefabData();
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

    if (componentType == "LocalTransform") {
        if (!m_world->Has<ECS::Components::LocalTransform>(entity)) {
            m_world->Add<ECS::Components::LocalTransform>(entity);
        }
        return;
    }

    nlohmann::json defaults = _getDefaultComponentData(componentType);

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

    if (applyDefault.operator() < ECS::Components::LocalTransform > ("LocalTransform")) return;
    if (applyDefault.operator() < ECS::Components::SpriteRenderer2D > ("SpriteRenderer2D")) return;
    if (applyDefault.operator() < ECS::Components::Rigidbody2D > ("Rigidbody2D")) return;
    if (applyDefault.operator() < ECS::Components::CircleCollider2D > ("CircleCollider2D")) return;
    if (applyDefault.operator() < ECS::Components::BoxCollider2D > ("BoxCollider2D")) return;
    if (applyDefault.operator() < ECS::Components::ShapeCircle2D > ("ShapeCircle2D")) return;
    if (applyDefault.operator() < ECS::Components::ShapeBox2D > ("ShapeBox2D")) return;
    if (applyDefault.operator() < ECS::Components::ShapeLine2D > ("ShapeLine2D")) return;
    if (applyDefault.operator() < ECS::Components::Camera3D > ("Camera3D")) return;
}

void InspectorPanel::_removeComponentFromEntity(const std::string& componentType) {
    if (!m_world) return;
    ECS::Entity entity{ m_entityId, 0 };
    if (!m_world->IsAlive(entity)) return;

    auto remove = [&]<typename T>(const std::string & name) {
        if (componentType == name) {
            m_world->Remove<T>(entity);
            m_statusMessage = std::string("Removed ") + componentType + " from entity";
            m_statusTimer = 2.0f;
            return true;
        }
        return false;
    };

    if (componentType == "LocalTransform") return;

    if (remove.operator() < ECS::Components::SpriteRenderer2D > ("SpriteRenderer2D")) return;
    if (remove.operator() < ECS::Components::Rigidbody2D > ("Rigidbody2D")) return;
    if (remove.operator() < ECS::Components::CircleCollider2D > ("CircleCollider2D")) return;
    if (remove.operator() < ECS::Components::BoxCollider2D > ("BoxCollider2D")) return;
    if (remove.operator() < ECS::Components::ShapeCircle2D > ("ShapeCircle2D")) return;
    if (remove.operator() < ECS::Components::ShapeBox2D > ("ShapeBox2D")) return;
    if (remove.operator() < ECS::Components::ShapeLine2D > ("ShapeLine2D")) return;
    if (remove.operator() < ECS::Components::Camera3D > ("Camera3D")) return;
}

bool InspectorPanel::_entityHasComponent(EntityId id, const std::string& componentType) {
    ECS::Entity entity{ id, 0 };
    if (!m_world->IsAlive(entity)) return false;

    auto has = [&]<typename T>(const std::string & name) {
        if (componentType == name) return m_world->Has<T>(entity);
        return false;
    };

    if (has.operator() < ECS::Components::LocalTransform > ("LocalTransform")) return true;
    if (has.operator() < ECS::Components::SpriteRenderer2D > ("SpriteRenderer2D")) return true;
    if (has.operator() < ECS::Components::Rigidbody2D > ("Rigidbody2D")) return true;
    if (has.operator() < ECS::Components::CircleCollider2D > ("CircleCollider2D")) return true;
    if (has.operator() < ECS::Components::BoxCollider2D > ("BoxCollider2D")) return true;
    if (has.operator() < ECS::Components::ShapeCircle2D > ("ShapeCircle2D")) return true;
    if (has.operator() < ECS::Components::ShapeBox2D > ("ShapeBox2D")) return true;
    if (has.operator() < ECS::Components::ShapeLine2D > ("ShapeLine2D")) return true;
    if (has.operator() < ECS::Components::Camera3D > ("Camera3D")) return true;

    return false;
}

// -------------------------------------------------------------------------
// Prefab Component Management
// -------------------------------------------------------------------------
void InspectorPanel::_addComponentToPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) {
        m_prefabData["Components"] = nlohmann::json::array();
    }

    if (_prefabHasComponent(componentType)) return;

    nlohmann::json data = _getDefaultComponentData(componentType);
    // Use full typename for prefabs
    std::string fullTypeName = "ECS::Components::" + componentType;
    m_prefabData["Components"].push_back({ {"TypeName", fullTypeName}, {"Data", data} });
}

void InspectorPanel::_removeComponentFromPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return;

    auto& components = m_prefabData["Components"];
    for (auto it = components.begin(); it != components.end(); it++) {
        std::string typeName = (*it)["TypeName"];
        // Match both short and full names
        if (typeName == componentType || typeName == "ECS::Components::" + componentType) {
            components.erase(it);
            m_statusMessage = std::string("Component removed: ") + componentType;
            m_statusTimer = 2.0f;
            return;
        }
    }
}

bool InspectorPanel::_prefabHasComponent(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return false;
    for (const auto& comp : m_prefabData["Components"]) {
        std::string typeName = comp["TypeName"];
        // Match both short and full names
        if (typeName == componentType || typeName == "ECS::Components::" + componentType) return true;
    }
    return false;
}

// -------------------------------------------------------------------------
// Default Component Data
// -------------------------------------------------------------------------
nlohmann::json InspectorPanel::_getDefaultComponentData(const std::string& componentType) {
    if (componentType == "LocalTransform") {
        return {
            {"Position", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}}},
            {"Rotation", {{"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 1.0f}}},
            {"Scale", {{"X", 1.0f}, {"Y", 1.0f}, {"Z", 1.0f}}}
        };
    }
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
    else if (componentType == "CircleCollider2D") {
        return {
            {"Radius", 0.5f},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"LayerMask", 0xFFFFFFFFu},
            {"Flags", 0}
        };
    }
    else if (componentType == "Camera3D") {
        return {
            {"UsePerspective", false},
            {"FOV", 45.0f},
            {"NearPlane", 0.1f},
            {"FarPlane", 100.0f},
            {"OrthoSize", 10.0f},
            {"AspectRatio", 16.0f / 9.0f},
            {"Active", false}
        };
    }
    else if (componentType == "BoxCollider2D") {
        return {
            {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Rotation", 0.0f},
            {"LayerMask", 0xFFFFFFFFu},
            {"Flags", 0}
        };
    }
    else if (componentType == "ShapeCircle2D") {
        return {
            {"Radius", 0.5f},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Thickness", 1.0f},
            {"Filled", false}
        };
    }
    else if (componentType == "ShapeBox2D") {
        return {
            {"HalfExtents", {{"X", 0.5f}, {"Y", 0.5f}}},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Color", {{"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f}}},
            {"Thickness", 1.0f},
            {"Filled", false}
        };
    }
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
        m_statusMessage = "No changes to save";
        m_statusTimer = 2.0f;
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
    m_statusMessage = "Prefab saved successfully";
    m_statusTimer = 2.0f;
}

void InspectorPanel::_applyPrefabToInstances() {
    if (!m_world) return;

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
    if (!m_prefabData.contains("Components")) return;

    // Apply each component from prefab data to the entity
    for (const auto& componentEntry : m_prefabData["Components"]) {
        std::string typeName = componentEntry["TypeName"];

        auto applyComponent = [&]<typename T>(const std::string & name) {
            if (typeName == name) {
                if (!m_world->Has<T>(entity)) {
                    m_world->Add<T>(entity);
                }
                if (m_world->Has<T>(entity)) {
                    auto& comp = m_world->Get<T>(entity);
                    from_json(componentEntry["Data"], comp);
                }
                return true;
            }
            return false;
        };

        if (applyComponent.operator() < ECS::Components::LocalTransform > ("ECS::Components::LocalTransform")) continue;
        if (applyComponent.operator() < ECS::Components::SpriteRenderer2D > ("ECS::Components::SpriteRenderer2D")) continue;
        if (applyComponent.operator() < ECS::Components::Rigidbody2D > ("ECS::Components::Rigidbody2D")) continue;
        if (applyComponent.operator() < ECS::Components::CircleCollider2D > ("ECS::Components::CircleCollider2D")) continue;
        if (applyComponent.operator() < ECS::Components::BoxCollider2D > ("ECS::Components::BoxCollider2D")) continue;
        if (applyComponent.operator() < ECS::Components::ShapeCircle2D > ("ECS::Components::ShapeCircle2D")) continue;
        if (applyComponent.operator() < ECS::Components::ShapeBox2D > ("ECS::Components::ShapeBox2D")) continue;
        if (applyComponent.operator() < ECS::Components::ShapeLine2D > ("ECS::Components::ShapeLine2D")) continue;
        if (applyComponent.operator() < ECS::Components::Camera3D > ("ECS::Components::Camera3D")) continue;
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
