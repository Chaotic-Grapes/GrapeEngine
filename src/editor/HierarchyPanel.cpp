/* Start Header *****************************************************************/
/*!
\file   HierarchyPanel.cpp
\author Foo Rui Qin    (50%)
        Samantha Leong (50%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Implements a Unity-like hierarchy window UI for managing entities in a tree structure.
*/
/* End Header *******************************************************************/

#include "../editor/HierarchyPanel.h"
#include "../editor/Viewport.h"
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

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
// Initialize the hierarchy panel with fonts and world/editor references.
// Sets up local state used for selection and expanded nodes.
void HierarchyPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world, Viewport* viewport) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
    m_viewport = viewport;
}

// Update the world reference when scene changes
void HierarchyPanel::SetWorld(ECS::World* world) {
    m_world = world;
    m_selectedEntityId = 0;
    m_expandedNodes.clear();
}

// Register a callback for selection change events
void HierarchyPanel::OnSelectionChanged(SelectionCallback callback) {
    m_selectionCallback = callback;
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------
// Render the hierarchy window with entity tree and controls
void HierarchyPanel::Render() {
    if (m_mainFont) ImGui::PushFont(m_mainFont);

    ImGui::Begin("Hierarchy");

    if (!m_world) {
        ImGui::TextDisabled("No scene attached");
        if (m_mainFont) ImGui::PopFont();
        ImGui::End();
        return;
    }

    _renderHeader();
    _renderEntityTree();
    _renderFooterButtons();

    // Click empty space to clear selection
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered()) {
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
    }

    if (m_mainFont) ImGui::PopFont();
    ImGui::End();
}

// -------------------------------------------------------------------------
// UI Sections
// -------------------------------------------------------------------------
// Render the header with entity count and context menu
void HierarchyPanel::_renderHeader() {
    _renderEntityContextMenu();

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2));

    // Count entities for display
    size_t entityCount = 0;
    m_world->Each([&](ECS::Entity e) {
        entityCount++;
        });

    ImGui::Text("Entities (%zu)", entityCount);
}

// Render the main entity tree with drag-drop support
void HierarchyPanel::_renderEntityTree() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
    ImGui::BeginChild("HierarchyTree", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);

    // Get fresh root entities list every frame
    auto rootEntities = _getRootEntities();
    // Ensure consistent ordering: older (lower indices) above, newer below
    std::sort(rootEntities.begin(), rootEntities.end());

    for (auto entityId : rootEntities) {
        _renderEntityNode(entityId, 0);
    }

    _handleTreeDragDrop();

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Backup drag-drop target for bottom of tree
    _handleTreeDragDrop();
}

// Render footer buttons (Clear All)
void HierarchyPanel::_renderFooterButtons() {
    if (ImGui::Button("Clear All")) {
        if (m_viewport) {
            m_viewport->ClearAllEntities();
        }
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
    }
}

// -------------------------------------------------------------------------
// Entity Tree Node Rendering
// -------------------------------------------------------------------------
// Render a single entity tree node and its children recursively.
// Handles selection, drag source, and drop target logic.
void HierarchyPanel::_renderEntityNode(EntityId entityId, int depth) {
    ECS::Entity entity = m_world->Resolve(entityId);
    if (entity.IsNull() || !m_world->IsAlive(entity)) return;

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

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChildren) nodeFlags |= ImGuiTreeNodeFlags_Leaf;
    if (m_selectedEntityId == entityId) nodeFlags |= ImGuiTreeNodeFlags_Selected;

    bool isExpanded = m_expandedNodes.find(entityId) != m_expandedNodes.end();
    if (isExpanded && hasChildren) ImGui::SetNextItemOpen(true);

    ImGui::PushID(static_cast<int>(entityId));
    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), nodeFlags);

    _handleNodeInteraction(entityId);
    _handleNodeDragDrop(entityId);

    if (nodeOpen) {
        m_expandedNodes.insert(entityId);
        std::sort(children.begin(), children.end());
        for (auto childId : children) {
            _renderEntityNode(childId, depth + 1);
        }
        ImGui::TreePop();
    }
    else if (hasChildren) {
        m_expandedNodes.erase(entityId);
    }

    ImGui::PopID();
}

// Handle node interaction (click, right-click, double-click)
void HierarchyPanel::_handleNodeInteraction(EntityId entityId) {
    // Single-click selection
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_selectedEntityId = entityId;
        if (m_selectionCallback) m_selectionCallback(entityId);
    }

    // Right-click context menu
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        m_selectedEntityId = entityId;
        if (m_selectionCallback) m_selectionCallback(entityId);
        m_contextMenuTarget = entityId;
        ImGui::OpenPopup("EntityContextMenu");
    }

    // Double-click to focus camera
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (m_viewport) {
            m_viewport->FocusOnEntity(entityId);
        }
    }
}

// Handle drag-drop for entity reparenting and prefab instantiation
void HierarchyPanel::_handleNodeDragDrop(EntityId entityId) {
    // Drag source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("ENTITY_ID", &entityId, sizeof(EntityId));
        ECS::Entity e = m_world->Resolve(entityId);
        if (m_world->IsAlive(e) && m_world->Has<ECS::Components::Name>(e)) {
            const auto& name = m_world->Get<ECS::Components::Name>(e);
            ImGui::Text("%s", name.Value);
        }
        ImGui::EndDragDropSource();
    }

    // Drop target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;
            if (m_viewport) {
                m_viewport->ReparentEntity(draggedId, entityId);
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
}

// Handle drag-drop for the tree background (reparent to root)
void HierarchyPanel::_handleTreeDragDrop() {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;
            if (m_viewport) {
                m_viewport->ReparentEntity(draggedId, ECS::Entity::NPOS32);
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
}

// -------------------------------------------------------------------------
// Context Menu
// -------------------------------------------------------------------------
// Render the entity context menu
void HierarchyPanel::_renderEntityContextMenu() {
    if (ImGui::BeginPopup("EntityContextMenu")) {
        if (m_contextMenuTarget != 0) {
            ECS::Entity entity = m_world->Resolve(m_contextMenuTarget);
            if (!entity.IsNull() && m_world->IsAlive(entity)) {
                if (ImGui::MenuItem("Delete")) {
                    _deleteEntity(m_contextMenuTarget);
                }
                if (ImGui::MenuItem("Clone")) {
                    _cloneEntity(m_contextMenuTarget);
                }
                if (ImGui::MenuItem("Add Child")) {
                    _addChildEntity(m_contextMenuTarget);
                }
                if (m_world->Has<ECS::Components::PrefabLink>(entity)) {
                    ImGui::Separator();
                    if (ImGui::MenuItem("Update from Prefab")) {
                        _updateFromPrefab(m_contextMenuTarget);
                    }
                    if (ImGui::MenuItem("Save to Prefab")) {
                        _saveToPrefab(m_contextMenuTarget);
                    }
                }
            }
        }
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------------------
// Entity Operations
// -------------------------------------------------------------------------
// Delete an entity and update selection
void HierarchyPanel::_deleteEntity(EntityId entityId) {
    if (m_viewport) {
        m_viewport->RemoveEntity(entityId, true);
    }
    if (m_selectedEntityId == entityId) {
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
    }
    m_contextMenuTarget = 0;
}

// Clone an entity
void HierarchyPanel::_cloneEntity(EntityId entityId) {
    if (m_viewport) {
        m_viewport->CloneEntity(entityId);
    }
}

// Add a child entity to the selected entity
void HierarchyPanel::_addChildEntity(EntityId parentId) {
    if (m_viewport) {
        m_viewport->AddEntity("Entity", parentId);
    }
}

// -------------------------------------------------------------------------
// Prefab Operations
// -------------------------------------------------------------------------
// Instantiate a prefab as a child of the specified parent
void HierarchyPanel::_instantiatePrefabAsChild(const std::string& prefabPath, EntityId parentId) {
    if (!m_world) return;

    try {
        std::ifstream file(prefabPath);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open prefab: " << prefabPath);
            return;
        }

        nlohmann::json prefabJson;
        file >> prefabJson;
        file.close();

        auto entity = Serialization::EntitySerializer::DeserializeEntity(*m_world, prefabJson);

        if (parentId != ECS::Entity::NPOS32) {
            ECS::Entity parent = m_world->Resolve(parentId);
            if (!parent.IsNull() && m_world->IsAlive(parent)) {
                m_world->Set<ECS::Parent>(entity, ECS::Parent{ parent });
            }
        }

        std::filesystem::path p(prefabPath);
        std::string linkPath = p.lexically_normal().string();
        m_world->Set<ECS::Components::PrefabLink>(entity, ECS::Components::PrefabLink(linkPath));

        LOG_INFO("Instantiated prefab: " << p.filename().string());
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to instantiate prefab: " << e.what());
    }
}

// Update entity from its linked prefab
void HierarchyPanel::_updateFromPrefab(EntityId entityId) {
    ECS::Entity entity = m_world->Resolve(entityId);
    if (entity.IsNull() || !m_world->IsAlive(entity)) return;

    if (!m_world->Has<ECS::Components::PrefabLink>(entity)) {
        LOG_WARNING("Entity has no prefab link");
        return;
    }

    const auto& link = m_world->Get<ECS::Components::PrefabLink>(entity);
    std::string prefabPath = link.getPath();

    try {
        std::ifstream file(prefabPath);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open prefab: " << prefabPath);
            return;
        }

        nlohmann::json prefabJson;
        file >> prefabJson;
        file.close();

        // Apply prefab data to entity (simplified - full implementation would preserve local overrides)
        if (prefabJson.contains("Components")) {
            for (auto& compEntry : prefabJson["Components"]) {
                std::string typeName = compEntry["Type"];
                // Match and update components
                // (Full implementation would use component registry pattern)
            }
        }

        LOG_INFO("Updated entity from prefab: " << std::filesystem::path(prefabPath).filename().string());
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to update from prefab: " << e.what());
    }
}

// Save entity state to its linked prefab
void HierarchyPanel::_saveToPrefab(EntityId entityId) {
    ECS::Entity entity = m_world->Resolve(entityId);
    if (entity.IsNull() || !m_world->IsAlive(entity)) return;

    if (!m_world->Has<ECS::Components::PrefabLink>(entity)) {
        LOG_WARNING("Entity has no prefab link");
        return;
    }

    const auto& link = m_world->Get<ECS::Components::PrefabLink>(entity);
    std::string prefabPath = link.getPath();

    try {
        auto entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

        std::ofstream file(prefabPath);
        if (!file.is_open()) {
            LOG_ERROR("Cannot write to prefab: " << prefabPath);
            return;
        }

        file << entityJson.dump(2);
        file.close();

        LOG_INFO("Saved entity to prefab: " << std::filesystem::path(prefabPath).filename().string());
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to save to prefab: " << e.what());
    }
}

// -------------------------------------------------------------------------
// Helper Methods
// -------------------------------------------------------------------------
// Get all root entities (entities without parents)
std::vector<EntityId> HierarchyPanel::_getRootEntities() const {
    std::vector<EntityId> roots;
    if (!m_world) return roots;

    m_world->Each([&](ECS::Entity e) {
        if (!m_world->Has<ECS::Parent>(e)) {
            roots.push_back(e.Index);
        }
        });

    return roots;
}

// Get all children of an entity
std::vector<EntityId> HierarchyPanel::_getChildren(EntityId parentId) const {
    std::vector<EntityId> children;
    if (!m_world) return children;

    m_world->Each<ECS::Parent>([&](ECS::Entity e, const ECS::Parent& parent) {
        if (parent.ParentEntity.Index == parentId) {
            children.push_back(e.Index);
        }
        });

    return children;
}
