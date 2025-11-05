/* Start Header *****************************************************************/
/*!
\file   HierarchyWindow.h
\author Samantha Leong (70%)
        Foo Rui Qin    (30%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   5th November 2025
\brief
Handles the hierarchy window displaying all entities in a tree structure with
parent-child relationships.

Features:
- Tree view with collapsible parent nodes
- Entity selection and multi-selection
- Add/Remove/Clone entity operations
- Drag-drop reparenting
- Right-click context menus

References:
- ImGui tree node API for hierarchy display
- Unity's hierarchy window design patterns
*/
/* End Header *******************************************************************/

#ifndef HIERARCHY_WINDOW_H
#define HIERARCHY_WINDOW_H

#include <string>
#include <vector>
#include <functional>
#include "ecs/Entity.h"

// Forward declarations
struct ImFont;
class World;

class HierarchyWindow {
public:
    // Callback types for when selection changes
    using SelectionCallback = std::function<void(EntityId)>;

    // Initialize with fonts and world reference
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world);

    // Render the hierarchy window
    void Render();

    // Get currently selected entity
    EntityId GetSelectedEntity() const { return m_selectedEntityId; }

    // Set selected entity (called from other windows)
    void SetSelectedEntity(EntityId id) { m_selectedEntityId = id; }

    // Register callback for when selection changes
    void OnSelectionChanged(SelectionCallback callback) { m_selectionCallback = callback; }

private:
    // Render a single entity node in the tree
    void _renderEntityNode(EntityId entityId, int depth);

    // Get all root entities (no parent)
    std::vector<EntityId> _getRootEntities();

    // Get all children of an entity
    std::vector<EntityId> _getChildren(EntityId parentId);

    // Entity operations
    void _addEntity(const std::string& name, EntityId parentId = 0);
    void _removeEntity(EntityId id);
    void _cloneEntity(EntityId id);
    void _reparentEntity(EntityId childId, EntityId newParentId);

    // References
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    World* m_world = nullptr;

    // Selection state
    EntityId m_selectedEntityId = 0;
    SelectionCallback m_selectionCallback;

    // UI state
    std::vector<EntityId> m_expandedNodes;  // Which parent nodes are expanded
};

#endif // HIERARCHY_WINDOW_H