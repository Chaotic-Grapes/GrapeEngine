/* Start Header *****************************************************************/
/*!
\file   HierarchyPanel.cpp
\author Foo Rui Qin    (45%)
        Samantha Leong (45%)
        Muhammad Nur Fadzly Bin Zulkifli (10%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   5th November 2025
\brief
Implements the Hierarchy panel that shows all scene entities as a tree.

The hierarchy gives a structured view of the active scene and is used to select
entities, organize them through parent-child relationships and perform common
editor actions like create, delete, clone and reparent through drag-drop. It also
syncs with the inspector and viewport so selection stays consistent across the UI
and supports prefab instantiation by accepting dragged prefab assets.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#define NOMINMAX
#define NO_ERROR
#include <windows.h>
#include "HierarchyPanel.h"
#include "ComponentWidgets.h"
#include "EditorComponentRegistry.h"
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
#include "BaseViewport.h"

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

        // Hide entities named "EditorCamera" (editor system entity)
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
    bool IsProtectedEntity(ECS::World* world, EntityId entityId) {
        if (!world)
            return false;

        // Resolve the entity from its ID
        ECS::Entity entity = world->Resolve(entityId);

        // Not alive, cannot be protected
        if (!world->IsAlive(entity))
            return false;

        // Protect editor cameras from modification
        if (world->Has<ECS::Components::CameraEditor3D>(entity))
            return true;

        return false;
    }
}

/**
 * @brief Opens a platform-specific file dialog to select a C# script and attaches it to an entity.
 *
 * This function is responsible for:
 *  - Opening a Windows file dialog (Win32 API) to let the user choose a `.cs` script file.
 *  - Extracting the script class name using the file stem.
 *  - Parsing the namespace declaration (if present) from the script file.
 *  - Constructing the full type name (`Namespace.ClassName`) used by the scripting backend.
 *  - Converting the file path to a project-relative path when possible.
 *  - Attaching the ScriptInstance component with the resolved type name and path.
 *  - Updating HierarchyPanel's entity selection state and invoking selection callbacks.
 *
 * On non-Windows platforms, the function simply logs a warning because no file dialog
 * implementation exists for Linux/macOS.
 *
 * @param entityId The target entity to attach the script to. If `ECS::Entity::NPOS32`,
 *                 the function exits immediately.
 */
void HierarchyPanel::_importAndAttachScript(EntityId entityId) {
    if (entityId == ECS::Entity::NPOS32) return;

    // 1. Call your external file dialog utility.
#ifdef _WIN32

    char filename[512] = "";

    // Win32 OPENFILENAMEA struct setup
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);

    //Set the filter ONLY for C# Script files (*.cs)
    // Format: "Description\0Pattern\0"
    ofn.lpstrFilter = "C# Script Files\0*.cs\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select Script File to Attach"; // Custom title

    // Flags remain the same to ensure the file exists and paths are valid
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Show file dialog: execution pauses here until the user selects or cancels
    if (GetOpenFileNameA(&ofn)) {
        // User selected a file; the path is in the 'filename' buffer
        std::string selectedFilePath(filename);

        // 1. Extract the class name and namespace from the C# file
        std::filesystem::path p(selectedFilePath);
        std::string scriptClassName = p.stem().string();
        std::string fullTypeName = scriptClassName; // Default to just class name

        // Try to parse namespace from the file
        std::ifstream fileStream(selectedFilePath);
        if (fileStream.is_open()) {
            std::string line;
            std::string namespaceStr;
            
            // Look for "namespace" declaration in the file
            while (std::getline(fileStream, line)) {
                // Trim whitespace
                size_t start = line.find_first_not_of(" \t\r\n");
                if (start == std::string::npos) continue;
                
                // Check if line starts with "namespace"
                if (line.substr(start, 9) == "namespace") {
                    // Extract namespace (handle both "namespace X;" and "namespace X {")
                    size_t nsStart = start + 9;
                    size_t nsEnd = line.find_first_of(";{", nsStart);
                    
                    if (nsEnd != std::string::npos) {
                        namespaceStr = line.substr(nsStart, nsEnd - nsStart);
                        
                        // Trim whitespace from namespace
                        size_t nsFirst = namespaceStr.find_first_not_of(" \t\r\n");
                        size_t nsLast = namespaceStr.find_last_not_of(" \t\r\n");
                        if (nsFirst != std::string::npos && nsLast != std::string::npos) {
                            namespaceStr = namespaceStr.substr(nsFirst, nsLast - nsFirst + 1);
                            fullTypeName = namespaceStr + "." + scriptClassName;
                        }
                        break;
                    }
                }
            }
            fileStream.close();
        }

        // 2. Convert to relative path if within project
        std::string relativePath = selectedFilePath;
        try {
            std::filesystem::path absPath = std::filesystem::absolute(selectedFilePath);
            std::filesystem::path currentPath = std::filesystem::current_path();
            relativePath = std::filesystem::relative(absPath, currentPath).string();
        }
        catch (const std::exception&) {
            // If relative path conversion fails, use the original path
            relativePath = selectedFilePath;
        }
        
        // 3. Attach the ScriptInstance component with full type name and path
        _attachScriptComponent(entityId, fullTypeName, relativePath);

        // 4. Update selection state (using the correct member variable name)
        m_selectedEntityIds.clear();
        m_selectedEntityIds.insert(entityId);
        m_anchorEntityId = entityId;
        if (m_selectionCallback) m_selectionCallback(entityId);
    }
    else {
        // User clicked cancel
        // Optional: LOG_INFO("Script selection cancelled by user");
    }

#else // Not Windows (Linux, Mac, etc.)
    // Matches the AssetLibrary's error handling for other platforms
    LOG_WARNING("File dialog not implemented for this platform");
#endif
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
    m_selectedEntityIds.clear();
    m_anchorEntityId = ECS::Entity::NPOS32;
    // Clear expanded nodes so tree redraws cleanly
    m_expandedNodes.clear();
}

// Register a callback for selection change events
// This allows other systems to react when entity selection changes
void HierarchyPanel::OnSelectionChanged(SelectionCallback callback) {
    m_selectionCallback = callback;
}

// Set the selected entity (clears previous selection)
void HierarchyPanel::SetSelectedEntity(EntityId id) {
    m_selectedEntityIds.clear();
    if (id != ECS::Entity::NPOS32) {
        m_selectedEntityIds.insert(id);
        m_anchorEntityId = id;
    }
    else {
        m_anchorEntityId = ECS::Entity::NPOS32;
    }
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
        LOG_WARNING("[HierarchyPanel] Render called but no world attached");
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

    // Process deferred deletions AFTER tree rendering is complete
    // This prevents crashes from modifying the hierarchy while iterating it
    if (!m_deferredDeletions.empty()) {
        for (EntityId id : m_deferredDeletions) {
            if (!IsProtectedEntity(m_world, id)) {
                // Collect all entities that will be deleted (parent + all children recursively)
                std::vector<EntityId> allDeletedIds;
                std::function<void(EntityId)> collectRecursive = [&](EntityId deleteId) {
                    allDeletedIds.push_back(deleteId);

                    // Recursively collect all children
                    auto children = _getChildren(deleteId);
                    for (EntityId childId : children) {
                        collectRecursive(childId);
                    }
                    };
                collectRecursive(id);

                // Perform the deletion (this destroys parent + all children)
                if (m_entityActions) {
                    m_entityActions->RemoveEntity(id);
                }

                // Remove ALL deleted entities from selection
                for (EntityId deletedId : allDeletedIds) {
                    m_selectedEntityIds.erase(deletedId);
                }

                // Update anchor if it was one of the deleted entities
                if (std::find(allDeletedIds.begin(), allDeletedIds.end(), m_anchorEntityId) != allDeletedIds.end()) {
                    m_anchorEntityId = m_selectedEntityIds.empty() ? ECS::Entity::NPOS32 : *m_selectedEntityIds.begin();
                }

                // Notify callback if selection changed
                if (m_selectedEntityIds.empty()) {
                    if (m_selectionCallback) m_selectionCallback(ECS::Entity::NPOS32);
                }
            }
        }
        m_deferredDeletions.clear();
        m_contextMenuTarget = ECS::Entity::NPOS32;
    }

    // Handle delete key for selected entities: global keyboard shortcut
    if (Input::IsKeyDown(KEY_DELETE) && !m_selectedEntityIds.empty()) {
        // Delete all selected entities (except protected ones)
        std::vector<EntityId> toDelete;
        for (EntityId id : m_selectedEntityIds) {
            if (!IsProtectedEntity(m_world, id)) {
                toDelete.push_back(id);
            }
        }
        for (EntityId id : toDelete) {
            _deleteEntity(id);
        }
    }

    // Handle F2 key for renaming selected entity (only if single selection)
    if (ImGui::IsKeyPressed(ImGuiKey_F2) && m_selectedEntityIds.size() == 1) {
        EntityId selected = *m_selectedEntityIds.begin();
        if (!IsProtectedEntity(m_world, selected)) {
            _startRename(selected);
        }
    }

    // Click empty space to clear selection
    // Only clears if clicking on window background, not on any items
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)
        && !ImGui::IsAnyItemHovered())
    {
        m_selectedEntityIds.clear();
        m_anchorEntityId = ECS::Entity::NPOS32;
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
        m_selectedEntityIds.clear();
        m_anchorEntityId = ECS::Entity::NPOS32;
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
        m_selectedEntityIds.clear();
        m_anchorEntityId = ECS::Entity::NPOS32;
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

    // Check if this is a prefab instance FIRST (needed for color, both parent and child)
    bool isPrefabInstance = m_world->Has<ECS::Components::PrefabLink>(entity);
    if (m_world->Has<ECS::Components::PrefabLink>(entity)) {
        isPrefabInstance = true;
    }
    else {
        // Check if any parent has PrefabLink
        ECS::Entity parent = m_world->ParentOf(entity);
        while (!parent.IsNull()) {
            if (m_world->Has<ECS::Components::PrefabLink>(parent)) {
                isPrefabInstance = true;
                break;
            }
            parent = m_world->ParentOf(parent);
        }
    }

    // Check if this entity has a script attached
    bool hasScript = false;
    if (m_world->Has<ECS::Components::ScriptInstance>(entity)) {
        const auto& scriptComp = m_world->Get<ECS::Components::ScriptInstance>(entity);
        if (strlen(scriptComp.TypeName) > 0) {
            hasScript = true;
        }
    }

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
    if (isPrefabInstance && m_world->Has<ECS::Components::PrefabLink>(entity)) {
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
    if (m_selectedEntityIds.find(entityId) != m_selectedEntityIds.end()) nodeFlags |= ImGuiTreeNodeFlags_Selected;

    // Manage expanded state: open if previously expanded
    bool isExpanded = m_expandedNodes.find(entityId) != m_expandedNodes.end();
    if (isExpanded && hasChildren) ImGui::SetNextItemOpen(true);

    // Push ID to ensure unique imgui identifiers
    ImGui::PushID(static_cast<int>(entityId));

    // Push blue color for prefab instances
    if (isPrefabInstance) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f)); // Light blue
    }

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
            m_renamingEntityId = ECS::Entity::NPOS32;
        }

        // Cancel rename on Escape or click away
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered())) {
            m_renamingEntityId = ECS::Entity::NPOS32;
        }
    }
    else {
        // Calculate icon sizes to reserve space on the right BEFORE creating the tree node.
        const float iconPadding = 6.0f; // space between icons and edge
        float prefabIconWidth = 0.0f;
        float scriptIconWidth = 0.0f;
        const char* prefabIcon = "\xEE\xA6\xA4";
        const char* scriptIcon = "\xEE\xA1\xAF";

        if (m_symbolsFont && isPrefabInstance) {
            ImGui::PushFont(m_symbolsFont);
            prefabIconWidth = ImGui::CalcTextSize(prefabIcon).x;
            ImGui::PopFont();
        }
        if (m_symbolsFont && hasScript) {
            ImGui::PushFont(m_symbolsFont);
            scriptIconWidth = ImGui::CalcTextSize(scriptIcon).x;
            ImGui::PopFont();
        }

        float iconsTotalWidth = 0.0f;
        if (prefabIconWidth > 0.0f) iconsTotalWidth += prefabIconWidth + iconPadding;
        if (scriptIconWidth > 0.0f) iconsTotalWidth += scriptIconWidth + iconPadding;

        // Compute available width for label using content region (safer than querying item rect)
        ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
        float cursorX = ImGui::GetCursorPosX();
        // Add a safety reserve to account for frame padding and item spacing so the
        // ellipsis or text never overlaps the icons even with font rounding.
        const ImGuiStyle& style = ImGui::GetStyle();
        float reservedWidth = style.ItemSpacing.x * 2.0f + style.FramePadding.x * 2.0f + 8.0f;
        float maxLabelWidth = contentMax.x - cursorX - iconsTotalWidth - reservedWidth;
        if (maxLabelWidth < 0.0f) maxLabelWidth = 0.0f;

        // Truncate label with ellipsis to fit into available width
        std::string displayLabel = label;
        if (m_mainFont) ImGui::PushFont(m_mainFont);
        ImVec2 fullSize = ImGui::CalcTextSize(displayLabel.c_str());
        if (fullSize.x > maxLabelWidth) {
            std::string ellipsis = "...";
            while (!displayLabel.empty()) {
                displayLabel.pop_back();
                std::string test = displayLabel + ellipsis;
                ImVec2 testSize = ImGui::CalcTextSize(test.c_str());
                if (testSize.x <= maxLabelWidth) {
                    displayLabel = test;
                    break;
                }
            }
            if (displayLabel.empty()) displayLabel = ellipsis;
        }
        if (m_mainFont) ImGui::PopFont();

        // Now create the tree node with the truncated label (safer, preserves ImGui internal state)
        nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entityId, nodeFlags, "%s", displayLabel.c_str());

        // After creating the node, get item rect to position icons correctly
        ImVec2 itemRectMin = ImGui::GetItemRectMin();
        ImVec2 itemRectMax = ImGui::GetItemRectMax();

        // Draw icons on the right, script icon at the far right, prefab just left of it
        float drawX = itemRectMax.x - iconPadding;
        float itemCenterY = itemRectMin.y + (itemRectMax.y - itemRectMin.y) * 0.5f;

        if (hasScript && m_symbolsFont) {
            ImGui::PushFont(m_symbolsFont);
            ImVec2 iconSize = ImGui::CalcTextSize(scriptIcon);
            ImVec2 iconPos = ImVec2(drawX - iconSize.x, itemCenterY - iconSize.y * 0.5f);
            ImGui::GetWindowDrawList()->AddText(m_symbolsFont, 26.f, iconPos,
                ImGui::GetColorU32(ImVec4(0.7f, 0.8f, 0.9f, 0.9f)), scriptIcon);
            ImGui::PopFont();
            drawX -= (iconSize.x + iconPadding);
        }

        if (isPrefabInstance && m_symbolsFont) {
            ImGui::PushFont(m_symbolsFont);
            ImVec2 iconSize = ImGui::CalcTextSize(prefabIcon);
            ImVec2 iconPos = ImVec2(drawX - iconSize.x, itemCenterY - iconSize.y * 0.5f);
            ImGui::GetWindowDrawList()->AddText(m_symbolsFont, 26.0f, iconPos,
                ImGui::GetColorU32(ImVec4(0.4f, 0.7f, 1.0f, 1.0f)), prefabIcon);
            ImGui::PopFont();
            drawX -= (iconSize.x + iconPadding);
        }

        // Handle interactions like clicks and drag-drop
        _handleNodeInteraction(entityId);
        _handleNodeDragDrop(entityId);
    }

    // Pop blue color if it was a prefab instance
    if (isPrefabInstance) {
        ImGui::PopStyleColor();
    }

    // Render context menu if opened
    _renderEntityContextMenu();

    // If node is open, render children recursively
    if (nodeOpen) {
        // Track that this node is expanded
        m_expandedNodes.insert(entityId);

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
    if (IsProtectedEntity(m_world, entityId)) return;

    // Skip interaction if currently renaming
    if (m_renamingEntityId != ECS::Entity::NPOS32) return;

    // Get current time for click timing
    float currentTime = static_cast<float>(ImGui::GetTime());

    // Check modifier keys
    bool ctrlPressed = ImGui::GetIO().KeyCtrl;
    bool shiftPressed = ImGui::GetIO().KeyShift;

    // Right-click context menu: select and open context menu
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        // If right-clicked entity is not in selection, make it the only selection
        if (m_selectedEntityIds.find(entityId) == m_selectedEntityIds.end()) {
            m_selectedEntityIds.clear();
            m_selectedEntityIds.insert(entityId);
            m_anchorEntityId = entityId;
        }
        // Trigger callback with first selected entity
        if (m_selectionCallback) m_selectionCallback(entityId);
        m_contextMenuTarget = entityId;
        ImGui::OpenPopup("EntityContextMenu");
    }

    // Fast double-click to focus camera
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (m_viewport) { 
            // Focus on entity
            m_viewport->FocusOnEntity(entityId);  
        }
        // Update click tracking
        m_lastClickedEntity = entityId;
        m_lastClickTime = currentTime;
    }
    // Handle single clicks (for selection and rename detection)
    else if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        // Check if this is a slow second click on the same already-selected entity BEFORE updating times
        // Only trigger rename if second click is within a reasonable time window (0.3s - 1.0s)
        bool isAlreadySelected = (m_selectedEntityIds.find(entityId) != m_selectedEntityIds.end());
        bool isSlowSecondClick = (m_lastClickedEntity == entityId &&
            isAlreadySelected &&
            m_selectedEntityIds.size() == 1 &&
            (currentTime - m_lastClickTime) > RENAME_DELAY_THRESHOLD &&
            (currentTime - m_lastClickTime) < 1.0f);

        if (isSlowSecondClick && !ctrlPressed && !shiftPressed) {
            // Start rename mode (only for single selection, no modifiers)
            _startRename(entityId);
        }
        else if (shiftPressed && m_anchorEntityId != ECS::Entity::NPOS32) {
            // Shift+Click: Range selection from anchor to clicked entity
            m_selectedEntityIds.clear();

            // Get all entities in flat list (hierarchy order)
            std::vector<EntityId> allEntities;
            std::function<void(EntityId)> collectEntities = [&](EntityId id) {
                allEntities.push_back(id);
                for (EntityId childId : _getChildren(id)) {
                    collectEntities(childId);
                }
                };
            for (EntityId rootId : _getRootEntities()) {
                collectEntities(rootId);
            }

            // Find indices of anchor and clicked entity
            auto anchorIt = std::find(allEntities.begin(), allEntities.end(), m_anchorEntityId);
            auto clickedIt = std::find(allEntities.begin(), allEntities.end(), entityId);

            if (anchorIt != allEntities.end() && clickedIt != allEntities.end()) {
                // Select range between anchor and clicked (inclusive)
                auto startIt = (anchorIt < clickedIt) ? anchorIt : clickedIt;
                auto endIt = (anchorIt < clickedIt) ? clickedIt : anchorIt;

                for (auto it = startIt; it <= endIt; ++it) {
                    m_selectedEntityIds.insert(*it);
                }
            }

            if (m_selectionCallback) m_selectionCallback(entityId);
        }
        else if (ctrlPressed) {
            // Ctrl+Click: Toggle selection
            if (isAlreadySelected) {
                m_selectedEntityIds.erase(entityId);
                // If we removed the anchor, set new anchor to first remaining selection
                if (m_anchorEntityId == entityId) {
                    m_anchorEntityId = m_selectedEntityIds.empty() ? ECS::Entity::NPOS32 : *m_selectedEntityIds.begin();
                }
            }
            else {
                m_selectedEntityIds.insert(entityId);
                m_anchorEntityId = entityId;
            }
            if (m_selectionCallback) m_selectionCallback(entityId);
        }
        else {
            // Normal click: handle based on whether entity is already selected
            if (isAlreadySelected && m_selectedEntityIds.size() > 1) {
                // Clicking on an already-selected entity in a multi-selection
                // Don't clear selection yet - could be starting a drag
                // Selection will only change if we detect it's not a drag
                // (This preserves multi-selection for drag-and-drop)
            }
            else {
                // Normal single selection
                m_selectedEntityIds.clear();
                m_selectedEntityIds.insert(entityId);
                m_anchorEntityId = entityId;
                if (m_selectionCallback) m_selectionCallback(entityId);
            }
        }

        // Update click tracking for next time
        m_lastClickedEntity = entityId;
        m_lastClickTime = currentTime;
    }
}

// Handle drag-drop for entity reparenting and prefab instantiation
void HierarchyPanel::_handleNodeDragDrop(EntityId entityId) {
    // Block dragging protected entities
    if (IsProtectedEntity(m_world, entityId)) return;

    // Drag source: make this entity draggable
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        // If dragging a selected entity, drag all selected entities (multi-select support)
        bool isDraggingSelection = m_selectedEntityIds.find(entityId) != m_selectedEntityIds.end() &&
            m_selectedEntityIds.size() > 1;

        if (isDraggingSelection) {
            // Drag all selected entities as a vector
            std::vector<EntityId> selectedVec(m_selectedEntityIds.begin(), m_selectedEntityIds.end());
            ImGui::SetDragDropPayload("ENTITY_IDS", selectedVec.data(), selectedVec.size() * sizeof(EntityId));

            // Show count in drag preview
            ImGui::Text("(%zu entities)", selectedVec.size());
        }
        else {
            // Single entity drag
            ImGui::SetDragDropPayload("ENTITY_ID", &entityId, sizeof(EntityId));

            // Show entity name as drag preview
            ECS::Entity entity = m_world->Resolve(entityId);
            if (m_world->IsAlive(entity) && m_world->Has<ECS::Components::Name>(entity)) {
                const auto& name = m_world->Get<ECS::Components::Name>(entity);
                ImGui::Text("%s", name.Value);
            }
        }
        ImGui::EndDragDropSource();
    }

    // Drop target: accept entities and prefabs dropped on this node
    if (ImGui::BeginDragDropTarget()) {
        // Handle single entity reparenting
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;

            // Block reparenting if either entity is protected
            if (!IsProtectedEntity(m_world, draggedId) && !IsProtectedEntity(m_world, entityId)) {
                if (m_entityActions) {
                    m_entityActions->ReparentEntity(draggedId, entityId);
                }
            }
        }

        // Handle multiple entities reparenting
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_IDS")) {
            size_t count = payload->DataSize / sizeof(EntityId);
            const EntityId* draggedIds = static_cast<const EntityId*>(payload->Data);

            // Reparent all dragged entities
            for (size_t i = 0; i < count; ++i) {
                EntityId draggedId = draggedIds[i];
                // Block reparenting if either entity is protected
                if (!IsProtectedEntity(m_world, draggedId) && !IsProtectedEntity(m_world, entityId)) {
                    if (m_entityActions) {
                        m_entityActions->ReparentEntity(draggedId, entityId);
                    }
                }
            }
        }

        // Handle prefab instantiation as child
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                EntityId newEntityId = _instantiatePrefabAsChild(droppedPath, entityId);
                if (newEntityId != ECS::Entity::NPOS32) {
                    m_selectedEntityIds.clear();
                    m_selectedEntityIds.insert(newEntityId);
                    m_anchorEntityId = newEntityId;
                    if (m_selectionCallback) m_selectionCallback(newEntityId);
                }
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
            const char* data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;
            while (data < end) {
                std::string path(data);
                data += path.size() + 1;
                if (path.empty()) continue;
                if (std::filesystem::path(path).extension() != ".prefab") continue;
                EntityId newEntityId = _instantiatePrefabAsChild(path, entityId);
                if (newEntityId != ECS::Entity::NPOS32) {
                    m_selectedEntityIds.clear();
                    m_selectedEntityIds.insert(newEntityId);
                    m_anchorEntityId = newEntityId;
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
        // Reparent single entity to root (make it have no parent)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ID")) {
            EntityId draggedId = *(EntityId*)payload->Data;

            // Block reparenting protected entities
            if (!IsProtectedEntity(m_world, draggedId)) {
                if (m_entityActions) {
                    m_entityActions->ReparentEntity(draggedId, ECS::Entity::NPOS32);
                }
            }
        }

        // Reparent multiple entities to root
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_IDS")) {
            size_t count = payload->DataSize / sizeof(EntityId);
            const EntityId* draggedIds = static_cast<const EntityId*>(payload->Data);

            // Reparent all dragged entities to root
            for (size_t i = 0; i < count; ++i) {
                EntityId draggedId = draggedIds[i];
                if (!IsProtectedEntity(m_world, draggedId)) {
                    if (m_entityActions) {
                        m_entityActions->ReparentEntity(draggedId, ECS::Entity::NPOS32);
                    }
                }
            }
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string droppedPath = std::string(static_cast<const char*>(payload->Data));
            if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                EntityId newEntityId = _instantiatePrefabAsChild(droppedPath, ECS::Entity::NPOS32);
                if (newEntityId != ECS::Entity::NPOS32) {
                    m_selectedEntityIds.clear();
                    m_selectedEntityIds.insert(newEntityId);
                    m_anchorEntityId = newEntityId;
                    if (m_selectionCallback) m_selectionCallback(newEntityId);
                }
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
            const char* data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;
            while (data < end) {
                std::string path(data);
                data += path.size() + 1;
                if (path.empty()) continue;
                if (std::filesystem::path(path).extension() != ".prefab") continue;
                EntityId newEntityId = _instantiatePrefabAsChild(path, ECS::Entity::NPOS32);
                if (newEntityId != ECS::Entity::NPOS32) {
                    m_selectedEntityIds.clear();
                    m_selectedEntityIds.insert(newEntityId);
                    m_anchorEntityId = newEntityId;
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
        size_t selectionCount = m_selectedEntityIds.size();

        // Only show menu options if entity is valid AND not protected
        if (!entity.IsNull() && m_world->IsAlive(entity) && !IsProtectedEntity(m_world, m_contextMenuTarget)) {
            // Add Child only available for single selection
            if (selectionCount == 1) {
                if (ImGui::Selectable("Add Child")) {
                    _addChildEntity(m_contextMenuTarget);
                }
            }

            // Clone works with multiple selections
            std::string cloneLabel = (selectionCount > 1) ? "Clone (" + std::to_string(selectionCount) + ")" : "Clone";
            if (ImGui::Selectable(cloneLabel.c_str())) {
                // Clone all selected entities
                for (EntityId id : m_selectedEntityIds) {
                    if (!IsProtectedEntity(m_world, id)) {
                        _cloneEntity(id);
                    }
                }
            }

            // Rename only available for single selection
            if (selectionCount == 1) {
                if (ImGui::Selectable("Rename")) {
                    _startRename(m_contextMenuTarget);
                }
            }

            // Detach Prefab: only for prefab instances (single selection)
            if (selectionCount == 1 && m_world->Has<ECS::Components::PrefabLink>(entity)) {
                if (ImGui::Selectable("Detach Prefab")) {
                    m_world->Remove<ECS::Components::PrefabLink>(entity);
                    LOG_INFO("Detached prefab link from entity");
                }
            }

            // Delete works with multiple selections
            std::string deleteLabel = (selectionCount > 1) ? "Delete (" + std::to_string(selectionCount) + ")" : "Delete";
            if (ImGui::Selectable(deleteLabel.c_str())) {
                // Delete all selected entities
                std::vector<EntityId> toDelete(m_selectedEntityIds.begin(), m_selectedEntityIds.end());
                for (EntityId id : toDelete) {
                    if (!IsProtectedEntity(m_world, id)) {
                        _deleteEntity(id);
                    }
                }
            }
            ImGui::Separator();
            // Script attachment/detachment - only for single selection
            if (selectionCount == 1) {
                ECS::Entity targetEntity = m_world->Resolve(m_contextMenuTarget);
                bool hasScriptComponent = m_world->Has<ECS::Components::ScriptInstance>(targetEntity);
                if (!hasScriptComponent) {
                    if (ImGui::Selectable("Attach Script")) {
                        _importAndAttachScript(m_contextMenuTarget);
                    }
                }
                else {
                    if (ImGui::Selectable("Detach Script")) {
                        m_world->Remove<ECS::Components::ScriptInstance>(targetEntity);
                    }
                }
            }
            if (ImGui::BeginMenu("Add Component")) {
                // You can add other component types (Renderer, Rigidbody, etc.) here later
                ECS::Entity targetEntity = m_world->Resolve(m_contextMenuTarget);
                const auto& registry = ComponentRegistryUI::GetAll();
                for (const auto& meta : registry) {
                    bool hasComponent = meta.HasComponent(m_world, targetEntity);
                    if (hasComponent) ImGui::BeginDisabled();
                    if (ImGui::MenuItem(meta.DisplayName.c_str())) {
                        meta.AddComponent(m_world, targetEntity, meta.GetDefaults());
                    }
                    if (hasComponent) ImGui::EndDisabled();
                }
                ImGui::EndMenu(); // End Add Component menu
            }
        }
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------------------
// Entity Operations
// -------------------------------------------------------------------------

// Delete an entity - defers actual deletion until after tree rendering
// This prevents crashes from modifying hierarchy while iterating it
void HierarchyPanel::_deleteEntity(EntityId entityId) {
    // Add to deferred deletion queue
    m_deferredDeletions.push_back(entityId);
}

// Clone an entity: creates a duplicate with same components and hierarchy
void HierarchyPanel::_cloneEntity(EntityId entityId) {
    // Block cloning protected entities
    if (IsProtectedEntity(m_world, entityId)) return;

    if (m_entityActions) {
        m_entityActions->CloneEntity(entityId);
    }
}

// Add a child entity to the selected entity
// Creates a new entity as child of the specified parent
void HierarchyPanel::_addChildEntity(EntityId parentId) {
    // Block adding children to protected entities
    if (IsProtectedEntity(m_world, parentId)) return;

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
    if (IsProtectedEntity(m_world, entityId)) return;

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

/**
 * @brief Ensures an entity has a ScriptInstance component, and sets its TypeName and ScriptPath.
 * @param entityId The entity to modify.
 * @param scriptName The fully qualified C# type name including namespace (e.g., "MyGame.PlayerController").
 * @param scriptPath The relative path to the script file (e.g., "Assets/Scripts/PlayerController.cs").
 */
void HierarchyPanel::_attachScriptComponent(EntityId entityId, const std::string& scriptName, const std::string& scriptPath) {
    if (entityId == ECS::Entity::NPOS32 || !m_world) return;

    ECS::Entity e = m_world->Resolve(entityId);
    if (e.IsNull() || !m_world->IsAlive(e)) return;

    // 1. Ensure the entity has an Active component (required by ScriptSystem)
    if (!m_world->Has<ECS::Components::Active>(e)) {
        ECS::Components::Active activeComp;
        activeComp.Enabled = true;
        m_world->Add<ECS::Components::Active>(e, activeComp);
    }

    // 2. Ensure the entity has a LocalTransform component (required by most scripts)
    if (!m_world->Has<ECS::Components::LocalTransform>(e)) {
        ECS::Components::LocalTransform transform;
        transform.Position = Vector3D{0, 0, 0};
        transform.Rotation = Quaternion{0, 0, 0, 1};
        transform.Scale = Vector3D{1, 1, 1};
        m_world->Add<ECS::Components::LocalTransform>(e, transform);
    }

    // 3. Ensure the ScriptInstance component exists (Add it if it doesn't)
    if (!m_world->Has<ECS::Components::ScriptInstance>(e)) {
        ECS::Components::ScriptInstance newScript;
        m_world->Add<ECS::Components::ScriptInstance>(e, newScript);
    }

    // 3. Update the component's TypeName and ScriptPath
    auto& scriptComp = m_world->Get<ECS::Components::ScriptInstance>(e);

    // Set the fully qualified type name (e.g., "MyGame.PlayerController")
    strncpy_s(scriptComp.TypeName, scriptName.c_str(), sizeof(scriptComp.TypeName) - 1);
    scriptComp.TypeName[sizeof(scriptComp.TypeName) - 1] = '\0'; // Always null-terminate

    // Set the script path
    strncpy_s(scriptComp.ScriptPath, scriptPath.c_str(), sizeof(scriptComp.ScriptPath) - 1);
    scriptComp.ScriptPath[sizeof(scriptComp.ScriptPath) - 1] = '\0'; // Always null-terminate

    // 4. Mark for re-initialization by the ScriptingSystem at runtime
    // Setting Initialized = false forces the runtime to load this C# class 
    // and call its Start/Awake method.
    scriptComp.Initialized = false;

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
    if (parentId != ECS::Entity::NPOS32 && IsProtectedEntity(m_world, parentId)) {
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

        ECS::Entity rootEntity;

        // Check if this is new hierarchical format or old single-entity format
        if (prefabJson.contains("Entity")) {
            // New format with hierarchy: use DeserializeEntityHierarchy
            rootEntity = Serialization::EntitySerializer::DeserializeEntityHierarchy(*m_world, prefabJson["Entity"], parentId);
        }
        else if (prefabJson.contains("Components")) {
            // Old format: single entity
            // Create new entity
            rootEntity = Serialization::EntitySerializer::DeserializeEntity(*m_world, prefabJson);

            // Set parent relationship if not creating as root
            // NPOS32 is a special value meaning "no parent" (root entity)
            if (parentId != ECS::Entity::NPOS32) {
                // Convert EntityId to ECS::Entity object for world operations
                ECS::Entity parent = m_world->Resolve(parentId);
                // Only set parent if the parent entity exists and is valid
                if (!parent.IsNull() && m_world->IsAlive(parent)) {
                    // Use Attach to properly update hierarchy indices
                    m_world->Attach(rootEntity, parent);
                }
            }
        }
        else {
            LOG_ERROR("Invalid prefab format: missing Entity or Components");
            return ECS::Entity::NPOS32;
        }

        if (rootEntity.IsNull() || !m_world->IsAlive(rootEntity)) {
            LOG_ERROR("Failed to instantiate prefab: " << prefabPath);
            return ECS::Entity::NPOS32;
        }

        // Add prefab link component to track prefab relationship
        // This component connects the instance back to the prefab template file
        std::filesystem::path p(prefabPath);
        std::string linkPath = p.lexically_normal().string();
        m_world->Set<ECS::Components::PrefabLink>(rootEntity, ECS::Components::PrefabLink(linkPath));

        // Mark scene as dirty
        if (m_fileMenu) {
            m_fileMenu->MarkSceneDirty();
        }

        // Set entity name from prefab filename
        std::string prefabName = p.stem().string();
        LOG_INFO("Instantiated prefab: " << prefabName);

        // Return the entity ID so caller can select it
        return rootEntity.Index;
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
// Get all root entities (entities without parents)
// Uses World's ParentOf API to check hierarchy maintained by Attach/Detach
std::vector<EntityId> HierarchyPanel::_getRootEntities() const {
    std::vector<EntityId> roots;
    if (!m_world) return roots;

    // Iterate all entities and collect those without parents
    m_world->Each([&](ECS::Entity e) {
        if (ShouldHideFromHierarchy(m_world, e)) return;
        // Use World's ParentOf to check if entity has no parent
        if (m_world->ParentOf(e).IsNull()) {
            roots.push_back(e.Index);
        }
        });

    return roots;
}

// Get all children of an entity
// Uses World's ForChildren API which leverages the HierarchyIndex maintained by Attach/Detach
std::vector<EntityId> HierarchyPanel::_getChildren(EntityId parentId) const {
    std::vector<EntityId> children;
    if (!m_world) return children;

    ECS::Entity parent = m_world->Resolve(parentId);
    if (parent.IsNull() || !m_world->IsAlive(parent))
        return children;

    // Use World's ForChildren directly which respects sibling order from HierarchyIndex
    m_world->ForChildren(parent, [&](ECS::Entity child) {
        if (!ShouldHideFromHierarchy(m_world, child)) {
            children.push_back(child.Index);
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

// -------------------------------------------------------------------------
// Entity Order Management (for scene serialization)
// -------------------------------------------------------------------------

// Rebuild entity order by doing depth-first traversal of the hierarchy
// This preserves the visual order seen in the editor for scene files
void HierarchyPanel::RebuildEntityOrder() {
    m_entityOrder.clear();
    if (!m_world) return;

    // Start with all root entities
    auto roots = _getRootEntities();
    for (EntityId rootId : roots) {
        _rebuildEntityOrderRecursive(rootId);
    }
}

// Recursive helper to traverse hierarchy depth-first
void HierarchyPanel::_rebuildEntityOrderRecursive(EntityId entityId) {
    // Add current entity to order
    m_entityOrder.push_back(entityId);

    // Recursively add all children
    auto children = _getChildren(entityId);
    for (EntityId childId : children) {
        _rebuildEntityOrderRecursive(childId);
    }
}

// Clear UI state when scene changes to prevent stale references
void HierarchyPanel::ClearUIState() {
    m_selectedEntityIds.clear();
    m_anchorEntityId = ECS::Entity::NPOS32;
    m_renamingEntityId = ECS::Entity::NPOS32;
    m_contextMenuTarget = ECS::Entity::NPOS32;
    m_searchFilter.clear();

    // Notify selection callback that nothing is selected
    if (m_selectionCallback) {
        m_selectionCallback(ECS::Entity::NPOS32);
    }
}
