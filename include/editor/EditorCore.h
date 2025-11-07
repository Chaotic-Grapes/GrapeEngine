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

    // Initialize core editor with fonts and world
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);
    // Render editor auxiliary windows and menus
    void ShowEditorWindows();
    // Handle interactions inside the game viewport
    void HandleInWorldInteraction();

    // Add a new entity under optional parent
    void AddEntity(const std::string& name, EntityId parentId = 0);
    // Remove an entity optionally including children
    void RemoveEntity(EntityId id, bool recursive = true);
    // Clone an entity and its hierarchy
    void CloneEntity(EntityId id);
    // Reparent an entity under a new parent
    void ReparentEntity(EntityId childId, EntityId newParentId);
    // Clear all entities from the scene
    void ClearAllEntities();

    // Check if we have a valid world bound
    bool HasValidWorld() const;
    // Update the world reference for editor operations
    void SetWorld(ECS::World* world);

    // Current scene path on disk
    std::string m_currentScenePath;
    // Current scene name for display
    std::string m_currentSceneName = "Untitled";

private:
    // Show the main menu bar with file and tools
    void _showMainMenu();
    // Show the viewport window and render scene
    void _showViewport();
    // Create a new entity with default components
    ECS::Entity _createGameEntity(const std::string& name);
    // Invalidate cached labels and states
    void _invalidateCache();
    // Get direct children of a parent entity
    std::vector<EntityId> _getChildren(EntityId parentId) const;
    // Get cached delete label for an entity
    const std::string& _getDeleteLabel(EntityId id);
    // Get cached clone label for an entity
    const std::string& _getCloneLabel(EntityId id);
    // Get cached collapsed header state for an entity
    const bool& _getCollapsedHeaderBool(EntityId id);

    // Save the active scene to a path
    bool _saveActiveScene(const std::string& path);
    // Load a scene from a path into the world
    bool _loadSceneFromPath(const std::string& path);
    // Create a new empty scene file and world
    void _createNewScene();
    // Open a file dialog for loading scenes
    void _openSceneDialog();
    // Save the current scene
    void _saveScene();
    // Save the scene using a dialog optionally as template
    void _saveSceneAsDialog(bool isTemplate);
    // Persist editor UI state to disk
    void _saveEditorState();
    // Load editor UI state from disk
    void _loadEditorState();

    // World pointer for scene operations
    ECS::World* m_world = nullptr;
    // Currently selected entity id
    EntityId m_selectedEntityId = 0;

    // Main font pointer for UI text
    ImFont* m_mainFont = nullptr;
    // Bold font pointer for headers
    ImFont* m_boldFont = nullptr;
    // Symbols font pointer for icons
    ImFont* m_symbolsFont = nullptr;

    // UI scale for zooming editor windows
    float m_uiScale = 1.35f;

    // Cached delete labels for entities
    std::unordered_map<EntityId, std::string> m_cachedDeleteLabels;
    // Cached clone labels for entities
    std::unordered_map<EntityId, std::string> m_cachedCloneLabels;
    // Cached collapsed header states for entities
    std::unordered_map<EntityId, bool> m_cachedCollapsedHeaders;

    // Max object name length used by editor input fields
    static constexpr size_t MAX_OBJECT_NAME_LENGTH = 64;
};

#endif