/* Start Header *****************************************************************/
/*!
\file   HierarchyWindow.cpp
\author Foo Rui Qin    (50%)
        Samantha Leong (50%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Implements a Unity-like hierarchy window UI for managing entities in a tree structure.
*/
/* End Header *******************************************************************/

#include "../editor/HierarchyWindow.h"
#include "core/Logger.h"
#include "helpers/MathHelper.h"
#include "services/Input.h"
#include "serialization/EntitySerializer.h"
#include "core/Application.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>

void HierarchyWindow::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world, EditorCore* editorCore) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
    m_editorCore = editorCore;
}

void HierarchyWindow::SetWorld(ECS::World* world) {
    m_world = world;
    m_selectedEntityId = 0;
    m_expandedNodes.clear();
}

void HierarchyWindow::Render() {
    ImGui::Begin("Hierarchy");

    bool noScene = true;
    if (Engine::CORE) {
        auto& sm = Engine::CORE->GetSceneManager();
        noScene = (sm.GetActive() == nullptr);
    }
    if (noScene) {
        ImGui::TextDisabled("No scene attached");
        ImGui::End();
        return;
    }

    ImGui::Text("Create New Object");
    static char nameBuffer[128] = "NewObject";
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
    ImGui::InputText("##NewObjectName", nameBuffer, sizeof(nameBuffer));

    ImGui::SameLine();
    if (ImGui::Button("Add") && strlen(nameBuffer) > 0) {
        if (m_editorCore) {
            m_editorCore->AddEntity(nameBuffer, ECS::Entity::NPOS32);  // Always add at root
        }
    }

    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2));

    std::vector<ECS::Entity> allEntities;
    m_world->Each([&](ECS::Entity e) {
        allEntities.push_back(e);
        });

    ImGui::Text("Objects (%zu)", allEntities.size());

    // Child window shows the tree; third param 'true' draws a border around the child
    ImGui::BeginChild("HierarchyTree", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);
    auto rootEntities = _getRootEntities();
    for (auto entityId : rootEntities) {
        _renderEntityNode(entityId, 0);
    }

    // Root-level drop target: accept reparenting or prefab instantiation into root
    if (ImGui::BeginDragDropTarget()) {
        // Drop an entity here to reparent it to root (NPOS32 sentinel)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;
            if (m_editorCore) {
                m_editorCore->ReparentEntity(draggedId, ECS::Entity::NPOS32);
            }
        }
        // Drop a prefab here to instantiate it at root
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                _instantiatePrefabAsChild(droppedPath, ECS::Entity::NPOS32);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();

    // Fallback drop target for the whole window (outside the child) to reparent to root
    if (ImGui::BeginDragDropTarget()) {
        // Allow dropping an entity anywhere in the window to reparent to root
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;
            if (m_editorCore) {
                m_editorCore->ReparentEntity(draggedId, ECS::Entity::NPOS32);
            }
        }
        // Also allow dropping a prefab anywhere in the window to instantiate at root
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                _instantiatePrefabAsChild(droppedPath, ECS::Entity::NPOS32);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Click on empty space clears selection: only when window is hovered and no item is under cursor
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered()) {
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
    }

    if (ImGui::Button("Clear All")) {
        if (m_editorCore) {
            m_editorCore->ClearAllEntities();
        }
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
    }

    ImGui::End();
}

void HierarchyWindow::_renderEntityNode(EntityId entityId, int depth) {
    ECS::Entity entity{ entityId, 0 };

    if (!m_world->IsAlive(entity)) return;

    auto children = _getChildren(entityId);
    bool hasChildren = !children.empty();

    std::stringstream oss;
    if (m_world->Has<ECS::Components::Name>(entity)) {
        const auto& nameComp = m_world->Get<ECS::Components::Name>(entity);
        oss << nameComp.Value << " (" << entityId << ")";
    }
    else {
        oss << "Entity (" << entityId << ")";
    }

    if (m_world->Has<ECS::Components::PrefabLink>(entity)) {
        const auto& link = m_world->Get<ECS::Components::PrefabLink>(entity);
        std::string prefabName = std::filesystem::path(link.getPath()).stem().string();
        oss << " [" << prefabName << "]";
    }
    std::string label = oss.str();

    // Tree node flags:
    // - OpenOnArrow: clicking the arrow toggles, clicking label selects
    // - SpanAvailWidth: make the selectable area span the full row
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    if (m_selectedEntityId == entityId) flags |= ImGuiTreeNodeFlags_Selected;

    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entityId, flags, "%s", label.c_str());

    // Clicking the node label selects the entity
    if (ImGui::IsItemClicked()) {
        m_selectedEntityId = entityId;
        if (m_selectionCallback) m_selectionCallback(entityId);
    }

    // Begin drag source: we drag an entity ID to reparent under another node
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY_ID", &entityId, sizeof(EntityId));
        if (m_world->Has<ECS::Components::Name>(entity)) {
            const auto& nameComp = m_world->Get<ECS::Components::Name>(entity);
            ImGui::Text("Reparent %s", nameComp.Value);
        }
        else {
            ImGui::Text("Reparent Entity %u", entityId);
        }
        ImGui::EndDragDropSource();
    }

    // Accept drops on this node: reparent entities or instantiate prefabs as children
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;
            if (m_editorCore) {
                m_editorCore->ReparentEntity(draggedId, entityId);
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                _instantiatePrefabAsChild(droppedPath, entityId);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Right-click context menu on node for quick actions
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::Selectable("Add Child")) {
            if (m_editorCore) {
                m_editorCore->AddEntity("Child", entityId);
            }
        }
        if (ImGui::Selectable("Clone")) {
            if (m_editorCore) {
                m_editorCore->CloneEntity(entityId);
            }
        }
        if (ImGui::Selectable("Delete")) {
            if (m_editorCore) {
                m_editorCore->RemoveEntity(entityId, true);
            }
            ImGui::EndPopup();
            if (nodeOpen) ImGui::TreePop();
            return;
        }
        ImGui::EndPopup();
    }

    if (nodeOpen) {
        for (auto childId : children) {
            _renderEntityNode(childId, depth + 1);
        }
        ImGui::TreePop();
    }
}

std::vector<EntityId> HierarchyWindow::_getRootEntities() {
    std::vector<EntityId> roots;

    m_world->Each([&](ECS::Entity e) {
        if (!m_world->Has<ECS::Parent>(e)) {
            roots.push_back(e.Index);
        }
        else {
            const auto& parent = m_world->Get<ECS::Parent>(e);
            if (parent.ParentEntity.IsNull()) {
                roots.push_back(e.Index);
            }
        }
        });

    return roots;
}

std::vector<EntityId> HierarchyWindow::_getChildren(EntityId parentId) {
    std::vector<EntityId> children;
    ECS::Entity parentEntity = m_world->Resolve(parentId);
    if (parentEntity.IsNull()) return children;

    m_world->ForChildren(parentEntity, [&](ECS::Entity child) {
        children.push_back(child.Index);
        });

    return children;
}

void HierarchyWindow::_instantiatePrefabAsChild(const std::string& prefabPath, EntityId parentId) {
    if (!m_world || prefabPath.empty()) return;

    try {
        std::ifstream file(prefabPath);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open prefab file: " << prefabPath);
            return;
        }

        nlohmann::json prefabJson;
        file >> prefabJson;
        file.close();

        if (!prefabJson.contains("Components") || !prefabJson["Components"].is_array()) {
            LOG_ERROR("Invalid prefab format: missing Components array");
            return;
        }

        // Create entity first
        ECS::Entity instance = m_world->Create();

        // Add Name component separately if it exists in prefab
        bool hasName = false;
        for (const auto& comp : prefabJson["Components"]) {
            // Defensive checks on prefab JSON entries
            if (!comp.contains("TypeName") || !comp["TypeName"].is_string())
                continue;

            if (comp["TypeName"] == "ECS::Components::Name") {
                hasName = true;
                auto& nameComp = m_world->Add<ECS::Components::Name>(instance);
                if (comp.contains("Data") && comp["Data"].is_object() &&
                    comp["Data"].contains("Value") && comp["Data"]["Value"].is_string()) {
                    std::string nameStr = comp["Data"]["Value"].get<std::string>();
                    std::strncpy(nameComp.Value, nameStr.c_str(), sizeof(nameComp.Value) - 1);
                    nameComp.Value[sizeof(nameComp.Value) - 1] = '\0';
                }
                break;
            }
        }

        // Deserialize remaining components
        for (const auto& comp : prefabJson["Components"]) {
            // Defensive checks
            if (!comp.contains("TypeName") || !comp["TypeName"].is_string())
                continue;
            if (comp["TypeName"] == "ECS::Components::Name") continue; // Already handled

            std::string typeName = comp["TypeName"].get<std::string>();
            auto compData = (comp.contains("Data") && comp["Data"].is_object())
                ? comp["Data"]
                : nlohmann::json::object();

            // Prefab JSON -> components (on instance, via ADL `from_json`)
            // Keys/types must match EntitySerializer.h macros
            // Type matched by `TypeName`; call `from_json(Data, component)`
            // Adds component if missing; otherwise updates existing fields
            auto addComp = [&]<typename T>(const std::string & name) -> bool {
                if (typeName != name) return false;
                if (!m_world->Has<T>(instance)) {
                    auto& c = m_world->Add<T>(instance);
                    from_json(compData, c);
                }
                return true;
            };

            if (addComp.operator() < ECS::Components::LocalTransform > ("ECS::Components::LocalTransform")) continue;
            if (addComp.operator() < ECS::Components::SpriteRenderer2D > ("ECS::Components::SpriteRenderer2D")) continue;
            if (addComp.operator() < ECS::Components::Rigidbody2D > ("ECS::Components::Rigidbody2D")) continue;
            if (addComp.operator() < ECS::Components::CircleCollider2D > ("ECS::Components::CircleCollider2D")) continue;
            if (addComp.operator() < ECS::Components::BoxCollider2D > ("ECS::Components::BoxCollider2D")) continue;
            if (addComp.operator() < ECS::Components::ShapeCircle2D > ("ECS::Components::ShapeCircle2D")) continue;
            if (addComp.operator() < ECS::Components::ShapeBox2D > ("ECS::Components::ShapeBox2D")) continue;
            if (addComp.operator() < ECS::Components::ShapeLine2D > ("ECS::Components::ShapeLine2D")) continue;
        }

        ECS::Entity parentEntity = m_world->Resolve(parentId);
        if (parentId == ECS::Entity::NPOS32 || parentEntity.IsNull()) {
            if (m_world->Has<ECS::Parent>(instance)) {
                m_world->Remove<ECS::Parent>(instance);
            }
        }
        else {
            // Directly assign parent without Set/Add
            if (m_world->Has<ECS::Parent>(instance)) {
                auto& parentComp = m_world->Get<ECS::Parent>(instance);
                parentComp.ParentEntity = parentEntity;
            }
            else {
                m_world->Add<ECS::Parent>(instance, parentEntity);
            }
        }

        std::filesystem::path p(prefabPath);
        std::string normalizedPath = p.lexically_normal().string();

        if (m_world->Has<ECS::Components::PrefabLink>(instance)) {
            auto& link = m_world->Get<ECS::Components::PrefabLink>(instance);
            link.setPath(normalizedPath);
        }
        else {
            m_world->Add<ECS::Components::PrefabLink>(instance, normalizedPath);
        }

        std::string prefabName = std::filesystem::path(prefabPath).stem().string();
        LOG_INFO("Instantiated prefab '" << prefabName << "' as child of entity " << parentId);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to instantiate prefab: " << e.what());
    }
}