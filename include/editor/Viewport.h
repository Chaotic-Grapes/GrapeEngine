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
#include "UndoSystem.h"
#include <imgui.h>
#include "ImGuizmo.h"
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
    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }

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
    
    // Game window renderer (always uses scene camera)
    std::shared_ptr<ECS::RendererSystem> m_gameRendererSystem;

    // State
    EntityId m_selectedEntityId = 0;
    bool m_isViewportHovered = false;
    int m_activeTab = 0; // 0 = Scene, 1 = Game

    // Stores the exact screen position and size of the drawn scene texture. M3<<<<<<<<<<<<<<<<<<<<<<<
    ImVec2 m_sceneDrawPos = { 0.0f, 0.0f };
    ImVec2 m_sceneDrawSize = { 0.0f, 0.0f };
    
    // Game window aspect ratio settings
    int m_selectedAspectRatio = 0; // Index into aspect ratio list
    bool m_freeAspect = true;      // Whether to use free aspect or fixed ratio

    // Event callback
    std::function<void(EntityId)> m_onSelectionChanged;

    // Undo system
    Editor::UndoSystem* m_undoSystem = nullptr;
};

#endif // VIEWPORT_H