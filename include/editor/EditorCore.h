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
#include <memory>

// Messaging system for subscriptions
#include "core/messaging/MessageSystem.h"

// Forward declaration to avoid heavy include in header
namespace ECS { class RendererSystem; }

class EditorCore {
public:
    EditorCore() = default;
    ~EditorCore() = default;

    // This sets up fonts and binds the world
    // It prepares editor subsystems and panels
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);
    // This draws auxiliary editor windows and menus
    // It handles layout and quick tools
    void ShowEditorWindows();
    // This handles picking dragging and hotkeys in the viewport
    // It talks to the renderer for selection updates
    void HandleInWorldInteraction();

    // This creates a new entity optionally under a parent
    // It adds default components and tracks it
    void AddEntity(const std::string& name, EntityId parentId = 0);
    // This deletes an entity and optionally its children
    // It updates caches and selection state
    void RemoveEntity(EntityId id, bool recursive = true);
    // This duplicates an entity hierarchy
    // It creates a copy with the same components
    void CloneEntity(EntityId id);
    // This moves an entity under a new parent
    // It updates hierarchy links cleanly
    void ReparentEntity(EntityId childId, EntityId newParentId);
    // This removes every entity from the world
    // It resets selection and caches
    void ClearAllEntities();

    // This checks if the world pointer is set
    // It guards operations that need a world
    bool HasValidWorld() const;
    // This updates the world reference
    // It resets caches and keeps panels in sync
    void SetWorld(ECS::World* world);

    // Get the currently selected entity ID (synced with renderer picking)
    EntityId GetSelectedEntityId() const { return m_selectedEntityId; }

    // Access the renderer system used by the editor viewport
    std::shared_ptr<ECS::RendererSystem> GetRendererSystem() const { return m_rendererSystem; }

    // Current scene path on disk
    std::string m_currentScenePath;
    // Current scene name for display
    std::string m_currentSceneName = "Untitled";

private:
    // This renders the main menu bar
    // It provides file and tools actions
    void _showMainMenu();
    // This draws the game viewport
    // It handles camera controls and overlays
    void _showViewport();
    // This creates an entity with default setup
    // It returns a ready to use entity
    ECS::Entity _createGameEntity(const std::string& name);
    // This clears cached labels and states
    // It forces a fresh rebuild next frame
    void _invalidateCache();
    // This returns direct children of a parent
    // It reads the hierarchy graph
    std::vector<EntityId> _getChildren(EntityId parentId) const;
    // This returns a cached delete label
    // It generates one if missing
    const std::string& _getDeleteLabel(EntityId id);
    // This returns a cached clone label
    // It creates and stores if needed
    const std::string& _getCloneLabel(EntityId id);
    // This returns whether a header is collapsed
    // It retrieves stored UI state
    const bool& _getCollapsedHeaderBool(EntityId id);

    // This writes the active scene to disk
    // It returns true on success
    bool _saveActiveScene(const std::string& path);
    // This loads a scene file into the world
    // It returns true on success
    bool _loadSceneFromPath(const std::string& path);
    // This creates a new empty scene
    // It resets world and files
    void _createNewScene();
    // This opens a dialog to pick a scene
    // It lets you browse and select a file
    void _openSceneDialog();
    // This saves the current scene
    // It writes data to the path
    void _saveScene();
    // This opens a dialog to save the scene
    // It supports templates if requested
    void _saveSceneAsDialog(bool isTemplate);
    // This saves editor UI state to disk
    // It remembers layout and selections
    void _saveEditorState();
    // This loads editor UI state from disk
    // It restores layout and selections
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

    // Renderer system used to drive the editor viewport
    std::shared_ptr<ECS::RendererSystem> m_rendererSystem;

    // Whether the mouse is currently hovering the viewport image (for picking/dragging)
    bool m_isViewportHovered = false;

    // Transient warning shown in viewport toolbar (e.g., no active Camera3D)
    std::string m_cameraToggleWarning;
    double m_cameraWarningExpiry = 0.0;
    Messaging::SubscriptionHandle m_debugMsgSubscription;
public:
    // This returns whether the ImGui viewport image is hovered
    // It helps coordinate picking and drag operations
    bool IsViewportHovered() const { return m_isViewportHovered; }
};

#endif