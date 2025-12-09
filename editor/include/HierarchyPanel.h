/* Start Header *****************************************************************/
/*!
\file   HierarchyPanel.h
\author Foo Rui Qin    (45%)
        Samantha Leong (45%)
        Muhammad Nur Fadzly Bin Zulkifli (10%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
        muhammadnurfadzly.b@digipen.edu
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
#include "EditorEntityActions.h"
#include "ecs/gui/GUIHelpers.h"

// Forward declaration
class BaseViewport;

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

    void SetSelectedEntity(EntityId id);
    void SetSelectedEntities(const std::unordered_set<EntityId>& ids);
    void SetViewport(BaseViewport* viewport) { m_viewport = viewport; }
    const std::unordered_set<EntityId>& GetSelectedEntities() const { return m_selectedEntityIds; }
    EntityId GetPrimarySelectedEntity() const { return m_selectedEntityIds.empty() ? ECS::Entity::NPOS32 : *m_selectedEntityIds.begin(); }
    void SetFileMenu(EditorFileMenu* fileMenu) { m_fileMenu = fileMenu; }

    // Entity order for scene serialization (preserves visual hierarchy order)
    const std::vector<EntityId>& GetEntityOrder() const { return m_entityOrder; }
    void SetEntityOrder(const std::vector<EntityId>& order) { m_entityOrder = order; }
    void RebuildEntityOrder(); // Rebuild from current hierarchy state

    // Clear UI state (selection, rename mode, context menu) - call when scene changes
    void ClearUIState();

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

    // Render context menu when right-clicking empty space in the hierarchy
    void _renderBackgroundContextMenu();

    // Ensure entity has a ScriptInstance component and set its script class name and path
    void _attachScriptComponent(EntityId entityId, const std::string& scriptName, const std::string& scriptPath); // scriptName must be fully qualified (e.g., "MyGame.PlayerController")

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

    // Start renaming an entity (prepare rename state and buffers)
    void _startRename(EntityId entityId);

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

    // Rebuild entity order list by traversing hierarchy depth-first
    void _rebuildEntityOrderRecursive(EntityId entityId);

    // Check if entity matches current search filter
    bool _matchesSearchFilter(EntityId entityId) const;

    // Handle clicking empty space to clear selection
    void _selectEmptySpace();
    
    // Opens a file dialog to select a script file and attaches the component using the filename as the script class name.
    void _importAndAttachScript(EntityId entityId);

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    // Font references for UI styling
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // System references
    ECS::World* m_world = nullptr;                  // ECS world containing all entities and components
    EntityActions* m_entityActions = nullptr;       // For entity operations
    BaseViewport* m_viewport = nullptr;             // For entity focus
    EditorFileMenu* m_fileMenu = nullptr;

    // Selection state
    std::unordered_set<EntityId> m_selectedEntityIds;   // Currently selected entity IDs (empty = no selection)
    EntityId m_anchorEntityId = ECS::Entity::NPOS32;    // Anchor entity for shift-selection range
    EntityId m_contextMenuTarget = ECS::Entity::NPOS32; // Entity targeted for context menu operations

    // UI state
    std::unordered_set<EntityId> m_expandedNodes;   // Track which tree nodes are expanded
    SelectionCallback m_selectionCallback;          // Callback for selection change events

    // Search filter state
    char m_searchBuffer[256] = "";                  // Buffer for search text input
    std::string m_searchFilter = "";                // Active search filter string

    // Rename state
    EntityId m_renamingEntityId = ECS::Entity::NPOS32;  // Entity currently being renamed
    char m_renameBuffer[128] = "";                      // Buffer for rename text input
    bool m_focusRenameInput = false;                    // Flag to focus rename input on next frame

    // Click timing for distinguishing fast double-click from slow double-click
    EntityId m_lastClickedEntity = ECS::Entity::NPOS32; // Last entity that was clicked
    float m_lastClickTime = 0.0f;                       // Time of last click
    static constexpr float RENAME_DELAY_THRESHOLD = 0.75f; // Delay threshold for rename (in seconds)

    // Entity order for scene serialization (preserves visual hierarchy order)
    // This is a HINT for saving - the ECS World's HierarchyIndex is the source of truth for rendering
    std::vector<EntityId> m_entityOrder;            // Ordered list for serialization

    // DON'T REMOVE: IMPORTANT
    std::vector<EntityId> m_deferredDeletions;
};

#endif