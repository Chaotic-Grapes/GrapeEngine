/* Start Header *****************************************************************/
/*!
\file   HierarchyPanel.h
\author Foo Rui Qin (60%)
        Samantha Leong (30%)
        Muhammad Nur Fadzly Bin Zulkifli (10%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   11th March 2026

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
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "EditorEntityActions.h"
#include "core/messaging/MessageSystem.h"

// Forward declaration
class BaseViewport;
namespace ECS { class PrefabManager; }
namespace Editor { class UndoSystem; }

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
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world,
        EntityActions* entityActions);

    // Update the world reference when switching between scenes, clearing selection and expanded nodes
    void SetWorld(ECS::World* world);

    // Set the prefab manager for displaying prefab instance information
    void SetPrefabManager(ECS::PrefabManager* manager) { m_prefabManager = manager; }

    // Register a callback to be notified when entity selection changes
    void OnSelectionChanged(SelectionCallback callback);

    // Set the primary selected entity, replacing the current selection
    void SetSelectedEntity(EntityId id);

    // Replace the current selection with the given set of entity IDs
    void SetSelectedEntities(const std::unordered_set<EntityId>& ids);

    // Set the viewport reference for entity focus operations
    void SetViewport(BaseViewport* viewport) { m_viewport = viewport; }

    // Set the undo system for recording hierarchy reorder actions
    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }

    // Return the full set of currently selected entity IDs
    const std::unordered_set<EntityId>& GetSelectedEntities() const { return m_selectedEntityIds; }

    // Return the primary selected entity, or NPOS32 if nothing is selected
    EntityId GetPrimarySelectedEntity() const { return m_selectedEntityIds.empty() ? ECS::Entity::NPOS32 : *m_selectedEntityIds.begin(); }

    // Set the file menu reference for save/load integration
    void SetFileMenu(EditorFileMenu* fileMenu) { m_fileMenu = fileMenu; }

    // Return the ordered entity list used for scene serialization
    const std::vector<EntityId>& GetEntityOrder() const { return m_entityOrder; }

    // Override the serialization entity order with the given list
    void SetEntityOrder(const std::vector<EntityId>& order);

    // Rebuild the serialization entity order from the current hierarchy state
    void RebuildEntityOrder();

    // Clear UI state (selection, rename mode, context menu) when the scene changes
    void ClearUIState();

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    // Main rendering function called every frame to draw the hierarchy window
    void Render();

private:
    // -------------------------------------------------------------------------
    // UI Sections
    // -------------------------------------------------------------------------

    // Render the header section with entity creation controls and entity count
    void _renderHeader();

    // Render the main entity tree with scrollable area and drag-drop support
    void _renderEntityTree();

    // -------------------------------------------------------------------------
    // Entity Tree Node Rendering
    // -------------------------------------------------------------------------

    // Render a single entity node and its children recursively
    void _renderEntityNode(EntityId entityId, int depth);

    // Handle mouse interactions with entity nodes (clicks, right-click, double-click)
    void _handleNodeInteraction(EntityId entityId);

    // Handle drag-drop operations for entity reparenting and prefab instantiation
    void _handleNodeDragDrop(EntityId entityId);

    // Handle drag-drop for tree background to reparent entities to root level
    void _handleTreeDragDrop();

    // -------------------------------------------------------------------------
    // Context Menu
    // -------------------------------------------------------------------------

    // Render the right-click context menu for entity operations
    void _renderEntityContextMenu(EntityId entityId);

    // Render context menu when right-clicking empty space in the hierarchy
    void _renderBackgroundContextMenu();

    // -------------------------------------------------------------------------
    // Entity Operations
    // -------------------------------------------------------------------------

    // Delete an entity and clean up selection state
    void _deleteEntity(EntityId entityId);

    // Create a duplicate of an entity with same components and hierarchy
    void _cloneEntity(EntityId entityId);

    // Add a new child entity to the specified parent
    void _addChildEntity(EntityId parentId);

    // Add a new root entity with no parent
    void _addRootEntity();

    // Collect top-level selected entities into the hierarchy clipboard
    void _copySelectedEntities();

    // Clone entities from the hierarchy clipboard and select the pasted results
    void _pasteCopiedEntities();

    // Queue deletion for all selected entities except protected ones
    void _deleteSelectedEntities();

    // Cut selected entities without touching the clipboard (Ctrl+X behaviour)
    void _cutSelectedEntities();

    // Start renaming an entity, preparing rename state and input buffer
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

    // Return all root entities (entities without a Parent component)
    std::vector<EntityId> _getRootEntities();

    // Return all direct children of a parent entity
    std::vector<EntityId> _getChildren(EntityId parentId);

    // Rebuild entity order list by traversing the hierarchy depth-first
    void _rebuildEntityOrderRecursive(EntityId entityId);

    // Return true if the entity matches the current search filter
    bool _matchesSearchFilter(EntityId entityId) const;

    // Handle clicking empty space to clear selection
    void _selectEmptySpace();

    // Seed root order from the serialized entity order when possible
    void _seedRootOrderFromEntityOrder();

    // Append an entity to the order list if not already present
    void _appendToOrderList(std::vector<EntityId>& order, EntityId entityId);

    // Remove an entity from the order list
    void _removeFromOrderList(std::vector<EntityId>& order, EntityId entityId);

    // Remove an entity from both root and child order lists
    void _removeEntityFromOrders(EntityId entityId);

    // Move an entity in the order list to appear after the target entity
    void _moveEntityInOrder(std::vector<EntityId>& order, EntityId entityId, EntityId targetId);

    // Apply an order list to root or child storage for the given parent
    void _applyOrder(EntityId parentId, const std::vector<EntityId>& order);

    // Push an undo command recording a reorder change for the given parent
    void _recordOrderChange(EntityId parentId, const std::vector<EntityId>& before, const std::vector<EntityId>& after);

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    ImFont* m_mainFont = nullptr;       // Main body font
    ImFont* m_boldFont = nullptr;       // Bold font for prefab instance labels
    ImFont* m_symbolsFont = nullptr;    // Symbols/icon font for icon-only buttons

    // System references
    ECS::World* m_world = nullptr;                  // ECS world containing all entities and components
    EntityActions* m_entityActions = nullptr;       // Handles entity create, clone and delete operations
    BaseViewport* m_viewport = nullptr;             // Viewport reference for entity focus
    EditorFileMenu* m_fileMenu = nullptr;           // File menu reference for save/load integration
    ECS::PrefabManager* m_prefabManager = nullptr;  // Prefab manager for prefab instance display
    Editor::UndoSystem* m_undoSystem = nullptr;     // Undo system for reorder actions

    // Selection state
    std::unordered_set<EntityId> m_selectedEntityIds;   // Currently selected entity IDs (empty = no selection)
    EntityId m_anchorEntityId = ECS::Entity::NPOS32;    // Anchor entity for shift-selection range
    EntityId m_contextMenuTarget = ECS::Entity::NPOS32; // Entity targeted for context menu operations
    EntityId m_pendingClickSelectionId = ECS::Entity::NPOS32; // Deferred single-click select to allow immediate drag

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
    EntityId m_lastClickedEntity = ECS::Entity::NPOS32;    // Last entity that was clicked
    float m_lastClickTime = 0.0f;                          // Time of last click for rename delay
    static constexpr float RENAME_DELAY_THRESHOLD = 0.45f; // Min delay for slow double-click rename
    static constexpr float RENAME_DELAY_MAX = 0.90f;       // Max delay to treat as intentional second click

    // Reorder undo coalescing
    EntityId m_lastReorderParentId = ECS::Entity::NPOS32;  // Parent ID for coalescing reorder undo
    float m_lastReorderTime = -1000.0f;                    // Timestamp for coalescing reorder undo
    static constexpr float REORDER_COALESCE_WINDOW = 0.6f; // Time window to merge consecutive reorder commands

    // Entity order for scene serialization (preserves visual hierarchy order)
    // This is a HINT for saving: the ECS World's HierarchyIndex is the source of truth for rendering
    std::vector<EntityId> m_entityOrder;            // Ordered entity list for serialization
    std::vector<EntityId> m_rootOrder;              // Persistent root order for stable hierarchy display
    std::unordered_map<EntityId, std::vector<EntityId>> m_childOrder; // Persistent per-parent child order
    std::vector<EntityId> m_copiedEntityIds;        // Internal clipboard for Ctrl+C/Ctrl+V

    // Entities queued for deletion at end of frame to avoid mid-iteration removal
    std::vector<EntityId> m_deferredDeletions;

    // Message subscription for viewport entity selection sync
    Messaging::SubscriptionHandle m_entitySelectedSubscription;

    // Prevents internal notify loops from collapsing multiselect
    bool m_suppressSelectionSync = false;
};

#endif