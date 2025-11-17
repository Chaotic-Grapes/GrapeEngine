/* Start Header *****************************************************************/
/*!
\file   HierarchyPanel.h
\author Foo Rui Qin    (50%)
        Samantha Leong (50%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025

\brief
Declares the HierarchyPanel class which manages the hierarchy window used to
browse and organise entities in a scene.

The hierarchy presents the ECS world as a tree, letting the editor display
parent-child relationships clearly. It handles selection, drag-drop reparenting,
entity creation and deletion and supports prefab instantiation by accepting
dragged prefab assets. Other editor panels use this class to stay updated on
which entity the user is working with.
*/
/* End Header *******************************************************************/

#ifndef HIERARCHY_PANEL_H
#define HIERARCHY_PANEL_H

#include "ecs/World.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include "../editor/EditorEntityActions.h"

// EntityId is a numeric identifier for entities in the ECS world
using EntityId = uint32_t;

// Callback function type for entity selection events
// Other systems can register to be notified when selection changes
using SelectionCallback = std::function<void(EntityId)>;

// Hierarchy panel for displaying and managing entity tree structure
class HierarchyPanel {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    // Initialize the hierarchy panel with required fonts and system references
    // Fonts are used for consistent UI styling across the panel
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world, 
        EntityActions* entityActions);

    // Update the world reference when switching between scenes
    // Clears internal state like selection and expanded nodes
    void SetWorld(ECS::World* world);

    // Register a callback function to be notified when entity selection changes
    // Useful for synchronizing selection with other editor panels
    void OnSelectionChanged(SelectionCallback callback);

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    // Main rendering function called every frame to draw the hierarchy window
    // Handles entity tree display, interactions and UI controls
    void Render();

private:
    // -------------------------------------------------------------------------
    // UI Sections
    // -------------------------------------------------------------------------

    // Render the header section with entity creation controls and entity count
    void _renderHeader();

    // Render the main entity tree with scrollable area and drag-drop support
    void _renderEntityTree();

    // Render footer buttons like Clear All for scene management
    void _renderFooterButtons();

    // -------------------------------------------------------------------------
    // Entity Tree Node Rendering
    // -------------------------------------------------------------------------

    // Render a single entity node and its children recursively
    // Depth parameter tracks indentation level for visual hierarchy
    void _renderEntityNode(EntityId entityId, int depth);

    // Handle mouse interactions with entity nodes (clicks, right-click, double-click)
    void _handleNodeInteraction(EntityId entityId);

    // Handle drag-drop operations for entity reparenting and prefab instantiation
    void _handleNodeDragDrop(EntityId entityId);

    // Handle drag-drop for tree background (reparenting to root level)
    void _handleTreeDragDrop();

    // -------------------------------------------------------------------------
    // Context Menu
    // -------------------------------------------------------------------------

    // Render the right-click context menu for entity operations
    void _renderEntityContextMenu();

    // -------------------------------------------------------------------------
    // Entity Operations
    // -------------------------------------------------------------------------

    // Delete an entity and clean up selection state
    void _deleteEntity(EntityId entityId);

    // Create a duplicate of an entity with same components and hierarchy
    void _cloneEntity(EntityId entityId);

    // Add a new child entity to the specified parent
    void _addChildEntity(EntityId parentId);

    // Add a new root entity (no parent)
    void _addRootEntity();

    // -------------------------------------------------------------------------
    // Prefab Operations
    // -------------------------------------------------------------------------

    // Instantiate a prefab file as a child of the specified parent entity
    // Returns the entity ID of the newly created instance
    EntityId _instantiatePrefabAsChild(const std::string& prefabPath, EntityId parentId);

    // -------------------------------------------------------------------------
    // Helper Methods
    // -------------------------------------------------------------------------

    // Get all root entities (entities without Parent component)
    std::vector<EntityId> _getRootEntities() const;

    // Get all direct children of a parent entity
    std::vector<EntityId> _getChildren(EntityId parentId) const;

    // Handle clicking empty space to clear selection
    void _selectEmptySpace();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // Font references for UI styling
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // System references
    ECS::World* m_world = nullptr;                  // ECS world containing all entities and components
    EntityActions* m_entityActions = nullptr; // For entity operations

    // Selection state
    EntityId m_selectedEntityId = 0;                // Currently selected entity ID (0 = no selection)
    EntityId m_contextMenuTarget = 0;               // Entity targeted for context menu operations

    // UI state
    std::unordered_set<EntityId> m_expandedNodes;   // Track which tree nodes are expanded
    SelectionCallback m_selectionCallback;          // Callback for selection change events
};

#endif