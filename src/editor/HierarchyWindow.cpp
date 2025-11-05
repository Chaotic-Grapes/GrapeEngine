/* Start Header *****************************************************************/
/*!
\file   HierarchyWindow.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   5th November 2025
\brief
Implements the hierarchy window displaying all entities in a tree structure.
*/
/* End Header *******************************************************************/

#include "../editor/HierarchyWindow.h"
#include "core/Logger.h"
#include "helpers/MathHelper.h"
#include "services/Input.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>

void HierarchyWindow::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
}

void HierarchyWindow::Render() {
    if (!m_world) return;

    ImGui::Begin("Hierarchy");

    // Add new entity section
    ImGui::Text("Create New Object");
    static char nameBuffer[128] = "NewObject";
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
    ImGui::InputText("##NewObjectName", nameBuffer, sizeof(nameBuffer));

    ImGui::SameLine();
    if (ImGui::Button("Add") && strlen(nameBuffer) > 0) {
        // Add as child of selected entity if one is selected
        _addEntity(nameBuffer, m_selectedEntityId);
    }

    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2));

    // Display entity count
    const auto allEntities = m_world->GetEntityManager().GetAllEntities();
    ImGui::Text("Objects (%zu)", allEntities.size());

    // Scrollable tree region
    ImGui::BeginChild("HierarchyTree", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);

    // Render root entities (those with no parent)
    auto rootEntities = _getRootEntities();
    for (auto entityId : rootEntities) {
        _renderEntityNode(entityId, 0);
    }

    // If user clicks on empty space inside the hierarchy tree, clear selection
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered()) {
        if (m_selectedEntityId != 0) {
            m_selectedEntityId = 0;
            if (m_selectionCallback) m_selectionCallback(0);
        }
    }

    ImGui::EndChild();

    // Bottom buttons
    if (ImGui::Button("Clear All")) {
        m_world->GetEntityManager().DestroyAllEntities();
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(0);
    }

    ImGui::End();
}

void HierarchyWindow::_renderEntityNode(EntityId entityId, int depth) {
    auto entity = m_world->GetEntityManager().GetEntity(entityId);
    if (entity.GetId() == 0) return;

    // Get children to determine if this is a parent node
    auto children = _getChildren(entityId);
    bool hasChildren = !children.empty();

    // Build label
    std::stringstream oss;
    oss << entity.GetName() << " (" << entityId << ")";
    std::string label = oss.str();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    if (m_selectedEntityId == entityId) flags |= ImGuiTreeNodeFlags_Selected;

    // Render tree node
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entityId, flags, "%s", label.c_str());

    // Handle selection
    if (ImGui::IsItemClicked()) {
        m_selectedEntityId = entityId;
        if (m_selectionCallback) m_selectionCallback(entityId);
    }

    // Drag source (for reparenting)
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY_ID", &entityId, sizeof(EntityId));
        ImGui::Text("Reparent %s", entity.GetName().c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target (for reparenting)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;
            _reparentEntity(draggedId, entityId);
        }
        ImGui::EndDragDropTarget();
    }

    // Right-click context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::Selectable("Add Child")) {
            _addEntity("Child", entityId);
        }
        if (ImGui::Selectable("Clone")) {
            _cloneEntity(entityId);
        }
        if (ImGui::Selectable("Delete")) {
            _removeEntity(entityId);
            ImGui::EndPopup();
            if (nodeOpen) ImGui::TreePop();
            return;
        }
        ImGui::EndPopup();
    }

    // Render children if expanded
    if (nodeOpen) {
        if (hasChildren) {
            for (auto childId : children) {
                _renderEntityNode(childId, depth + 1);
            }
        }
        ImGui::TreePop();
    }
}

std::vector<EntityId> HierarchyWindow::_getRootEntities() {
    std::vector<EntityId> roots;
    auto allEntities = m_world->GetEntityManager().GetAllEntities();

    for (auto id : allEntities) {
        auto entity = m_world->GetEntityManager().GetEntity(id);
        auto* transform = entity.GetComponent<Component::Transform>();
        
        // Root entities have no parent (ParentId == 0)
        if (transform && transform->ParentId == 0) {
            roots.push_back(id);
        }
    }

    return roots;
}

std::vector<EntityId> HierarchyWindow::_getChildren(EntityId parentId) {
    std::vector<EntityId> children;
    auto allEntities = m_world->GetEntityManager().GetAllEntities();

    for (auto id : allEntities) {
        auto entity = m_world->GetEntityManager().GetEntity(id);
        auto* transform = entity.GetComponent<Component::Transform>();
        
        if (transform && transform->ParentId == parentId) {
            children.push_back(id);
        }
    }

    return children;
}

void HierarchyWindow::_addEntity(const std::string& name, EntityId parentId) {
    if (!m_world || name.empty()) return;

    // Create entity with Transform
    auto entity = m_world->CreateEntity(name);
    entity.AddComponent<Component::Transform>();

    // Set parent
    if (parentId != 0) {
        auto* transform = entity.GetComponent<Component::Transform>();
        if (transform) {
            transform->ParentId = parentId;
        }
    }

    // Add default visual component
    auto& shapeRenderer = entity.AddComponent<Component::ShapeRenderer2D>();
    shapeRenderer.Type = Component::ShapeRenderer2D::ShapeType::Circle;
    shapeRenderer.Radius = 35.0f;
    shapeRenderer.FillColor = Color(1.0f, 1.0f, 1.0f, 1.0f);

    // Random position
    entity.Transform().Position.X = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowWidth()));
    entity.Transform().Position.Y = MathHelper::Randomize(0.0f, static_cast<float>(Input::GetWindowHeight()));

    LOG_INFO("Added entity: " << name);
}

void HierarchyWindow::_removeEntity(EntityId id) {
    if (!m_world) return;

    // Also remove all children recursively
    auto children = _getChildren(id);
    for (auto childId : children) {
        _removeEntity(childId);
    }

    auto entity = m_world->GetEntityManager().GetEntity(id);
    m_world->GetEntityManager().DestroyEntity(entity);

    if (m_selectedEntityId == id) {
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(0);
    }
}

void HierarchyWindow::_cloneEntity(EntityId id) {
    if (!m_world) return;

    auto entity = m_world->GetEntityManager().GetEntity(id);
    auto cloned = entity.Clone();

    // Offset position
    cloned.Transform().Position.X += 50.0f;
    cloned.Transform().Position.Y += 50.0f;

    LOG_INFO("Cloned entity: " << entity.GetName());
}

void HierarchyWindow::_reparentEntity(EntityId childId, EntityId newParentId) {
    if (!m_world || childId == newParentId) return;

    // Prevent circular parenting (child can't be parent of its own ancestor)
    EntityId checkId = newParentId;
    while (checkId != 0) {
        if (checkId == childId) {
            LOG_WARNING("Cannot create circular parent relationship");
            return;
        }
        auto checkEntity = m_world->GetEntityManager().GetEntity(checkId);
        auto* checkTransform = checkEntity.GetComponent<Component::Transform>();
        checkId = checkTransform ? checkTransform->ParentId : 0;
    }

    // Set new parent
    auto entity = m_world->GetEntityManager().GetEntity(childId);
    auto* transform = entity.GetComponent<Component::Transform>();
    if (transform) {
        transform->ParentId = newParentId;
        LOG_INFO("Reparented entity " << childId << " to " << newParentId);
    }
}
