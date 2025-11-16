/* Start Header *****************************************************************/
/*!
\file   Viewport.h
\author Samantha Leong (80%)
        Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   16th November 2025
\brief
Declares the Viewport class for viewport rendering and interaction.
File menu operations have been moved to EditorFileMenu.
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/World.h"
#include "../editor/EditorFileMenu.h"
#include <imgui.h>
#include <memory>

// Forward declarations
namespace ECS { class RendererSystem; }
namespace Scenes { class SceneManager; }
using EntityId = uint32_t;

// Viewport panel for main menu, viewport rendering, and entity operations
class Viewport {
public:
    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
        ECS::World* world, Scenes::SceneManager* sceneManager);
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
    // Accessors
    // -------------------------------------------------------------------------
    inline bool HasValidWorld() const { return m_world != nullptr; }
    inline EntityId GetSelectedEntityId() const { return m_selectedEntityId; }
    inline bool IsViewportHovered() const { return m_isViewportHovered; }

private:
    // -------------------------------------------------------------------------
    // Viewport
    // -------------------------------------------------------------------------
    void _renderViewport();

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;
    ECS::World* m_world = nullptr;
    std::shared_ptr<ECS::RendererSystem> m_rendererSystem;

    EditorFileMenu m_fileMenu;

    EntityId m_selectedEntityId = 0;
    bool m_isViewportHovered = false;
    float m_uiScale = 1.0f;
};
