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
#include "../editor/EditorUIHelpers.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace {
    template <typename T>
    bool ApplyComponentIfMatch(ECS::World* world,
                               ECS::Entity entity,
                               const std::string& typeName,
                               const std::string& expectedName,
                               nlohmann::json& componentEntry) {
        if (typeName == expectedName) {
            if (world->Has<T>(entity)) {
                auto& comp = world->Get<T>(entity);
                from_json(componentEntry["Data"], comp);
            }
            return true;
        }
        return false;
    }
}

void InspectorWindow::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
    m_componentUI.Initialize(mainFont, boldFont, symbolsFont);
}

void InspectorWindow::Render(float /*fontScale*/) {
    ImGui::PushFont(m_mainFont);

    const char* windowTitle = (m_mode == InspectionMode::Prefab) ? "Prefab Editor" : "Property Editor";

    // NOTE: removed fixed SetNextWindowContentSize(450, 0) because it caused the
    // inspector content to look "narrow" while the child inside could scroll wider.
    ImGui::Begin(windowTitle);

    if (m_mode == InspectionMode::None) {
        ImGui::TextDisabled("No selection");
    }
    else if (m_mode == InspectionMode::Entity) {
        _renderEntityCore();
    }
    else if (m_mode == InspectionMode::Prefab) {
        _renderPrefabInspector();
    }

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
    if (!m_world) {
        m_mode = InspectionMode::None;
        return;
    }
    ECS::Entity e = m_world->Resolve(id);
    m_mode = m_world->IsAlive(e) ? InspectionMode::Entity : InspectionMode::None;
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
        m_lastSavedPrefabHash = std::hash<std::string>{}(m_prefabData.dump());
        m_mode = InspectionMode::Prefab;
    }
    catch (const std::exception& e) {
        m_mode = InspectionMode::None;
        LOG_ERROR("Failed to parse prefab JSON: " << e.what());
    }
}

void InspectorWindow::ClearSelection() {
    m_mode = InspectionMode::None;
    m_inspectedEntityId = 0;
    m_inspectedPrefabPath.clear();
    m_prefabData = {};
    m_componentsToDelete.clear();
}

void InspectorWindow::_renderEntityCore() {
    if (!m_world) {
        ImGui::TextDisabled("No entity selected");
        return;
    }

    ECS::Entity entity = m_world->Resolve(m_inspectedEntityId);
    if (!m_world->IsAlive(entity)) {
        ImGui::TextDisabled("Entity invalid");
        return;
    }

    const char* entityName = "Unnamed";
    if (m_world->Has<ECS::Components::Name>(entity)) {
        entityName = m_world->Get<ECS::Components::Name>(entity).Value;
    }

    ImGui::Text("Entity ");
    ImGui::SameLine();
    ImGui::TextDisabled("%s (ID: %u)", entityName, (unsigned)m_inspectedEntityId);

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

    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("EntityComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

    if (entityJson.contains("Components")) {
        ImGui::Dummy(ImVec2(0, 4));

        // Always render Transform first (not deletable)
        for (auto& componentEntry : entityJson["Components"]) {
            if (componentEntry["TypeName"] == "ECS::Components::LocalTransform") {
                _renderComponentSection("Transform", "LocalTransform", componentEntry["Data"],
                    [this](nlohmann::json& d) { m_componentUI.RenderLocalTransform(d); }, false);
                break;
            }
        }

        const std::vector<std::string> orderedTypes = {
            "ECS::Components::Camera3D",
            "ECS::Components::SpriteRenderer2D",
            "ECS::Components::Rigidbody2D",
            "ECS::Components::LinearVelocity2D",
            "ECS::Components::AngularVelocity2D",
            "ECS::Components::CircleCollider2D",
            "ECS::Components::BoxCollider2D",
            "ECS::Components::ShapeCircle2D",
            "ECS::Components::ShapeBox2D",
            "ECS::Components::ShapeLine2D"
        };

        for (const auto& type : orderedTypes) {
            for (auto& componentEntry : entityJson["Components"]) {
                if (componentEntry["TypeName"] != type) continue;

                // Ensure Data field exists and has defaults
                if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) {
                    std::string shortType = type.substr(std::string("ECS::Components::").length());
                    componentEntry["Data"] = _getDefaultComponentData(shortType);
                }
                auto& data = componentEntry["Data"];

                if (type == "ECS::Components::SpriteRenderer2D") {
                    _renderComponentSection("Sprite Renderer", "SpriteRenderer2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderSpriteRenderer2D(d); });
                }
                else if (type == "ECS::Components::Rigidbody2D") {
                    // Locate velocity component data for nested Advanced dropdown
                    _renderComponentSection("Rigidbody 2D", "Rigidbody2D", data,
                        [this](nlohmann::json& d) {
                            // Render only the core Rigidbody properties; no Advanced section
                            m_componentUI.RenderRigidbody2D(d);
                        });
                }
                else if (type == "ECS::Components::LinearVelocity2D") {
                    _renderComponentSection("Linear Velocity 2D", "LinearVelocity2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderLinearVelocity2D(d); });
                }
                else if (type == "ECS::Components::AngularVelocity2D") {
                    _renderComponentSection("Angular Velocity 2D", "AngularVelocity2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderAngularVelocity2D(d); });
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

                break;
            }
        }

        // Apply deletions queued by the delete buttons
        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromEntity(type);
        }
        m_componentsToDelete.clear();

        // Write changes back into actual components
        for (auto& componentEntry : entityJson["Components"]) {
            std::string typeName = componentEntry["TypeName"];

            // Use a file-scope templated helper to avoid MSVC restriction on local class templates

            if (ApplyComponentIfMatch<ECS::Components::LocalTransform>(m_world, entity, typeName, "ECS::Components::LocalTransform", componentEntry)) continue;
            if (ApplyComponentIfMatch<ECS::Components::SpriteRenderer2D>(m_world, entity, typeName, "ECS::Components::SpriteRenderer2D", componentEntry)) continue;
            if (ApplyComponentIfMatch<ECS::Components::Rigidbody2D>(m_world, entity, typeName, "ECS::Components::Rigidbody2D", componentEntry)) continue;
            // Ensure velocity component edits are persisted to the world
            if (ApplyComponentIfMatch<ECS::Components::LinearVelocity2D>(m_world, entity, typeName, "ECS::Components::LinearVelocity2D", componentEntry)) continue;
            if (ApplyComponentIfMatch<ECS::Components::AngularVelocity2D>(m_world, entity, typeName, "ECS::Components::AngularVelocity2D", componentEntry)) continue;
            if (ApplyComponentIfMatch<ECS::Components::CircleCollider2D>(m_world, entity, typeName, "ECS::Components::CircleCollider2D", componentEntry)) continue;
            if (ApplyComponentIfMatch<ECS::Components::BoxCollider2D>(m_world, entity, typeName, "ECS::Components::BoxCollider2D", componentEntry)) continue;
            if (ApplyComponentIfMatch<ECS::Components::ShapeCircle2D>(m_world, entity, typeName, "ECS::Components::ShapeCircle2D", componentEntry)) continue;
            if (ApplyComponentIfMatch<ECS::Components::ShapeBox2D>(m_world, entity, typeName, "ECS::Components::ShapeBox2D", componentEntry)) continue;
            if (ApplyComponentIfMatch<ECS::Components::ShapeLine2D>(m_world, entity, typeName, "ECS::Components::ShapeLine2D", componentEntry)) continue;
            if (ApplyComponentIfMatch<ECS::Components::Camera3D>(m_world, entity, typeName, "ECS::Components::Camera3D", componentEntry)) continue;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::Separator();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
    }

    // Add "Save As Prefab" button next to "Add Component"
    ImGui::SameLine();
    if (ImGui::Button("Save As Prefab")) {
        _saveEntityAsPrefab();
    }

        if (ImGui::BeginPopup("AddComponentMenu")) {
            ImGui::PushFont(m_boldFont);
            ImGui::Text("Components");
            ImGui::PopFont();
            ImGui::Separator();

            _renderComponentMenuItem("Transform", "LocalTransform");
            _renderComponentMenuItem("SpriteRenderer", "SpriteRenderer2D");
            _renderComponentMenuItem("Rigidbody2D", "Rigidbody2D");
            _renderComponentMenuItem("LinearVelocity2D", "LinearVelocity2D");
            _renderComponentMenuItem("AngularVelocity2D", "AngularVelocity2D");
            _renderComponentMenuItem("CircleCollider2D", "CircleCollider2D");
            _renderComponentMenuItem("BoxCollider2D", "BoxCollider2D");
            _renderComponentMenuItem("ShapeCircle2D", "ShapeCircle2D");
            _renderComponentMenuItem("ShapeBox2D", "ShapeBox2D");
            _renderComponentMenuItem("ShapeLine2D", "ShapeLine2D");
            _renderComponentMenuItem("Camera 3D", "Camera3D");
            ImGui::EndPopup();
        }
}

void InspectorWindow::_renderPrefabInspector() {
    if (m_inspectedPrefabPath.empty()) {
        ImGui::TextDisabled("No prefab selected");
        return;
    }

    ImGui::Text("Editing Prefab Template");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", std::filesystem::path(m_inspectedPrefabPath).filename().string().c_str());

    ImGui::Separator();
    ImGui::TextWrapped("Changes to this prefab will update ALL instances in the scene.");
    ImGui::Separator();

    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("PrefabComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (m_prefabData.contains("Components") && m_prefabData["Components"].is_array()) {
        ImGui::Dummy(ImVec2(0, 4));

        for (auto& componentEntry : m_prefabData["Components"]) {
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string())
                continue;
            if (componentEntry["TypeName"] == "ECS::Components::LocalTransform") {
                if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object())
                    componentEntry["Data"] = nlohmann::json::object();
                auto& dataObj = componentEntry["Data"];
                _renderComponentSection("Transform", "LocalTransform", dataObj,
                    [this](nlohmann::json& d) { m_componentUI.RenderLocalTransform(d); }, false);
                break;
            }
        }

        const std::vector<std::string> orderedTypes = {
            "ECS::Components::Camera3D",
            "ECS::Components::SpriteRenderer2D",
            "ECS::Components::Rigidbody2D",
            "ECS::Components::LinearVelocity2D",
            "ECS::Components::AngularVelocity2D",
            "ECS::Components::CircleCollider2D",
            "ECS::Components::BoxCollider2D",
            "ECS::Components::ShapeCircle2D",
            "ECS::Components::ShapeBox2D",
            "ECS::Components::ShapeLine2D"
        };

        for (const auto& type : orderedTypes) {
            for (auto& componentEntry : m_prefabData["Components"]) {
                if (componentEntry["TypeName"] != type) continue;

                // Ensure Data field exists and has defaults
                if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) {
                    std::string shortType = type.substr(std::string("ECS::Components::").length());
                    componentEntry["Data"] = _getDefaultComponentData(shortType);
                }
                auto& data = componentEntry["Data"];

                if (type == "ECS::Components::SpriteRenderer2D") {
                    _renderComponentSection("Sprite Renderer", "SpriteRenderer2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderSpriteRenderer2D(d); });
                }
                else if (type == "ECS::Components::Rigidbody2D") {
                    _renderComponentSection("Rigidbody 2D", "Rigidbody2D", data,
                        [this](nlohmann::json& d) {
                            // Render only the core Rigidbody properties; no Advanced section
                            m_componentUI.RenderRigidbody2D(d);
                        });
                }
                else if (type == "ECS::Components::LinearVelocity2D") {
                    _renderComponentSection("Linear Velocity 2D", "LinearVelocity2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderLinearVelocity2D(d); });
                }
                else if (type == "ECS::Components::AngularVelocity2D") {
                    _renderComponentSection("Angular Velocity 2D", "AngularVelocity2D", data,
                        [this](nlohmann::json& d) { m_componentUI.RenderAngularVelocity2D(d); });
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

                break;
            }
        }

        for (const auto& type : m_componentsToDelete) {
            _removeComponentFromPrefab(type);
        }
        m_componentsToDelete.clear();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

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
        // Allow adding velocity components directly to prefab templates
        _renderComponentMenuItem("Linear Velocity 2D", "LinearVelocity2D");
        _renderComponentMenuItem("Angular Velocity 2D", "AngularVelocity2D");
        _renderComponentMenuItem("Circle Collider 2D", "CircleCollider2D");
        _renderComponentMenuItem("Box Collider 2D", "BoxCollider2D");
        _renderComponentMenuItem("Shape Circle", "ShapeCircle2D");
        _renderComponentMenuItem("Shape Box", "ShapeBox2D");
        _renderComponentMenuItem("Shape Line", "ShapeLine2D");
        _renderComponentMenuItem("Camera 3D", "Camera3D");
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Apply to All Instances")) {
        _applyPrefabChangesToInstances();
    }
}

template <typename T>
void InspectorWindow::_renderComponentSection(
    const std::string& headerName,
    const std::string& componentType,
    nlohmann::json& data,
    T renderContent,
    bool canDelete)
{
    // Draw the collapsible header first
    bool nodeOpen = ImGui::CollapsingHeader(
        headerName.c_str(),
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth
    );

    // Save the cursor after the header to restore before rendering fields
    ImVec2 afterHeader = ImGui::GetCursorPos();

    // --- Trash icon section ---
    {
        // Push a small vertical gap so it sits *below* the header
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        // Compute X pos at the far right of the scrollable region
        float rightEdge = EditorUI::GetCurrentLabelOffset() + EditorUI::GetContentWidth();
        ImVec2 buttonSize = ImGui::CalcTextSize("\xee\xa1\xb2"); // material symbol bin
        float buttonX = rightEdge - buttonSize.x - 8.0f; // margin from edge

        ImGui::SetCursorPosX(buttonX);
        if (!canDelete) ImGui::BeginDisabled();

        ImGui::PushFont(m_symbolsFont);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, canDelete
            ? ImVec4(0.8f, 0.25f, 0.25f, 1.0f)
            : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        bool clicked = ImGui::SmallButton(("\xee\xa1\xb2##Delete" + componentType).c_str());

        ImGui::PopStyleColor(4);
        ImGui::PopFont();

        if (!canDelete) ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(canDelete ? "Remove Component" : "Transform cannot be removed");
        }

        if (clicked && canDelete)
            m_componentsToDelete.push_back(componentType);
    }

    // Restore cursor to after-header position before drawing contents
    ImGui::SetCursorPos(afterHeader);

    if (!nodeOpen)
        return;

    ImGui::Dummy(ImVec2(0.0f, 8.0f)); // spacing before body
    renderContent(data);
}

// ============================================================================
//  Component menu item helper: adds component to entity/prefab when clicked
// ============================================================================
void InspectorWindow::_renderComponentMenuItem(const char* displayName, const char* componentType) {
    bool hasComponent = false;

    if (m_mode == InspectionMode::Entity) {
        hasComponent = _entityHasComponent(m_inspectedEntityId, componentType);
    }
    else if (m_mode == InspectionMode::Prefab) {
        hasComponent = _prefabHasComponent(componentType);
    }

    // Disable if already present
    if (hasComponent) ImGui::BeginDisabled();
    if (ImGui::MenuItem(displayName)) {
        if (!hasComponent) {
            if (m_mode == InspectionMode::Entity)
                _addComponentToEntity(componentType);
            else if (m_mode == InspectionMode::Prefab)
                _addComponentToPrefab(componentType);
            ImGui::CloseCurrentPopup();
        }
    }
    if (hasComponent) ImGui::EndDisabled();
}

// ============================================================================
//  Removes a component from a selected entity
// ============================================================================
void InspectorWindow::_removeComponentFromEntity(const std::string& componentType) {
    if (!m_world) return;

    ECS::Entity e = m_world->Resolve(m_inspectedEntityId);

    if (componentType == "LocalTransform") return; // can't remove Transform

    if (componentType == "SpriteRenderer2D" && m_world->Has<ECS::Components::SpriteRenderer2D>(e))
        m_world->Remove<ECS::Components::SpriteRenderer2D>(e);
    else if (componentType == "Rigidbody2D" && m_world->Has<ECS::Components::Rigidbody2D>(e))
        m_world->Remove<ECS::Components::Rigidbody2D>(e);
    else if (componentType == "LinearVelocity2D" && m_world->Has<ECS::Components::LinearVelocity2D>(e))
        m_world->Remove<ECS::Components::LinearVelocity2D>(e);
    else if (componentType == "AngularVelocity2D" && m_world->Has<ECS::Components::AngularVelocity2D>(e))
        m_world->Remove<ECS::Components::AngularVelocity2D>(e);
    else if (componentType == "CircleCollider2D" && m_world->Has<ECS::Components::CircleCollider2D>(e))
        m_world->Remove<ECS::Components::CircleCollider2D>(e);
    else if (componentType == "BoxCollider2D" && m_world->Has<ECS::Components::BoxCollider2D>(e))
        m_world->Remove<ECS::Components::BoxCollider2D>(e);
    else if (componentType == "ShapeCircle2D" && m_world->Has<ECS::Components::ShapeCircle2D>(e))
        m_world->Remove<ECS::Components::ShapeCircle2D>(e);
    else if (componentType == "ShapeBox2D" && m_world->Has<ECS::Components::ShapeBox2D>(e))
        m_world->Remove<ECS::Components::ShapeBox2D>(e);
    else if (componentType == "ShapeLine2D" && m_world->Has<ECS::Components::ShapeLine2D>(e))
        m_world->Remove<ECS::Components::ShapeLine2D>(e);
    else if (componentType == "Camera3D" && m_world->Has<ECS::Components::Camera3D>(e))
        m_world->Remove<ECS::Components::Camera3D>(e);

    m_statusMessage = "Removed component: " + componentType;
    m_statusTimer = 2.0f;
}

// ============================================================================
//  Removes a component entry from prefab JSON
// ============================================================================
void InspectorWindow::_removeComponentFromPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return;

    auto& components = m_prefabData["Components"];
    components.erase(std::remove_if(components.begin(), components.end(),
        [&](const nlohmann::json& comp) {
            return comp["TypeName"] == "ECS::Components::" + componentType;
        }),
        components.end());

    // Write back to prefab file
    std::ofstream file(m_inspectedPrefabPath);
    if (file.is_open()) {
        file << m_prefabData.dump(2);
        file.close();
    }

    m_statusMessage = "Removed prefab component: " + componentType;
    m_statusTimer = 2.0f;
}

// ============================================================================
//  Applies prefab changes to all entities linked to that prefab
// ============================================================================
void InspectorWindow::_applyPrefabChangesToInstances() {
    if (!m_world || m_inspectedPrefabPath.empty()) return;

    size_t prefabHash = std::hash<std::string>{}(m_prefabData.dump());
    if (prefabHash == m_lastSavedPrefabHash) {
        m_statusMessage = "No changes to apply.";
        m_statusTimer = 2.0f;
        return;
    }

    // save prefab JSON
    std::ofstream file(m_inspectedPrefabPath);
    if (file.is_open()) {
        file << m_prefabData.dump(2);
        file.close();
    }
    m_lastSavedPrefabHash = prefabHash;

    // update all linked entities
    m_world->Each([&](ECS::Entity e) {
        if (m_world->Has<ECS::Components::PrefabLink>(e)) {
            const auto& link = m_world->Get<ECS::Components::PrefabLink>(e);
            if (link.prefabPath == m_inspectedPrefabPath)
                _applyPrefabDataToEntity(e);
        }
        });

    m_statusMessage = "Prefab changes applied to all instances.";
    m_statusTimer = 3.0f;
}

// ============================================================================
//  Adds a component to an entity
// ============================================================================
void InspectorWindow::_addComponentToEntity(const std::string& componentType) {
    if (!m_world || m_inspectedEntityId == 0) return;
    ECS::Entity e = m_world->Resolve(m_inspectedEntityId);

    // Enforce mutual exclusivity among shape renderers: only one shape component allowed
    if (componentType == "ShapeCircle2D" || componentType == "ShapeBox2D" || componentType == "ShapeLine2D") {
        if (m_world->Has<ECS::Components::ShapeCircle2D>(e) && componentType != "ShapeCircle2D") {
            m_world->Remove<ECS::Components::ShapeCircle2D>(e);
        }
        if (m_world->Has<ECS::Components::ShapeBox2D>(e) && componentType != "ShapeBox2D") {
            m_world->Remove<ECS::Components::ShapeBox2D>(e);
        }
        if (m_world->Has<ECS::Components::ShapeLine2D>(e) && componentType != "ShapeLine2D") {
            m_world->Remove<ECS::Components::ShapeLine2D>(e);
        }
    }

    // Get default data for the component
    nlohmann::json defaultData = _getDefaultComponentData(componentType);

    // All components are registered via EntitySerializer
    const auto& reg = Serialization::EntitySerializer::Registry();
    for (const auto& [tid, info] : reg) {
        if (info.Name == "ECS::Components::" + componentType) {
            info.Deserialize(*m_world, e, defaultData);
            m_statusMessage = "Added component: " + componentType;
            m_statusTimer = 2.0f;
            return;
        }
    }

    m_statusMessage = "Unknown component type: " + componentType;
    m_statusTimer = 2.0f;
}

// ============================================================================
//  Adds a component definition to prefab JSON
// ============================================================================
void InspectorWindow::_addComponentToPrefab(const std::string& componentType) {
    if (!m_prefabData.contains("Components"))
        m_prefabData["Components"] = nlohmann::json::array();

    // Enforce mutual exclusivity among shape renderers in prefab JSON
    if (componentType == "ShapeCircle2D" || componentType == "ShapeBox2D" || componentType == "ShapeLine2D") {
        auto& components = m_prefabData["Components"];
        components.erase(std::remove_if(components.begin(), components.end(),
            [&](const nlohmann::json& comp) {
                std::string tn = comp.value("TypeName", "");
                if (tn == "ECS::Components::ShapeCircle2D" && componentType != "ShapeCircle2D") return true;
                if (tn == "ECS::Components::ShapeBox2D" && componentType != "ShapeBox2D") return true;
                if (tn == "ECS::Components::ShapeLine2D" && componentType != "ShapeLine2D") return true;
                return false;
            }), components.end());
    }

    nlohmann::json newComp;
    newComp["TypeName"] = "ECS::Components::" + componentType;
    newComp["Data"] = _getDefaultComponentData(componentType);

    m_prefabData["Components"].push_back(newComp);

    std::ofstream file(m_inspectedPrefabPath);
    if (file.is_open()) {
        file << m_prefabData.dump(2);
        file.close();
    }

    m_statusMessage = "Added prefab component: " + componentType;
    m_statusTimer = 2.0f;
}

// ============================================================================
//  Applies prefab JSON data to a live entity (using EntitySerializer)
// ============================================================================
void InspectorWindow::_applyPrefabDataToEntity(ECS::Entity entity) {
    if (!m_world) return;

    // open prefab json
    std::ifstream file(m_inspectedPrefabPath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open prefab for reapply: " << m_inspectedPrefabPath);
        return;
    }

    nlohmann::json prefabJson;
    file >> prefabJson;
    file.close();

    // Use built-in deserializer to overwrite existing entity's components
    const auto& components = prefabJson.contains("Components") ? prefabJson["Components"] : nlohmann::json::array();
    for (auto& comp : components) {
        std::string typeName = comp.value("TypeName", "");
        auto& reg = Serialization::EntitySerializer::Registry();
        for (auto& [tid, info] : reg) {
            if (info.Name == typeName) {
                info.Deserialize(*m_world, entity, comp["Data"]);
                break;
            }
        }
    }

    m_statusMessage = "Applied prefab changes to entity.";
    m_statusTimer = 2.0f;
}

// ============================================================================
//  Save the currently selected entity as a prefab file
// ============================================================================
void InspectorWindow::_saveEntityAsPrefab() {
    if (!m_world || m_inspectedEntityId == 0) return;
    ECS::Entity entity{ m_inspectedEntityId, 0 };
    if (!m_world->IsAlive(entity)) return;

    std::string baseName = "Entity";
    if (m_world->Has<ECS::Components::Name>(entity))
        baseName = m_world->Get<ECS::Components::Name>(entity).Value;

    // sanitize
    baseName.erase(std::remove_if(baseName.begin(), baseName.end(),
        [](char c) { return !std::isalnum(static_cast<unsigned char>(c)) && c != '_'; }), baseName.end());

    std::filesystem::create_directories("assets/prefabs");

    // --- ensure unique ---
    std::string savePath = "assets/prefabs/" + baseName + ".prefab";
    if (std::filesystem::exists(savePath)) {
        // add entity ID suffix for uniqueness
        savePath = "assets/prefabs/" + baseName + "_" + std::to_string(m_inspectedEntityId) + ".prefab";
    }

    nlohmann::json prefabJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

    try {
        std::ofstream file(savePath);
        file << prefabJson.dump(2);
        file.close();
        m_statusMessage = "Saved prefab in assets/prefab";
        LOG_INFO("Saved prefab: " << savePath);
    }
    catch (const std::exception& e) {
        m_statusMessage = std::string("Failed to save prefab: ") + e.what();
        LOG_ERROR("Exception saving prefab: " << e.what());
    }

    m_statusTimer = 2.0f;
}

// ============================================================================
//  Get default component data for a given component type
// ============================================================================
nlohmann::json InspectorWindow::_getDefaultComponentData(const std::string& componentType) {
    nlohmann::json defaultData = nlohmann::json::object();

    if (componentType == "LocalTransform") {
        defaultData["Position"] = { {"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f} };
        defaultData["Rotation"] = { {"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 1.0f} };
        defaultData["Scale"] = { {"X", 1.0f}, {"Y", 1.0f}, {"Z", 1.0f} };
    }
    else if (componentType == "LinearVelocity2D") {
        // Ensure expected schema exists for UI: Value with X/Y
        defaultData["Value"] = { {"X", 0.0f}, {"Y", 0.0f} };
    }
    else if (componentType == "AngularVelocity2D") {
        // Ensure expected schema exists for UI: Value as float
        defaultData["Value"] = 0.0f;
    }
    else if (componentType == "SpriteRenderer2D") {
        defaultData["TextureId"] = 0;
        defaultData["TexturePath"] = "";
        defaultData["Width"] = 0;
        defaultData["Height"] = 0;
        defaultData["Color"] = { {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
        defaultData["Tiling"] = { {"X", 1.0f}, {"Y", 1.0f} };
        defaultData["Offset"] = { {"X", 0.0f}, {"Y", 0.0f} };
    }
    else if (componentType == "Rigidbody2D") {
        defaultData["Mass"] = 1.0f;
        defaultData["InverseMass"] = 1.0f;
        defaultData["LinearDamping"] = 0.0f;
        defaultData["AngularDamping"] = 0.0f;
        defaultData["GravityScale"] = 1.0f;
        defaultData["Flags"] = 0;
    }
    else if (componentType == "CircleCollider2D") {
        defaultData["Offset"] = { {"X", 0.0f}, {"Y", 0.0f} };
        defaultData["Radius"] = 50.0f;
        defaultData["LayerMask"] = 0;
        defaultData["Flags"] = 0;
    }
    else if (componentType == "BoxCollider2D") {
        defaultData["Offset"] = { {"X", 0.0f}, {"Y", 0.0f} };
        defaultData["HalfExtents"] = { {"X", 50.0f}, {"Y", 50.0f} };
        defaultData["Rotation"] = 0.0f;
        defaultData["LayerMask"] = 0;
        defaultData["Flags"] = 0;
    }
    else if (componentType == "ShapeCircle2D") {
        defaultData["Radius"] = 50.0f;
        defaultData["Offset"] = { {"X", 0.0f}, {"Y", 0.0f} };
        defaultData["Color"] = { {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
        defaultData["Thickness"] = 1.0f;
        defaultData["Filled"] = false;
    }
    else if (componentType == "ShapeBox2D") {
        defaultData["HalfExtents"] = { {"X", 50.0f}, {"Y", 50.0f} };
        defaultData["Offset"] = { {"X", 0.0f}, {"Y", 0.0f} };
        defaultData["Color"] = { {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
        defaultData["Thickness"] = 1.0f;
        defaultData["Filled"] = false;
    }
    else if (componentType == "ShapeLine2D") {
        defaultData["A"] = { {"X", 0.0f}, {"Y", 0.0f} };
        defaultData["B"] = { {"X", 100.0f}, {"Y", 0.0f} };
        defaultData["Thickness"] = 1.0f;
        defaultData["Color"] = { {"R", 1.0f}, {"G", 1.0f}, {"B", 1.0f}, {"A", 1.0f} };
    }
    else if (componentType == "Camera3D") {
        defaultData["Active"] = true;
        defaultData["UsePerspective"] = false;
        defaultData["FOV"] = 45.0f;
        defaultData["OrthoSize"] = 16.0f;
        defaultData["NearPlane"] = 0.1f;
        defaultData["FarPlane"] = 100.0f;
        defaultData["AspectRatio"] = 1.777f;
    }

    return defaultData;
}

// ============================================================================
//  Check if an entity has a specific component
// ============================================================================
bool InspectorWindow::_entityHasComponent(EntityId id, const std::string& componentType) {
    if (!m_world) return false;

    ECS::Entity entity = m_world->Resolve(id);
    if (!m_world->IsAlive(entity)) return false;

    if (componentType == "LocalTransform") return m_world->Has<ECS::Components::LocalTransform>(entity);
    if (componentType == "SpriteRenderer2D") return m_world->Has<ECS::Components::SpriteRenderer2D>(entity);
    if (componentType == "Rigidbody2D") return m_world->Has<ECS::Components::Rigidbody2D>(entity);
    if (componentType == "LinearVelocity2D") return m_world->Has<ECS::Components::LinearVelocity2D>(entity);
    if (componentType == "AngularVelocity2D") return m_world->Has<ECS::Components::AngularVelocity2D>(entity);
    if (componentType == "CircleCollider2D") return m_world->Has<ECS::Components::CircleCollider2D>(entity);
    if (componentType == "BoxCollider2D") return m_world->Has<ECS::Components::BoxCollider2D>(entity);
    if (componentType == "ShapeCircle2D") return m_world->Has<ECS::Components::ShapeCircle2D>(entity);
    if (componentType == "ShapeBox2D") return m_world->Has<ECS::Components::ShapeBox2D>(entity);
    if (componentType == "ShapeLine2D") return m_world->Has<ECS::Components::ShapeLine2D>(entity);
    if (componentType == "Camera3D") return m_world->Has<ECS::Components::Camera3D>(entity);

    return false;
}

// ============================================================================
//  Check if a prefab has a specific component
// ============================================================================
bool InspectorWindow::_prefabHasComponent(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return false;

    std::string fullTypeName = "ECS::Components::" + componentType;
    for (const auto& comp : m_prefabData["Components"]) {
        if (comp["TypeName"] == fullTypeName) {
            return true;
        }
    }
    return false;
}
