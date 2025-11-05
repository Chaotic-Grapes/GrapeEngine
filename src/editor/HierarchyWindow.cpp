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
- HierarchyWindow = Pure UI/View layer
- EditorCore = Entity/Level management operations

HOW TO USE:
- Click entity to select it, selection callback triggers
- Enter name + Add to create new entity (delegates to EditorCore)
- Tree shows root entities (ParentId 0) and children recursively
- Drag entity onto another to reparent (delegates to EditorCore)
- Right-click menu: Add Child, Clone, Delete (all delegate to EditorCore)
- PrefabLink marks prefab instances and stores normalized path for consistent identification
- Clicking empty space clears selection, Clear All deletes everything
*/
/* End Header *******************************************************************/

#include "../editor/HierarchyWindow.h"
#include "core/Logger.h"
#include "helpers/MathHelper.h"
#include "services/Input.h"
#include "serialization/EntitySerializer.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>

// Initialize fonts, world reference, and EditorCore for entity operations
void HierarchyWindow::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world, EditorCore* editorCore) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;           // Save reference to world so we can access entities
    m_editorCore = editorCore; // Save reference to EditorCore for entity operations
}

// Render the full hierarchy window and handle all interactions
void HierarchyWindow::Render() {
    // If world is null, we can't render anything
    if (!m_world) return;

    ImGui::Begin("Hierarchy");

    // Input field for creating a new entity
    ImGui::Text("Create New Object");
    static char nameBuffer[128] = "NewObject";                           // Buffer for new entity name
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);             // Limit input width
    ImGui::InputText("##NewObjectName", nameBuffer, sizeof(nameBuffer)); // InputText returns true if edited

    ImGui::SameLine();
    if (ImGui::Button("Add") && strlen(nameBuffer) > 0) {
        // Delegate entity creation to EditorCore
        if (m_editorCore) {
            m_editorCore->AddEntity(nameBuffer, m_selectedEntityId);
        }
    }

    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2));

    const auto allEntities = m_world->GetEntityManager().GetAllEntities(); // Get all entity IDs
    ImGui::Text("Objects (%zu)", allEntities.size());                      // Display total entity count

    // Child region for scrolling tree
    ImGui::BeginChild("HierarchyTree", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);
    auto rootEntities = _getRootEntities(); // Fetch all entities with no parent
    for (auto entityId : rootEntities) {
        _renderEntityNode(entityId, 0);     // Render each root node recursively
    }

    // Handle prefab dropped onto empty space (CHILD) to create at root
    // ImGuiPayload contains the data being dragged (here it is a file path)
    // We only accept ASSET_PATH payload type
    // lexically_normal paths used later ensure consistent comparison across OS
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            // Cast payload data to string
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            // Only accept .prefab files
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                _instantiatePrefabAsChild(droppedPath, 0); // 0 parent means root
            }
        }
        ImGui::EndDragDropTarget();
    }

    // End the scrolling child region before handling window-level interactions
    ImGui::EndChild();

    // Also accept prefab drops on the remaining window area (outside the child)
    if (ImGui::BeginDragDropTarget()) {
        // Now accept prefab drops on the remaining window area (outside the child)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            // Cast payload data to string
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            // Only accept .prefab files
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                _instantiatePrefabAsChild(droppedPath, 0); // 0 parent means root
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Clicking empty space deselects any entity
    // !IsAnyItemHovered ensures clicks on buttons or text don't deselect
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered()) {
        if (m_selectedEntityId != 0) {
            m_selectedEntityId = 0;                          // Clear selected entity
            if (m_selectionCallback) m_selectionCallback(0); // Notify inspector to clear
        }
    }

    // Clear all entities button
    if (ImGui::Button("Clear All")) {
        if (m_editorCore) {
            m_editorCore->ClearAllEntities();
        }
        m_selectedEntityId = 0;                             // Clear selection
        if (m_selectionCallback) m_selectionCallback(0);    // Notify inspector
    }

    ImGui::End();
}

// Render a single entity node and its children recursively
void HierarchyWindow::_renderEntityNode(EntityId entityId, int depth) {
    // Fetch entity object
    auto entity = m_world->GetEntityManager().GetEntity(entityId);
    // Skip invalid entity
    if (entity.GetId() == 0) return;

    auto children = _getChildren(entityId); // Get children for this entity
    bool hasChildren = !children.empty();   // Check if node should be expandable

    // Build display label
    std::stringstream oss;
    oss << entity.GetName() << " (" << entityId << ")"; // Show name and ID
    if (auto* link = entity.GetComponent<Component::PrefabLink>()) {
        // PrefabLink exists -> append prefab filename
        std::string prefabName = std::filesystem::path(link->prefabPath).stem().string();
        oss << " [" << prefabName << "]";
    }
    std::string label = oss.str();

    // Setup ImGui tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;                       // Leaf nodes can't be expanded
    if (m_selectedEntityId == entityId) flags |= ImGuiTreeNodeFlags_Selected; // Highlight selected

    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entityId, flags, "%s", label.c_str());

    // Clicking selects this entity
    if (ImGui::IsItemClicked()) {
        m_selectedEntityId = entityId;                          // Update selection
        if (m_selectionCallback) m_selectionCallback(entityId); // Notify inspector
    }

    // Drag source for reparenting
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY_ID", &entityId, sizeof(EntityId)); // Pass entity ID
        ImGui::Text("Reparent %s", entity.GetName().c_str());                // Show feedback while dragging
        ImGui::EndDragDropSource();
    }

    // Drag drop target for both entities and prefab files
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;       // Cast payload to entity ID
            if (m_editorCore) {
                m_editorCore->ReparentEntity(draggedId, entityId); // Delegate to EditorCore
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                _instantiatePrefabAsChild(droppedPath, entityId); // Prefab becomes child
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Right-click context menu
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
                m_editorCore->RemoveEntity(entityId, true); // Recursive delete
            }
            ImGui::EndPopup();
            if (nodeOpen) ImGui::TreePop(); // TreePop needed if node was opened
            return;
        }
        ImGui::EndPopup();
    }

    // Render children recursively if node is open
    if (nodeOpen) {
        for (auto childId : children) {
            _renderEntityNode(childId, depth + 1);
        }
        ImGui::TreePop();
    }
}

// Get all root entities (ParentId == 0)
std::vector<EntityId> HierarchyWindow::_getRootEntities() {
    std::vector<EntityId> roots;                                       // Store IDs of root entities
    auto allEntities = m_world->GetEntityManager().GetAllEntities();   // Get all IDs

    for (auto id : allEntities) {
        auto entity = m_world->GetEntityManager().GetEntity(id);       // Fetch entity
        auto* transform = entity.GetComponent<Component::Transform>(); // Only care if has Transform
        // No parent means root
        if (transform && transform->ParentId == 0) {
            roots.push_back(id);
        }
    }

    return roots;
}

// Get all children of a given parent
std::vector<EntityId> HierarchyWindow::_getChildren(EntityId parentId) {
    std::vector<EntityId> children;
    auto allEntities = m_world->GetEntityManager().GetAllEntities();   // Get all entity IDs

    for (auto id : allEntities) {
        auto entity = m_world->GetEntityManager().GetEntity(id);       // Fetch entity
        auto* transform = entity.GetComponent<Component::Transform>(); // Check Transform
        // ParentId matches -> is child
        if (transform && transform->ParentId == parentId) {
            children.push_back(id);
        }
    }

    return children;
}

// Instantiate a prefab as child of a given entity
void HierarchyWindow::_instantiatePrefabAsChild(const std::string& prefabPath, EntityId parentId) {
    if (!m_world || prefabPath.empty()) return;

    try {
        std::ifstream file(prefabPath);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open prefab file: " << prefabPath);
            return;
        }

        nlohmann::json prefabJson;
        // Load JSON data
        file >> prefabJson;
        file.close();

        // Deserialize creates a new entity with all prefab components
        Entity instance = Serialization::EntitySerializer::DeserializeEntity(*m_world, prefabJson);

        // Set parent
        if (auto* transform = instance.GetComponent<Component::Transform>()) {
            transform->ParentId = parentId;
        }

        // Add PrefabLink component to mark as instance
        std::filesystem::path p(prefabPath);
        std::string normalizedPath = p.lexically_normal().string();

        // lexically_normal ensures path uses consistent separators and format
        instance.AddComponent<Component::PrefabLink>(normalizedPath);

        std::string prefabName = std::filesystem::path(prefabPath).stem().string();
        LOG_INFO("Instantiated prefab '" << prefabName << "' as child of entity " << parentId);
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to instantiate prefab: " << e.what());
    }
}
