/* Start Header *****************************************************************/
/*!
\file   HierarchyWindow.h
\author Foo Rui Qin    (50%)
        Samantha Leong (50%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Handles the hierarchy window displaying all entities in a tree structure with
parent-child relationships.
- Pure UI/View layer - delegates ALL entity operations to EditorCore
- EditorCore handles add/remove/clone/reparent operations
- HierarchyWindow only manages tree rendering and UI interactions
*/
/* End Header *******************************************************************/

#ifndef HIERARCHY_WINDOW_H
#define HIERARCHY_WINDOW_H

#include <string>
#include <vector>
#include <functional>
#include "ecs/Entity.h"
#include "../editor/EditorCore.h"

// Forward declarations
struct ImFont;
namespace ECS {
    class World;
}

class HierarchyWindow {
public:
    // Callback types for when selection changes
    using SelectionCallback = std::function<void(EntityId)>;

    // Initialize with fonts, world reference, and EditorCore for entity operations
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world, EditorCore* editorCore);

    // Render the hierarchy window
    void Render();

    // Update world and clear selection when scenes change
    void SetWorld(ECS::World* world);

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

    // Get all children of an entity (UI helper for tree rendering)
    std::vector<EntityId> _getChildren(EntityId parentId);

    // Instantiate a prefab as a child of the specified parent entity (UI-specific)
    void _instantiatePrefabAsChild(const std::string& prefabPath, EntityId parentId);

    // References
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    ECS::World* m_world = nullptr;
    EditorCore* m_editorCore = nullptr;

    // Selection state
    EntityId m_selectedEntityId = 0;
    SelectionCallback m_selectionCallback;

    // UI state
    std::vector<EntityId> m_expandedNodes;  // Which parent nodes are expanded
};

#endif // HIERARCHY_WINDOW_H