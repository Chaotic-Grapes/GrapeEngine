/* Start Header *****************************************************************/
/*!
\file   EditorCore.h
\author Samantha Leong (80%)
        Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Declares the EditorCore class for core editor functionality and centralized
entity management.
- Entity/Level management layer (Model + Controller)
- Single source of truth for all entity CRUD operations
- Delegates to World/EntityManager for actual ECS operations
- Used by HierarchyWindow and other UI panels
*/
/* End Header *******************************************************************/

#ifndef EDITOR_CORE_H
#define EDITOR_CORE_H

#include "ecs/World.h"
#include "ecs/Entity.h"
#include <imgui.h>
#include <unordered_map>
#include <string>
#include <vector>

class EditorCore {
public:
    EditorCore() = default;
    ~EditorCore() = default;

    // Initialize with fonts and world reference
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world);

    // Render main menu bar at top of screen
    void ShowEditorWindows();

    // Handle mouse picking and dragging entities in viewport
    void HandleInWorldInteraction();

    // Level management (currently disabled but kept for future use)
    // void SaveLevel(const std::string& filename);
    // void LoadLevel(const std::string& filename);

    // Centralized entity management operations
    void AddEntity(const std::string& name, EntityId parentId = 0);
    void RemoveEntity(EntityId id, bool recursive = true);
    void CloneEntity(EntityId id);
    void ReparentEntity(EntityId childId, EntityId newParentId);
    void ClearAllEntities();

    // Check if world is valid before operations
    bool HasValidWorld() const;

private:
    // Render main menu bar with File menu
    void _showMainMenu();

    // Create entity with default components (Transform, Shape, Collider)
    Entity _createGameEntity(const std::string& name);

    // Clear cached UI labels when entities change
    void _invalidateCache();

    // Get all children of a parent entity (for recursive operations)
    std::vector<EntityId> _getChildren(EntityId parentId) const;

    // Get unique labels for UI buttons (for ImGui ID uniqueness)
    const std::string& _getDeleteLabel(EntityId id) const;
    const std::string& _getCloneLabel(EntityId id) const;
    const bool& _getCollapsedHeaderBool(EntityId id) const;

    // Core state
    World* m_world = nullptr;
    EntityId m_selectedEntityId = 0;

    // Fonts for UI rendering
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // Cached UI labels to avoid string allocations every frame
    mutable std::unordered_map<EntityId, std::string> m_cachedDeleteLabels;
    mutable std::unordered_map<EntityId, std::string> m_cachedCloneLabels;
    mutable std::unordered_map<EntityId, bool> m_cachedCollapsedHeaders;

    // Max name length constraint
    static constexpr size_t MAX_OBJECT_NAME_LENGTH = 64;
};

#endif