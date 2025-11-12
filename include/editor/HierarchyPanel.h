/* Start Header *****************************************************************/
/*!
\file   HierarchyPanel.h
\author Foo Rui Qin    (50%)
        Samantha Leong (50%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   5th November 2025
\brief
Declares the HierarchyPanel class for Unity-like hierarchy window UI.
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/World.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>

// Forward declarations
class Viewport;

using EntityId = uint32_t;
using SelectionCallback = std::function<void(EntityId)>;

// Hierarchy panel for displaying and managing entity tree structure
class HierarchyPanel {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world, Viewport* viewport);
    void SetWorld(ECS::World* world);
    void OnSelectionChanged(SelectionCallback callback);

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------
    void Render();

private:
    // -------------------------------------------------------------------------
    // UI Sections
    // -------------------------------------------------------------------------
    void _renderHeader();
    void _renderEntityTree();
    void _renderFooterButtons();

    // -------------------------------------------------------------------------
    // Entity Tree Node Rendering
    // -------------------------------------------------------------------------
    void _renderEntityNode(EntityId entityId, int depth);
    void _handleNodeInteraction(EntityId entityId);
    void _handleNodeDragDrop(EntityId entityId);
    void _handleTreeDragDrop();

    // -------------------------------------------------------------------------
    // Context Menu
    // -------------------------------------------------------------------------
    void _renderEntityContextMenu();

    // -------------------------------------------------------------------------
    // Entity Operations
    // -------------------------------------------------------------------------
    void _deleteEntity(EntityId entityId);
    void _cloneEntity(EntityId entityId);
    void _addChildEntity(EntityId parentId);

    // -------------------------------------------------------------------------
    // Prefab Operations
    // -------------------------------------------------------------------------
    void _instantiatePrefabAsChild(const std::string& prefabPath, EntityId parentId);
    void _updateFromPrefab(EntityId entityId);
    void _saveToPrefab(EntityId entityId);

    // -------------------------------------------------------------------------
    // Helper Methods
    // -------------------------------------------------------------------------
    std::vector<EntityId> _getRootEntities() const;
    std::vector<EntityId> _getChildren(EntityId parentId) const;

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    ECS::World* m_world = nullptr;
    Viewport* m_viewport = nullptr;

    EntityId m_selectedEntityId = 0;
    EntityId m_contextMenuTarget = 0;
    std::unordered_set<EntityId> m_expandedNodes;
    SelectionCallback m_selectionCallback;
};
