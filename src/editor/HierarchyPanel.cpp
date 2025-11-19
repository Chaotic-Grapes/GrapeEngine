/* Start Header *****************************************************************/
/*!
\file   HierarchyPanel.cpp
\author Foo Rui Qin    (50%)
        Samantha Leong (50%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Implements the Hierarchy panel that shows all scene entities as a tree.

The hierarchy gives a structured view of the active scene and is used to select
entities, organize them through parent-child relationships and perform common
editor actions like create, delete, clone and reparent through drag-drop. It also
syncs with the inspector and viewport so selection stays consistent across the UI
and supports prefab instantiation by accepting dragged prefab assets.
*/
/* End Header *******************************************************************/

#include "../editor/HierarchyPanel.h"
#include "../editor/ComponentWidgets.h"
#include "../editor/EditorComponentRegistry.h"
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
    // Helper template function to safely add components during deserialization
    // Checks if the component type matches expected name before adding
    template <typename T>
    bool AddComponentIfMatch(ECS::World* world, ECS::Entity instance, const std::string& typeName,
        const std::string& expectedName, nlohmann::json& compData)
    {
        // If the json component name does not match what this function handles we skip it
        // This avoids adding the wrong component type to the entity
        if (typeName != expectedName) return false;

        // Only add the component if the entity does not already have it
        // world Has<T> checks if this entity already contains a component of type T
        if (!world->Has<T>(instance)) {
            // Add<T> attaches the component to the entity and returns a reference to it
            auto& c = world->Add<T>(instance);
            // from_json fills the new component using values from the json 
            // This allows prefabs and saved scenes to restore component state exactly
            from_json(compData, c);
        }
        // true means this template handled the component successfully
        return true;
    }

    // Helper function to check if entity should be hidden from hierarchy
    // Returns true if entity should be filtered out (hidden)
    bool ShouldHideFromHierarchy(ECS::World* world, ECS::Entity entity) {
        if (!world || entity.IsNull() || !world->IsAlive(entity)) return true;

        // Hide entity ID 0 (EditorCamera and other system entities)
        if (entity.Index == 0) return true;

        // Additional filter: hide entities named "EditorCamera" regardless of ID
        if (world->Has<ECS::Components::Name>(entity)) {
            const auto& name = world->Get<ECS::Components::Name>(entity);
            if (std::string(name.Value) == "EditorCamera") {
                return true;
            }
        }

        return false;
    }

    // Helper function to check if an entity is protected from modification
    // Returns true if entity should NOT be modified (deleted, cloned, reparented, etc.)
    bool IsProtectedEntity(EntityId entityId) {
        // Protect entity ID 0 (EditorCamera) from all modifications
        return (entityId == 0);
    }
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

// Initialize the hierarchy panel with fonts and world/editor references
// Sets up local state used for selection and expanded nodes
void HierarchyPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
    ECS::World* world, EntityActions* entityActions)
{
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    // Cache pointers to world and entity actions so we operate on correct scene
    m_world = world;
    m_entityActions = entityActions;
}

// Update the world reference when scene changes
// Clears selection and expanded nodes when switching to a new world
void HierarchyPanel::SetWorld(ECS::World* world) {
    m_world = world;
    // Reset selection when scene changes
    m_selectedEntityId = 0;
    // Clear expanded nodes so tree redraws cleanly
    m_expandedNodes.clear();
}

// Register a callback for selection change events
// This allows other systems to react when entity selection changes
void HierarchyPanel::OnSelectionChanged(SelectionCallback callback) {
    m_selectionCallback = callback;
}

// Add this implementation
void HierarchyPanel::SetSelectedEntity(EntityId id) {
    m_selectedEntityId = id;
    // Don't trigger the callback here to avoid circular updates
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

// Render the hierarchy window with entity tree and controls
// Handles entity selection, drag-drop and keyboard shortcuts
void HierarchyPanel::Render() {
    // Push main font for consistent text styling
    if (m_mainFont) ImGui::PushFont(m_mainFont);

    // Begin the hierarchy window
    ImGui::Begin("Hierarchy");

    // Early return if no world is attached to prevent crashes
    if (!m_world) {
        ImGui::TextDisabled("No scene attached");
        ImGui::TextDisabled("Create a new scene or open one via File");
        if (m_mainFont) ImGui::PopFont();
        ImGui::End();
        return;
    }

    // Render the main UI sections
    _renderHeader();           // Header with entity creation controls
    _renderEntityTree();       // Main entity tree with drag-drop
    _renderFooterButtons();    // Footer buttons like Clear All

    // Handle delete key for selected entity: global keyboard shortcut
    // Block deletion of protected entities (ID 0)
    if (Input::IsKeyDown(KEY_DELETE) && !IsProtectedEntity(m_selectedEntityId)) {
        _deleteEntity(m_selectedEntityId);
    }

    // Handle F2 key for renaming selected entity
    if (ImGui::IsKeyPressed(ImGuiKey_F2) && m_selectedEntityId != 0 && !IsProtectedEntity(m_selectedEntityId)) {
        _startRename(m_selectedEntityId);
    }

    // Click empty space to clear selection
    // Only clears if clicking on window background, not on any items
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered())
    {
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
    }

    // Clean up font and end window
    if (m_mainFont) ImGui::PopFont();
    ImGui::End();
}

// -------------------------------------------------------------------------
// UI Sections
// -------------------------------------------------------------------------

// Render the header with entity count, context menu & add entity controls
void HierarchyPanel::_renderHeader() {
    // Search bar for filtering entities
    ImGui::SetNextItemWidth(-1); // Full width
    if (ImGui::InputTextWithHint("##SearchFilter", "Search...", m_searchBuffer, sizeof(m_searchBuffer))) {
        m_searchFilter = m_searchBuffer;
    }
    ImGui::Dummy(ImVec2(0, 2));

    // Entity creation section
    // ImGui::Text("Create New Entity");
    static char nameBuffer[128] = "New Entity";

    // Input text for entity name with responsive width
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
    ImGui::InputText("##NewEntityName", nameBuffer, sizeof(nameBuffer));

    // Add entity button
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        // Use default name if buffer is empty
        std::string entityName = (strlen(nameBuffer) > 0) ? nameBuffer : "New Entity";
        if (m_entityActions) {
            // Add as root entity (NPOS32 means no parent)
            m_entityActions->AddEntity(entityName, ECS::Entity::NPOS32);
        }
        // Don't clear the buffer: keep the name for easy repeated additions
    }

    // Visual spacing
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2));

    // Count visible entities (excluding hidden ones like EditorCamera)
    size_t entityCount = 0;
    m_world->Each([&](ECS::Entity e) {
        if (!ShouldHideFromHierarchy(m_world, e)) {
            entityCount++;
        }
        });

    // Display entity count
    ImGui::Text("Entities (%zu)", entityCount);
    ImGui::Dummy(ImVec2(0, 2));
}

// Render the main entity tree with drag-drop support
// This is the core of the hierarchy panel showing all entities
void HierarchyPanel::_renderEntityTree() {
    // Adjust frame padding for better tree node appearance
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));

    // Create scrollable child region for the tree
    ImGui::BeginChild("HierarchyTree", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);

    // Get fresh root entities list every frame to reflect changes
    auto rootEntities = _getRootEntities();

    // Sort by index to display entities in consistent order (1, 2, 3, ...)
    std::sort(rootEntities.begin(), rootEntities.end());

    // Render each root entity and its children recursively
    for (auto entityId : rootEntities) {
        _renderEntityNode(entityId, 0);
    }

    // Root-level drop target: accept reparenting into root
    // This handles dropping entities to make them root-level
    _handleTreeDragDrop();

    // Click empty space in tree to clear selection
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered())
    {
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Backup drag-drop target for bottom of tree
    // Ensures drop targets are available even when tree is empty
    _handleTreeDragDrop();
}

// Render footer buttons (Clear All)
void HierarchyPanel::_renderFooterButtons() {
    // Clear All button: removes all entities from the scene
    if (ImGui::Button("Clear All")) {
        if (m_entityActions) {
            m_entityActions->ClearAllEntities();
        }
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
    }
}

// -------------------------------------------------------------------------
// Entity Tree Node Rendering
// -------------------------------------------------------------------------

// Render a single entity tree node and its children recursively
// Handles selection, drag source and drop target logic
// Depth parameter tracks recursion level for indentation
void HierarchyPanel::_renderEntityNode(EntityId entityId, int depth) {
    // Resolve entity ID to entity object and check if valid
    ECS::Entity entity = m_world->Resolve(entityId);

    // Skip hidden entities (like EditorCamera at ID 0)
    if (ShouldHideFromHierarchy(m_world, entity)) return;

    // Get children for this entity to determine if it's a leaf node
    auto children = _getChildren(entityId);
    bool hasChildren = !children.empty();

    // Build display label with entity name
    std::stringstream oss;
    if (m_world->Has<ECS::Components::Name>(entity)) {
        const auto& nameComp = m_world->Get<ECS::Components::Name>(entity);
        oss << nameComp.Value;
    }
    else {
        oss << "Entity";
    }

    // Append prefab indicator if this is a prefab instance
    if (m_world->Has<ECS::Components::PrefabLink>(entity)) {
        const auto& link = m_world->Get<ECS::Components::PrefabLink>(entity);
        std::string prefabName = std::filesystem::path(link.getPath()).stem().string();
        oss << " [" << prefabName << "]";
    }

    std::string label = oss.str();

    // Skip this entity if it doesn't match the search filter
    if (!_matchesSearchFilter(entityId)) {
        return;
    }

    // Configure tree node flags for proper behavior and appearance
    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow   // Open when clicking arrow
        | ImGuiTreeNodeFlags_OpenOnDoubleClick                      // Open when double-clicking
        | ImGuiTreeNodeFlags_SpanAvailWidth;                        // Make entire row hoverable/selectable

    // Mark as leaf if no children (changes arrow behavior)
    if (!hasChildren) nodeFlags |= ImGuiTreeNodeFlags_Leaf;

    // Highlight if this entity is selected
    if (m_selectedEntityId == entityId) nodeFlags |= ImGuiTreeNodeFlags_Selected;

    // Manage expanded state - open if previously expanded
    bool isExpanded = m_expandedNodes.find(entityId) != m_expandedNodes.end();
    if (isExpanded && hasChildren) ImGui::SetNextItemOpen(true);

    // Push ID to ensure unique imgui identifiers
    ImGui::PushID(static_cast<int>(entityId));

    // Render rename input if this entity is being renamed
    bool nodeOpen = false;
    if (m_renamingEntityId == entityId) {
        // Show input field for renaming
        if (m_focusRenameInput) {
            ImGui::SetKeyboardFocusHere();
            m_focusRenameInput = false;
        }
        
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##RenameInput", m_renameBuffer, sizeof(m_renameBuffer), 
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            // Apply rename on Enter
            if (strlen(m_renameBuffer) > 0 && m_world->Has<ECS::Components::Name>(entity)) {
                auto& nameComp = m_world->Get<ECS::Components::Name>(entity);
                strncpy_s(nameComp.Value, m_renameBuffer, sizeof(nameComp.Value) - 1);
                nameComp.Value[sizeof(nameComp.Value) - 1] = '\0';
            }
            m_renamingEntityId = 0;
        }
        
        // Cancel rename on Escape or click away
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || 
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered())) {
            m_renamingEntityId = 0;
        }
    }
    else {
        // Render the actual tree node
        nodeOpen = ImGui::TreeNodeEx(label.c_str(), nodeFlags);

        // Handle interactions like clicks and drag-drop
        _handleNodeInteraction(entityId);
        _handleNodeDragDrop(entityId);
    }

    // Render context menu if opened
    _renderEntityContextMenu();

    // If node is open, render children recursively
    if (nodeOpen) {
        // Track that this node is expanded
        m_expandedNodes.insert(entityId);

        // Sort children by index for consistent display order
        std::sort(children.begin(), children.end());

        // Recursively render each child
        for (auto childId : children) {
            _renderEntityNode(childId, depth + 1);
        }
        ImGui::TreePop();
    }
    else if (hasChildren) {
        // Remove from expanded set when collapsed
        m_expandedNodes.erase(entityId);
    }

    ImGui::PopID();
}

// Handle node interaction (click, right-click, double-click)
void HierarchyPanel::_handleNodeInteraction(EntityId entityId) {
    // Block interaction with protected entities (shouldn't happen due to filtering, but just in case)
    if (IsProtectedEntity(entityId)) return;

    // Skip interaction if currently renaming
    if (m_renamingEntityId != 0) return;

    // Get current time for click timing
    float currentTime = ImGui::GetTime();

    // Right-click context menu: select and open context menu
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        m_selectedEntityId = entityId;
        if (m_selectionCallback) m_selectionCallback(entityId);
        m_contextMenuTarget = entityId;
        ImGui::OpenPopup("EntityContextMenu");
    }

    // Fast double-click to focus camera
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (m_entityActions) {
            // TODO: Implement camera focus functionality
            // This triggers on fast double-click
        }
        // Update click tracking
        m_lastClickedEntity = entityId;
        m_lastClickTime = currentTime;
    }
    // Handle single clicks (for selection and rename detection)
    else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        // Check if this is a slow second click on the same already-selected entity BEFORE updating times
        bool isSlowSecondClick = (m_lastClickedEntity == entityId && 
                                   m_selectedEntityId == entityId &&
                                   (currentTime - m_lastClickTime) > RENAME_DELAY_THRESHOLD &&
                                   (currentTime - m_lastClickTime) < 2.0f);
        
        if (isSlowSecondClick) {
            // Start rename mode
            _startRename(entityId);
        }
        else {
            // Normal selection
            m_selectedEntityId = entityId;
            if (m_selectionCallback) m_selectionCallback(entityId);
        }
        
        // Update click tracking for next time
        m_lastClickedEntity = entityId;
        m_lastClickTime = currentTime;
    }
}

// Handle drag-drop for entity reparenting and prefab instantiation
void HierarchyPanel::_handleNodeDragDrop(EntityId entityId) {
    // Block dragging protected entities
    if (IsProtectedEntity(entityId)) return;

    // Drag source: make this entity draggable
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        // Set payload with entity ID for drag-drop operations
        ImGui::SetDragDropPayload("ENTITY_ID", &entityId, sizeof(EntityId));

        // Show entity name as drag preview
        ECS::Entity entity = m_world->Resolve(entityId);
        // Only if the entity hasn't been destroyed
        if (m_world->IsAlive(entity) && m_world->Has<ECS::Components::Name>(entity)) {
            const auto& name = m_world->Get<ECS::Components::Name>(entity);
            ImGui::Text("%s", name.Value);
        }
        ImGui::EndDragDropSource();
    }

    // Drop target: accept entities and prefabs dropped on this node
    if (ImGui::BeginDragDropTarget()) {
        // Handle entity reparenting
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;

            // Block reparenting if either entity is protected
            if (!IsProtectedEntity(draggedId) && !IsProtectedEntity(entityId)) {
                if (m_entityActions) {
                    m_entityActions->ReparentEntity(draggedId, entityId);
                }
            }
        }

        // Handle prefab instantiation as child
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                // Instantiate the prefab as a child of the current entity node
                EntityId newEntityId = _instantiatePrefabAsChild(droppedPath, entityId);

                // Select the newly created entity so it shows up in inspector immediately
                if (newEntityId != ECS::Entity::NPOS32) {
                    m_selectedEntityId = newEntityId;
                    if (m_selectionCallback) m_selectionCallback(newEntityId);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
}

// Handle drag-drop for the tree background (reparent to root)
void HierarchyPanel::_handleTreeDragDrop() {
    if (ImGui::BeginDragDropTarget()) {
        // Reparent entity to root (make it have no parent)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;

            // Block reparenting protected entities
            if (!IsProtectedEntity(draggedId)) {
                if (m_entityActions) {
                    m_entityActions->ReparentEntity(draggedId, ECS::Entity::NPOS32);
                }
            }
        }

        // Instantiate prefab as root entity
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                EntityId newEntityId = _instantiatePrefabAsChild(droppedPath, ECS::Entity::NPOS32);

                // Select the newly created entity so it shows up in inspector immediately
                if (newEntityId != ECS::Entity::NPOS32) {
                    m_selectedEntityId = newEntityId;
                    if (m_selectionCallback) m_selectionCallback(newEntityId);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
}

// -------------------------------------------------------------------------
// Context Menu
// -------------------------------------------------------------------------

// Render the entity context menu with entity operations
void HierarchyPanel::_renderEntityContextMenu() {
    if (ImGui::BeginPopup("EntityContextMenu")) {
        ECS::Entity entity = m_world->Resolve(m_contextMenuTarget);

        // Only show menu options if entity is valid AND not protected
        if (!entity.IsNull() && m_world->IsAlive(entity) && !IsProtectedEntity(m_contextMenuTarget)) {
            if (ImGui::Selectable("Add Child")) {
                _addChildEntity(m_contextMenuTarget);
            }
            if (ImGui::Selectable("Clone")) {
                _cloneEntity(m_contextMenuTarget);
            }
            if (ImGui::Selectable("Rename")) {
                _startRename(m_contextMenuTarget);
            }
            ImGui::Separator();
            if (ImGui::Selectable("Delete")) {
                _deleteEntity(m_contextMenuTarget);
            }
        }
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------------------
// Entity Operations
// -------------------------------------------------------------------------

// Delete an entity and update selection
// Also handles cleanup of selection state
void HierarchyPanel::_deleteEntity(EntityId entityId) {
    // Block deletion of protected entities
    if (IsProtectedEntity(entityId)) return;

    if (m_entityActions) {
        m_entityActions->RemoveEntity(entityId);
    }

    // Clear selection if deleted entity was selected
    if (m_selectedEntityId == entityId) {
        m_selectedEntityId = 0;
        if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
    }
    m_contextMenuTarget = 0;
}

// Clone an entity: creates a duplicate with same components and hierarchy
void HierarchyPanel::_cloneEntity(EntityId entityId) {
    // Block cloning protected entities
    if (IsProtectedEntity(entityId)) return;

    if (m_entityActions) {
        m_entityActions->CloneEntity(entityId);
    }
}

// Add a child entity to the selected entity
// Creates a new entity as child of the specified parent
void HierarchyPanel::_addChildEntity(EntityId parentId) {
    // Block adding children to protected entities
    if (IsProtectedEntity(parentId)) return;

    if (m_entityActions) {
        m_entityActions->AddEntity("Entity", parentId);
    }
}

// Add a new root entity (entity without parent)
void HierarchyPanel::_addRootEntity() {
    if (m_entityActions) {
        m_entityActions->AddEntity("Entity", ECS::Entity::NPOS32);
    }
}

// Start renaming an entity (prepare rename state and buffers)
void HierarchyPanel::_startRename(EntityId entityId) {
    // Block renaming protected entities
    if (IsProtectedEntity(entityId)) return;

    // Start renaming this entity
    m_renamingEntityId = entityId;
    
    // Copy current name to rename buffer
    ECS::Entity entity = m_world->Resolve(entityId);
    if (m_world->Has<ECS::Components::Name>(entity)) {
        const auto& nameComp = m_world->Get<ECS::Components::Name>(entity);
        strncpy_s(m_renameBuffer, nameComp.Value, sizeof(m_renameBuffer) - 1);
        m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
    }
    else {
        strncpy_s(m_renameBuffer, "Entity", sizeof(m_renameBuffer) - 1);
    }
    
    m_focusRenameInput = true;
}

// -------------------------------------------------------------------------
// Prefab Operations
// -------------------------------------------------------------------------

// Instantiate a prefab as a child of the specified parent
// Handles JSON deserialization and component setup
// Returns the entity ID of the newly created instance
EntityId HierarchyPanel::_instantiatePrefabAsChild(const std::string& prefabPath, EntityId parentId) {
    if (!m_world) return ECS::Entity::NPOS32;

    // Block adding children to protected entities
    if (parentId != ECS::Entity::NPOS32 && IsProtectedEntity(parentId)) {
        return ECS::Entity::NPOS32;
    }

    try {
        // Open and read prefab file
        std::ifstream file(prefabPath);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open prefab: " << prefabPath);
            return ECS::Entity::NPOS32;
        }

        // Parse JSON data from prefab file
        nlohmann::json prefabJson;
        file >> prefabJson;
        file.close();

        // Validate prefab structure
        if (!prefabJson.contains("Components") || !prefabJson["Components"].is_array()) {
            LOG_ERROR("Invalid prefab format: missing Components array");
            return ECS::Entity::NPOS32;
        }

        // Create new entity
        ECS::Entity entity = m_world->Create();

        // Set entity name from prefab filename (Unity-like behavior)
        std::filesystem::path p(prefabPath);
        std::string prefabName = p.stem().string();

        // Create Name component and copy prefab name into it
        ECS::Components::Name nameComp;
        strncpy_s(nameComp.Value, prefabName.c_str(), sizeof(nameComp.Value) - 1);
        nameComp.Value[sizeof(nameComp.Value) - 1] = '\0'; // Ensure null termination
        m_world->Set<ECS::Components::Name>(entity, nameComp);

        // Apply all components from prefab data using ComponentRegistryUI
        // This automatically handles all component types without repetitive code
        for (const auto& componentEntry : prefabJson["Components"]) {
            // Validate component entry structure
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
            if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

            std::string typeName = componentEntry["TypeName"];
            const auto* meta = ComponentRegistryUI::Find(typeName);

            // Use registry's AddComponent function to create and deserialize the component
            if (meta) {
                meta->AddComponent(m_world, entity, componentEntry["Data"]);
            }
        }

        // Set parent relationship if not creating as root
        // NPOS32 is a special value meaning "no parent" (root entity)
        if (parentId != ECS::Entity::NPOS32) {
            // Convert EntityId to ECS::Entity object for world operations
            ECS::Entity parent = m_world->Resolve(parentId);
            // Only set parent if the parent entity exists and is valid
            if (!parent.IsNull() && m_world->IsAlive(parent)) {
                m_world->Set<ECS::Parent>(entity, ECS::Parent{ parent });
            }
        }

        // Add prefab link component to track prefab relationship
        // This component connects the instance back to the prefab template file
        std::string linkPath = p.lexically_normal().string();
        m_world->Set<ECS::Components::PrefabLink>(entity, ECS::Components::PrefabLink(linkPath));

        LOG_INFO("Instantiated prefab: " << prefabName);

        // Return the entity ID so caller can select it
        return entity.Index;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to instantiate prefab: " << e.what());
        return ECS::Entity::NPOS32;
    }
}

// -------------------------------------------------------------------------
// Helper Methods
// -------------------------------------------------------------------------

// Get all root entities (entities without parents)
// Iterates through all entities and collects those without Parent component
std::vector<EntityId> HierarchyPanel::_getRootEntities() const {
    std::vector<EntityId> roots;
    if (!m_world) return roots;

    m_world->Each([&](ECS::Entity e) {
        // Skip hidden/protected entities (like EditorCamera at ID 0)
        if (ShouldHideFromHierarchy(m_world, e)) return;

        if (!m_world->Has<ECS::Parent>(e)) {
            roots.push_back(e.Index);
        }
        });

    return roots;
}

// Get all children of an entity
// Searches for entities that have this entity as their parent
std::vector<EntityId> HierarchyPanel::_getChildren(EntityId parentId) const {
    std::vector<EntityId> children;
    if (!m_world) return children;

    // Iterate through all entities that have a Parent component
    // The Each<> template function only processes entities with the specified component
    m_world->Each<ECS::Parent>([&](ECS::Entity e, const ECS::Parent& parent) {
        // Skip hidden children
        if (ShouldHideFromHierarchy(m_world, e)) return;

        if (parent.ParentEntity.Index == parentId) {
            children.push_back(e.Index);
        }
        });

    return children;
}

// Check if entity matches current search filter
bool HierarchyPanel::_matchesSearchFilter(EntityId entityId) const {
    // Empty filter matches everything
    if (m_searchFilter.empty()) return true;

    // Get entity and check name
    ECS::Entity entity = m_world->Resolve(entityId);
    if (entity.IsNull() || !m_world->IsAlive(entity)) return false;

    if (m_world->Has<ECS::Components::Name>(entity)) {
        const auto& nameComp = m_world->Get<ECS::Components::Name>(entity);
        std::string entityName = nameComp.Value;
        
        // Convert both to lowercase for case-insensitive search
        std::string lowerName = entityName;
        std::string lowerFilter = m_searchFilter;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
        
        // Check if entity name contains the filter string
        return lowerName.find(lowerFilter) != std::string::npos;
    }

    // No name component, check if searching for "Entity"
    std::string defaultName = "Entity";
    std::string lowerFilter = m_searchFilter;
    std::transform(defaultName.begin(), defaultName.end(), defaultName.begin(), ::tolower);
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
    return defaultName.find(lowerFilter) != std::string::npos;
}
