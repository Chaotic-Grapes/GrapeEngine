/* Start Header *****************************************************************/
/*!
\file   Viewport.h
\author Samantha Leong (80%)
        Foo Rui Qin    (20%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Header for Viewport class handling viewport rendering and entity selection with events.
*/
/* End Header *******************************************************************/

#ifndef VIEWPORT_H
#define VIEWPORT_H

#include "ecs/World.h"
#include "ecs/Entity.h"
#include "EditorFileMenu.h"
#include "graphics/EditorCamera.hpp"
#include "../editor/UndoSystem.h"
#include <imgui.h>
#include <memory>
#include <functional>

// Forward declarations
namespace ECS { class RendererSystem; }
namespace Scenes { class SceneManager; }
namespace Engine { class EditorCamera; }

using EntityId = uint32_t;
class EditorFileMenu;

class Viewport {
public:
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
        ECS::World* world, Scenes::SceneManager* sceneManager);

    void SetWorld(ECS::World* world);
    void HandleInWorldInteraction();
    void ShowEditorWindows();

    // Event registration
    void OnSelectionChanged(std::function<void(EntityId)> callback);
    void SetFileMenu(EditorFileMenu* fileMenu);

    // Accessors
    EntityId GetSelectedEntityId() const;
    bool IsViewportHovered() const;
    bool HasValidWorld() const { return m_world != nullptr; }

    // Undo System
    Editor::UndoSystem* GetUndoSystem() { return &m_undoSystem; }

private:
    void _renderViewport();

    ECS::World* m_world = nullptr;
    EditorFileMenu* m_fileMenu = nullptr;

    // UI fonts
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // Renderer (manages EditorCamera internally)
    std::shared_ptr<ECS::RendererSystem> m_rendererSystem;

    // State
    EntityId m_selectedEntityId = 0;
    bool m_isViewportHovered = false;

    // Event callback
    std::function<void(EntityId)> m_onSelectionChanged;

    // Undo system
    Editor::UndoSystem m_undoSystem;
};

#endif // VIEWPORT_H