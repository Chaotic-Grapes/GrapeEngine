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
#include "helpers/MathUtils.h"
#include "services/Input.h"
#include "serialization/EntitySerializer.h"
#include "core/Application.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>

namespace {
    template <typename T>
    bool AddComponentIfMatch(ECS::World* world,
                             ECS::Entity instance,
                             const std::string& typeName,
                             const std::string& expectedName,
                             nlohmann::json& compData) {
        if (typeName != expectedName) return false;
        if (!world->Has<T>(instance)) {
            auto& c = world->Add<T>(instance);
            from_json(compData, c);
        }
        return true;
    }
}

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
            m_editorCore->AddEntity(nameBuffer, ECS::Entity::NPOS32);
            // Auto-select the most recently created entity (highest ID)
            EntityId newestId = 0;
            m_world->Each([&](ECS::Entity e) {
                if (e.Index > newestId) newestId = e.Index;
            });
            if (newestId != 0) {
                m_selectedEntityId = newestId;
                if (m_selectionCallback) m_selectionCallback(newestId);
            }
        }
    }

    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2));

    // Count entities for display
    size_t entityCount = 0;
    m_world->Each([&](ECS::Entity e) {
        entityCount++;
        });

    ImGui::Text("Objects (%zu)", entityCount);

    ImGui::BeginChild("HierarchyTree", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);

    // Get fresh root entities list every frame
    auto rootEntities = _getRootEntities();
    // Ensure consistent ordering: older (lower indices) above, newer below
    std::sort(rootEntities.begin(), rootEntities.end());

    for (auto entityId : rootEntities) {
        _renderEntityNode(entityId, 0);
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;
            if (m_editorCore) {
                m_editorCore->ReparentEntity(draggedId, ECS::Entity::NPOS32);
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                _instantiatePrefabAsChild(droppedPath, ECS::Entity::NPOS32);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;
            if (m_editorCore) {
                m_editorCore->ReparentEntity(draggedId, ECS::Entity::NPOS32);
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                _instantiatePrefabAsChild(droppedPath, ECS::Entity::NPOS32);
            }
        }
        ImGui::EndDragDropTarget();
    }

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
    ECS::Entity entity = m_world->Resolve(entityId);
    if (entity.IsNull() || !m_world->IsAlive(entity)) return;

    // (Removed) Editor camera filtering; show all entities as before

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

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    if (m_selectedEntityId == entityId) flags |= ImGuiTreeNodeFlags_Selected;

    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(entityId + 1), flags, "%s", label.c_str());

    if (ImGui::IsItemClicked()) {
        m_selectedEntityId = entityId;
        if (m_selectionCallback) m_selectionCallback(entityId);
    }

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

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;
            if (draggedId != entityId && m_editorCore) {
                // Check if draggedId is ancestor of entityId to prevent circular parenting
                bool isAncestor = false;
                EntityId checkId = entityId;
                while (checkId != ECS::Entity::NPOS32) {
                    if (checkId == draggedId) {
                        isAncestor = true;
                        break;
                    }
                    ECS::Entity checkEntity{ checkId, 0 };
                    if (m_world->Has<ECS::Parent>(checkEntity)) {
                        checkId = m_world->Get<ECS::Parent>(checkEntity).ParentEntity.Index;
                    }
                    else {
                        break;
                    }
                }

                if (!isAncestor) {
                    m_editorCore->ReparentEntity(draggedId, entityId);
                }
                else {
                    LOG_WARNING("Invalid reparent: operation would create a circular hierarchy");
                }
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
            // If the deleted entity was selected, clear selection and notify
            if (m_selectedEntityId == entityId) {
                m_selectedEntityId = 0;
                if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
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
        // Always show the editor camera in hierarchy for clarity

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
    // Children come in newest-first from the ECS; reverse to show oldest-first
    std::reverse(children.begin(), children.end());
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

        ECS::Entity instance = m_world->Create();

        bool hasName = false;
        bool hasLocalTransform = false;
        for (const auto& comp : prefabJson["Components"]) {
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

        for (const auto& comp : prefabJson["Components"]) {
            if (!comp.contains("TypeName") || !comp["TypeName"].is_string())
                continue;
            if (comp["TypeName"] == "ECS::Components::Name") continue;

            std::string typeName = comp["TypeName"].get<std::string>();
            auto compData = (comp.contains("Data") && comp["Data"].is_object())
                ? comp["Data"]
                : nlohmann::json::object();

            // Use a file-scope templated helper to avoid MSVC restriction on local class templates

            if (AddComponentIfMatch<ECS::Components::LocalTransform>(m_world, instance, typeName, "ECS::Components::LocalTransform", compData)) { hasLocalTransform = true; continue; }
            if (AddComponentIfMatch<ECS::Components::SpriteRenderer2D>(m_world, instance, typeName, "ECS::Components::SpriteRenderer2D", compData)) continue;
            if (AddComponentIfMatch<ECS::Components::Rigidbody2D>(m_world, instance, typeName, "ECS::Components::Rigidbody2D", compData)) continue;
            if (AddComponentIfMatch<ECS::Components::CircleCollider2D>(m_world, instance, typeName, "ECS::Components::CircleCollider2D", compData)) continue;
            if (AddComponentIfMatch<ECS::Components::BoxCollider2D>(m_world, instance, typeName, "ECS::Components::BoxCollider2D", compData)) continue;
            if (AddComponentIfMatch<ECS::Components::ShapeCircle2D>(m_world, instance, typeName, "ECS::Components::ShapeCircle2D", compData)) continue;
            if (AddComponentIfMatch<ECS::Components::ShapeBox2D>(m_world, instance, typeName, "ECS::Components::ShapeBox2D", compData)) continue;
            if (AddComponentIfMatch<ECS::Components::ShapeLine2D>(m_world, instance, typeName, "ECS::Components::ShapeLine2D", compData)) continue;
        }

        // Ensure sensible defaults if missing from prefab
        if (!hasName) {
            std::string prefabName = std::filesystem::path(prefabPath).stem().string();
            auto& nameComp = m_world->Add<ECS::Components::Name>(instance);
            std::strncpy(nameComp.Value, prefabName.c_str(), sizeof(nameComp.Value) - 1);
            nameComp.Value[sizeof(nameComp.Value) - 1] = '\0';
        }
        if (!hasLocalTransform) {
            m_world->Add<ECS::Components::LocalTransform>(instance);
        }

        ECS::Entity parentEntity = m_world->Resolve(parentId);
        if (parentId == ECS::Entity::NPOS32 || parentEntity.IsNull()) {
            if (m_world->Has<ECS::Parent>(instance)) {
                m_world->Remove<ECS::Parent>(instance);
            }
        }
        else {
            if (m_world->Has<ECS::Parent>(instance)) {
                auto& parentComp = m_world->Get<ECS::Parent>(instance);
                parentComp.ParentEntity = parentEntity;
            }
            else {
                m_world->Add<ECS::Parent>(instance, ECS::Parent{ parentEntity });
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
        if (parentId == ECS::Entity::NPOS32) {
            LOG_INFO("Instantiated prefab '" << prefabName << "' as child of ROOT");
        }
        else {
            LOG_INFO("Instantiated prefab '" << prefabName << "' as child of entity " << parentId);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to instantiate prefab: " << e.what());
    }
}
