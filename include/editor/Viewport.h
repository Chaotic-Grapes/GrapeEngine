/* Start Header *****************************************************************/
/*!
\file   ViewportPanel.h
\author Samantha Leong (80%)
        Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Declares the ViewportPanel class for core editor functionality and entity management.
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/World.h"
#include <imgui.h>
#include <string>
#include <memory>
#include <functional>

// Forward declarations
namespace ECS { class RendererSystem; }
using EntityId = uint32_t;

// Viewport panel for main menu, viewport rendering, and entity operations
class Viewport {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world);
    void SetWorld(ECS::World* world);

    // -------------------------------------------------------------------------
    // Update
    // -------------------------------------------------------------------------
    void HandleInWorldInteraction();

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------
    void ShowEditorWindows();

    // -------------------------------------------------------------------------
    // Entity Operations
    // -------------------------------------------------------------------------
    void AddEntity(const std::string& name, EntityId parentId);
    void ReparentEntity(EntityId childId, EntityId newParentId);
    void RemoveEntity(EntityId id, bool recursive);
    void CloneEntity(EntityId id);
    void ClearAllEntities();
    void FocusOnEntity(EntityId id);

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------
    inline bool HasValidWorld() const { return m_world != nullptr; }
    inline EntityId GetSelectedEntityId() const { return m_selectedEntityId; }
    inline bool IsViewportHovered() const { return m_isViewportHovered; }

private:
    // -------------------------------------------------------------------------
    // Keyboard Shortcuts
    // -------------------------------------------------------------------------
    void _handleKeyboardShortcuts();

    // -------------------------------------------------------------------------
    // Main Menu
    // -------------------------------------------------------------------------
    void _renderMainMenu();

    // -------------------------------------------------------------------------
    // Viewport
    // -------------------------------------------------------------------------
    void _renderViewport();

    // -------------------------------------------------------------------------
    // Scene Management
    // -------------------------------------------------------------------------
    void _createNewScene();
    void _openSceneDialog();
    void _openScene(const std::string& path);
    void _saveScene();
    void _saveSceneAsDialog(bool isTemplate);
    void _saveSceneToFile(const std::string& path);

    // -------------------------------------------------------------------------
    // Helper Methods
    // -------------------------------------------------------------------------
    void _invalidateCache();
    void _saveEditorState();
    void _loadEditorState();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    ECS::World* m_world = nullptr;
    std::shared_ptr<ECS::RendererSystem> m_rendererSystem;

    EntityId m_selectedEntityId = 0;
    bool m_isViewportHovered = false;
    float m_uiScale = 1.0f;
};
