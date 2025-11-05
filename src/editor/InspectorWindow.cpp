/* Start Header *****************************************************************/
/*!
\file   InspectorWindow.cpp
\author Foo Rui Qin (70%)
        Samantha Leong (30%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Implements the unified InspectorWindow that adapts to selection context:
entities, prefabs, and general assets. Absorbs PrefabEditor functionality.
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

void InspectorWindow::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
    ComponentUI::Initialize(mainFont, boldFont, symbolsFont);
}

void InspectorWindow::Render(float fontScale) {
    ImGui::PushFont(m_mainFont);
    // Match OverlayService docking layout window titles
    const char* windowTitle = (m_mode == InspectionMode::Prefab) ? "Prefab Editor" : "Property Editor";
    ImGui::Begin(windowTitle);
    ImGui::SetWindowFontScale(fontScale);

    if (m_mode == InspectionMode::None) {
        ImGui::TextDisabled("No selection");
    }
    else if (m_mode == InspectionMode::Entity) {
        _renderEntityInspector();
    }
    else if (m_mode == InspectionMode::Prefab) {
        _renderPrefabInspector();
    }

    // Status toast
    if (m_statusTimer > 0.0f) {
        ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
            ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        ImGui::Separator();
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());
        m_statusTimer -= ImGui::GetIO().DeltaTime;
    }

    ImGui::End();
    ImGui::PopFont();
}

void InspectorWindow::InspectEntity(EntityId id) {
    m_inspectedEntityId = id;
    m_mode = id ? InspectionMode::Entity : InspectionMode::None;
}

void InspectorWindow::InspectPrefab(const std::string& prefabPath) {
    if (prefabPath.empty()) {
        m_statusMessage = "Failed: No prefab path";
        m_statusTimer = 3.0f;
        return;
    }
    std::ifstream file(prefabPath);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot open prefab";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot open prefab file: " << prefabPath);
        return;
    }
    try {
        m_prefabData = nlohmann::json::parse(file);
        file.close();
        m_inspectedPrefabPath = prefabPath;
        m_mode = InspectionMode::Prefab;
        m_statusMessage = "Opened prefab";
        m_statusTimer = 2.0f;
    }
    catch (const std::exception& e) {
        m_statusMessage = std::string("Failed: Parse prefab ") + e.what();
        m_statusTimer = 3.0f;
        m_mode = InspectionMode::None;
    }
}

void InspectorWindow::ClearSelection() {
    m_mode = InspectionMode::None;
    m_inspectedEntityId = 0;
    m_inspectedPrefabPath.clear();
    m_prefabData = {};
    m_componentsToDelete.clear();
}

// === Private: Entity inspection ===
void InspectorWindow::_renderEntityInspector() {
    if (!m_world || m_inspectedEntityId == 0) {
        ImGui::TextDisabled("No entity selected");
        return;
    }
    auto entity = m_world->GetEntityManager().GetEntity(m_inspectedEntityId);
    if (entity.GetId() == 0) {
        ImGui::TextDisabled("Entity invalid");
        return;
    }

    ImGui::Text("Entity ");
    ImGui::SameLine();
    ImGui::TextDisabled("%s (ID: %u)", entity.GetName().c_str(), (unsigned)m_inspectedEntityId);

    // Prefab header if entity links to a prefab
    if (auto* link = entity.GetComponent<Component::PrefabLink>()) {
        ImGui::Text("Prefab");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", std::filesystem::path(link->prefabPath).filename().string().c_str());
        ImGui::SameLine();
        if (ImGui::Button("Open")) {
            InspectPrefab(link->prefabPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply")) {
            try {
                // Load prefab JSON
                std::ifstream in(link->prefabPath);
                nlohmann::json prefabJson;
                in >> prefabJson;
                in.close();

                // Instantiate prefab entity
                Entity child = Serialization::EntitySerializer::DeserializeEntity(*m_world, prefabJson);

                // Parent under the inspected entity so it shows in Hierarchy
                if (auto* t = child.GetComponent<Component::Transform>()) {
                    t->ParentId = m_inspectedEntityId;
                }

                m_statusMessage = "Prefab instance created under entity";
                m_statusTimer = 2.0f;
            }
            catch (const std::exception& e) {
                m_statusMessage = std::string("Failed: Apply prefab ") + e.what();
                m_statusTimer = 3.0f;
            }
        }
    }
    else {
        // Show attach prompt and accept drag-drop of a .prefab to attach PrefabLink
        ImGui::Text("Prefab");
        ImGui::SameLine();
        ImGui::TextDisabled("None");
        ImGui::SameLine();
        ImGui::TextDisabled("(drag a .prefab here)");

        if (ImGui::BeginDragDropTarget()) {
            ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0.2f, 0.5f, 1.0f, 1.0f));
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string droppedPath = static_cast<const char*>(payload->Data);
                if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                    entity.AddComponent<Component::PrefabLink>(droppedPath);
                    m_statusMessage = "Prefab attached";
                    m_statusTimer = 2.0f;
                }
                else {
                    m_statusMessage = "Not a prefab: drop a .prefab";
                    m_statusTimer = 2.0f;
                }
            }
            ImGui::PopStyleColor();
            ImGui::EndDragDropTarget();
        }
    }

    // Removed the separator under Prefab for a cleaner look
    ImGui::BeginChild("EntityComponents", ImVec2(0, 0), false);

    // Serialize entity -> JSON for unified component UI
    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(entity);
    if (entityJson.contains("Components")) {
        for (auto& componentEntry : entityJson["Components"]) {
            std::string componentType = componentEntry["Type"];
            auto& data = componentEntry["Data"];

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

        // Apply deletions
        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromEntity(type);
        }
        m_componentsToDelete.clear();

        // Write back modified JSON to live components
        for (const auto& componentEntry : entityJson["Components"]) {
            std::string typeName = componentEntry["Type"];
            auto applyComponent = [&]<typename T>(const std::string& name) {
                if (typeName == name) {
                    if (auto* comp = entity.GetComponent<T>()) {
                        from_json(componentEntry["Data"], *comp);
                    }
                    return true;
                }
                return false;
            };
            if (applyComponent.operator()<Component::Transform>("Transform")) continue;
            if (applyComponent.operator()<Component::SpriteRenderer>("SpriteRenderer")) continue;
            if (applyComponent.operator()<Component::Rigidbody2D>("Rigidbody2D")) continue;
            if (applyComponent.operator()<Component::CircleCollider2D>("CircleCollider2D")) continue;
            if (applyComponent.operator()<Component::BoxCollider2D>("BoxCollider2D")) continue;
            if (applyComponent.operator()<Component::ShapeRenderer2D>("ShapeRenderer2D")) continue;
            if (applyComponent.operator()<Component::LineRenderer>("LineRenderer")) continue;
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }
    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();
        _renderComponentMenuItem("Transform", "Transform");
        _renderComponentMenuItem("Sprite Renderer", "SpriteRenderer");
        _renderComponentMenuItem("Rigidbody 2D", "Rigidbody2D");
        _renderComponentMenuItem("Circle Collider 2D", "CircleCollider2D");
        _renderComponentMenuItem("Box Collider 2D", "BoxCollider2D");
        _renderComponentMenuItem("Shape Renderer 2D", "ShapeRenderer2D");
        _renderComponentMenuItem("Line Renderer", "LineRenderer");
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

// === Private: Prefab inspection ===
void InspectorWindow::_renderPrefabInspector() {
    if (m_inspectedPrefabPath.empty()) {
        ImGui::TextDisabled("No prefab selected");
        return;
    }

    ImGui::Text("Prefab");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", std::filesystem::path(m_inspectedPrefabPath).filename().string().c_str());

    // Render all components in the prefab JSON
    if (m_prefabData.contains("Components")) {
        for (auto& componentEntry : m_prefabData["Components"]) {
            std::string componentType = componentEntry["Type"];
            auto& data = componentEntry["Data"];

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

        // Process deletions
        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromPrefab(type);
        }
        m_componentsToDelete.clear();
    }

    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }
    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();
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
    if (ImGui::Button("Apply")) {
        _savePrefab();
    }
}

// === Component management helpers ===
void InspectorWindow::_addComponentToEntity(const std::string& componentType) {
    if (!m_world || m_inspectedEntityId == 0) return;
    auto entity = m_world->GetEntityManager().GetEntity(m_inspectedEntityId);
    if (componentType == "Transform") {
        if (!entity.GetComponent<Component::Transform>()) {
            entity.AddComponent<Component::Transform>();
        }
        return; // Transform defaults fine
    }

    nlohmann::json defaults = _getDefaultComponentData(componentType);
    auto applyDefault = [&]<typename T>(const std::string& name) {
        if (componentType == name) {
            if (!entity.GetComponent<T>()) {
                entity.AddComponent<T>();
            }
            if (auto* comp = entity.GetComponent<T>()) {
                from_json(defaults, *comp);
            }
            return true;
        }
        return false;
    };
    if (applyDefault.operator()<Component::SpriteRenderer>("SpriteRenderer")) return;
    if (applyDefault.operator()<Component::Rigidbody2D>("Rigidbody2D")) return;
    if (applyDefault.operator()<Component::CircleCollider2D>("CircleCollider2D")) return;
    if (applyDefault.operator()<Component::BoxCollider2D>("BoxCollider2D")) return;
    if (applyDefault.operator()<Component::ShapeRenderer2D>("ShapeRenderer2D")) return;
    if (applyDefault.operator()<Component::LineRenderer>("LineRenderer")) return;
}

void InspectorWindow::_removeComponentFromEntity(const std::string& componentType) {
    if (!m_world || m_inspectedEntityId == 0) return;
    auto& em = m_world->GetEntityManager();
    auto remove = [&]<typename T>(const std::string& name) {
        if (componentType == name) {
            em.RemoveComponent<T>(m_inspectedEntityId);
            m_statusMessage = std::string("Removed ") + componentType + " from entity";
            m_statusTimer = 2.0f;
            return true;
        }
        return false;
    };
    if (componentType == "Transform") return; // cannot remove
    if (remove.operator()<Component::SpriteRenderer>("SpriteRenderer")) return;
    if (remove.operator()<Component::Rigidbody2D>("Rigidbody2D")) return;
    if (remove.operator()<Component::CircleCollider2D>("CircleCollider2D")) return;
    if (remove.operator()<Component::BoxCollider2D>("BoxCollider2D")) return;
    if (remove.operator()<Component::ShapeRenderer2D>("ShapeRenderer2D")) return;
    if (remove.operator()<Component::LineRenderer>("LineRenderer")) return;
}

void InspectorWindow::_addComponentToPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) m_prefabData["Components"] = nlohmann::json::array();
    if (_prefabHasComponent(componentType)) return;
    nlohmann::json data = _getDefaultComponentData(componentType);
    m_prefabData["Components"].push_back({ {"Type", componentType}, {"Data", data} });
}

void InspectorWindow::_removeComponentFromPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return;
    auto& components = m_prefabData["Components"];
    for (auto it = components.begin(); it != components.end(); ++it) {
        if ((*it)["Type"] == componentType) {
            components.erase(it);
            m_statusMessage = std::string("Component removed: ") + componentType;
            m_statusTimer = 2.0f;
            return;
        }
    }
}

bool InspectorWindow::_entityHasComponent(EntityId id, const std::string& componentType) {
    auto entity = m_world->GetEntityManager().GetEntity(id);
    auto has = [&]<typename T>(const std::string& name) {
        if (componentType == name) return entity.GetComponent<T>() != nullptr;
        return false;
    };
    if (componentType == "Transform") return entity.GetComponent<Component::Transform>() != nullptr;
    if (has.operator()<Component::SpriteRenderer>("SpriteRenderer")) return true;
    if (has.operator()<Component::Rigidbody2D>("Rigidbody2D")) return true;
    if (has.operator()<Component::CircleCollider2D>("CircleCollider2D")) return true;
    if (has.operator()<Component::BoxCollider2D>("BoxCollider2D")) return true;
    if (has.operator()<Component::ShapeRenderer2D>("ShapeRenderer2D")) return true;
    if (has.operator()<Component::LineRenderer>("LineRenderer")) return true;
    return false;
}

bool InspectorWindow::_prefabHasComponent(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return false;
    for (const auto& comp : m_prefabData["Components"]) {
        if (comp["Type"] == componentType) return true;
    }
    return false;
}

nlohmann::json InspectorWindow::_getDefaultComponentData(const std::string& componentType) {
    if (componentType == "Transform") {
        return {
            {"Position", { {"x", 0.0f}, {"y", 0.0f} }},
            {"Rotation", 0.0f},
            {"Scale",    { {"x", 1.0f}, {"y", 1.0f} }}
        };
    }
    else if (componentType == "SpriteRenderer") {
        return {
            {"TexturePath", ""},
            {"Color", { {"r", 1.0f}, {"g", 1.0f}, {"b", 1.0f}, {"a", 1.0f} }},
            {"FlipX", false},
            {"FlipY", false},
            {"SortingLayer", 0},
            {"OrderInLayer", 0}
        };
    }
    else if (componentType == "Rigidbody2D") {
        return {
            {"Mass", 1.0f},
            {"Velocity", { {"x", 0.0f}, {"y", 0.0f} }},
            {"BodyType", "Dynamic"}
        };
    }
    else if (componentType == "CircleCollider2D") {
        return {
            {"Radius", 0.5f},
            {"Offset", { {"x", 0.0f}, {"y", 0.0f} }},
            {"IsTrigger", false}
        };
    }
    else if (componentType == "BoxCollider2D") {
        return {
            {"Size", { {"x", 1.0f}, {"y", 1.0f} }},
            {"Offset", { {"x", 0.0f}, {"y", 0.0f} }},
            {"IsTrigger", false}
        };
    }
    else if (componentType == "ShapeRenderer2D") {
        return {
            {"Type", "Circle"},
            {"FillColor", { {"r", 1.0f}, {"g", 1.0f}, {"b", 1.0f}, {"a", 1.0f} }},
            {"Radius", 0.5f}
        };
    }
    else if (componentType == "LineRenderer") {
        return {
            {"Start", { {"x", 0.0f}, {"y", 0.0f} }},
            {"End", { {"x", 1.0f}, {"y", 1.0f} }},
            {"Thickness", 1.0f}
        };
    }
    return {};
}

template <typename T>
void InspectorWindow::_renderComponentSection(const std::string& headerName, const std::string& componentType,
    nlohmann::json& data, T renderContent, bool canDelete) {
    bool nodeOpen = ImGui::CollapsingHeader(headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    ImVec2 originalCursorPos = ImGui::GetCursorPos();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 50);
    if (!canDelete) ImGui::BeginDisabled();
    ImGui::PushFont(m_symbolsFont);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, canDelete ? ImVec4(0.7f, 0.2f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    if (ImGui::SmallButton((std::string("\xEE\xA1\xB2") + "##Delete" + componentType).c_str())) {
        m_componentsToDelete.push_back(componentType);
    }
    ImGui::PopStyleColor(4);
    ImGui::PopFont();
    if (!canDelete) ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip(canDelete ? "Remove Component" : "Transform cannot be removed");
    }
    ImGui::SetCursorPos(originalCursorPos);
    if (nodeOpen) {
        renderContent(data);
    }
}

void InspectorWindow::_renderComponentMenuItem(const char* displayName, const char* componentType) {
    bool hasComponent = false;
    if (m_mode == InspectionMode::Entity) {
        hasComponent = _entityHasComponent(m_inspectedEntityId, componentType);
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

void InspectorWindow::_savePrefab() {
    if (m_inspectedPrefabPath.empty()) return;
    try {
        std::ofstream out(m_inspectedPrefabPath);
        out << m_prefabData.dump(4);
        out.close();
        m_statusMessage = "Prefab saved";
        m_statusTimer = 2.0f;

        // Update instances
        auto updateEntityFromPrefab = [&](Entity& entity) {
            try {
                for (const auto& componentEntry : m_prefabData["Components"]) {
                    std::string typeName = componentEntry["Type"];
                    auto apply = [&]<typename T>(const std::string& name) {
                        if (typeName == name) {
                            if (auto* comp = entity.GetComponent<T>()) {
                                from_json(componentEntry["Data"], *comp);
                            }
                            return true;
                        }
                        return false;
                    };
                    if (apply.operator()<Component::Transform>("Transform")) continue;
                    if (apply.operator()<Component::SpriteRenderer>("SpriteRenderer")) continue;
                    if (apply.operator()<Component::Rigidbody2D>("Rigidbody2D")) continue;
                    if (apply.operator()<Component::CircleCollider2D>("CircleCollider2D")) continue;
                    if (apply.operator()<Component::BoxCollider2D>("BoxCollider2D")) continue;
                    if (apply.operator()<Component::ShapeRenderer2D>("ShapeRenderer2D")) continue;
                    if (apply.operator()<Component::LineRenderer>("LineRenderer")) continue;
                }
                return true;
            } catch (...) { return false; }
        };

        if (m_world) {
            int updated = 0;
            for (auto id : m_world->GetEntityManager().GetAllEntities()) {
                auto entity = m_world->GetEntityManager().GetEntity(id);
                auto* link = entity.GetComponent<Component::PrefabLink>();
                if (!link || link->prefabPath != m_inspectedPrefabPath) continue;
                if (updateEntityFromPrefab(entity)) ++updated;
            }
            LOG_INFO("Updated " << updated << " prefab instances");
        }
    }
    catch (const std::exception& e) {
        m_statusMessage = std::string("Failed: Save prefab ") + e.what();
        m_statusTimer = 3.0f;
    }
}