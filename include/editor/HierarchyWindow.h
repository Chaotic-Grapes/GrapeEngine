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

    // This sets up fonts the world and editor core
    // It prepares the tree view for entities
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world, EditorCore* editorCore);

    // This draws the hierarchy window
    // It shows entities and supports selection
    void Render();

    // This updates the world and clears selection
    // It keeps the tree synced with scenes
    void SetWorld(ECS::World* world);

    // This returns the currently selected entity
    // It is used by other panels
    EntityId GetSelectedEntity() const { return m_selectedEntityId; }

    // This sets the selected entity
    // It updates internal state
    void SetSelectedEntity(EntityId id) { m_selectedEntityId = id; }

    // This registers a callback for selection changes
    // It calls it whenever user picks a new entity
    void OnSelectionChanged(SelectionCallback callback) { m_selectionCallback = callback; }

private:
    // This draws one entity node in the tree
    // It handles expand select and context actions
    void _renderEntityNode(EntityId entityId, int depth);

    // This returns all root entities
    // It filters those without a parent
    std::vector<EntityId> _getRootEntities();

    // This returns children of an entity
    // It looks up hierarchy links
    std::vector<EntityId> _getChildren(EntityId parentId);

    // This creates a prefab as a child
    // It adds the new entity under the parent
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

#endif