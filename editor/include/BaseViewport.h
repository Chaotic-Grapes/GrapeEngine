/* Start Header *****************************************************************/
/*!
\file   BaseViewport.h
\author Samantha Leong (50%)
        Foo Rui Qin    (50%)
\par    s.leong@digipen.edu
        ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Base class for Scene and Game viewports. Provides shared functionality for
rendering, entity selection, and camera management.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BASE_VIEWPORT_H
#define BASE_VIEWPORT_H

#include "ecs/World.h"
#include "ecs/Entity.h"
#include "EditorFileMenu.h"
#include "EditorCamera.hpp"
#include "UndoSystem.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "Math/Quaternion.h"
#include "Math/Vector3D.h"
#include <imgui.h>
#include "ImGuizmo.h"
#include <glm/glm.hpp>
#include <memory>
#include <functional>

// Forward declarations
namespace ECS { class RendererSystem; }
namespace Scenes { class SceneManager; }
namespace Editor { class EditorCamera; }

using EntityId = uint32_t;
class EditorFileMenu;

class BaseViewport {
public:
    virtual ~BaseViewport() = default;

    enum class ViewportType {
        Scene,
        Game
    };

    void SetViewportType(ViewportType type) { m_viewportType = type; }
    ViewportType GetViewportType() const { return m_viewportType; }
    bool IsSceneViewport() const { return m_viewportType == ViewportType::Scene; }
    bool IsGameViewport() const { return m_viewportType == ViewportType::Game; }

    virtual void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
        ECS::World* world, Scenes::SceneManager* sceneManager);

    virtual void SetWorld(ECS::World* world);
    virtual void HandleInWorldInteraction() = 0;
    virtual void ShowEditorWindows() = 0;

    // Event registration
    virtual void OnSelectionChanged(std::function<void(EntityId)> callback);
    virtual void SetFileMenu(EditorFileMenu* fileMenu);

    // Accessors
    EntityId GetSelectedEntityId() const;
    bool IsViewportHovered() const;
    bool HasValidWorld() const { return m_world != nullptr; }

    // Set the currently selected entity programmatically (e.g. when selection
    // is changed from the hierarchy). This updates internal state and the
    // renderer's selected entity so the gizmo / outline appear.
    virtual void SetSelectedEntity(EntityId id);

    // Undo System
    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }
    virtual void FocusOnEntity(EntityId entityId);

    // Get viewport bounds for UI click detection
    ImVec2 GetSceneDrawPos() const { return m_sceneDrawPos; }
    ImVec2 GetSceneDrawSize() const { return m_sceneDrawSize; }

    // Get the editor camera
    Editor::EditorCamera* GetEditorCamera() const { return m_editorCamera.get(); }

    // Toggle camera frustum visualization
    void SetShowCameraFrustum(bool show) { m_showCameraFrustum = show; }
    bool GetShowCameraFrustum() const { return m_showCameraFrustum; }

protected:
    void _renderCameraFrustum();
    void _drawFpsOverlay(const ImVec2& viewportPos, const ImVec2& viewportSize);
    ECS::RendererSystem* _getRendererSystem();
    void _handleEntityDragToMove();

    ECS::World* m_world = nullptr;
    EditorFileMenu* m_fileMenu = nullptr;

    // UI fonts
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // Editor camera (owned by viewport)
    std::unique_ptr<Editor::EditorCamera> m_editorCamera;

    // State
    EntityId m_selectedEntityId = ECS::Entity::NPOS32;
    bool m_isViewportHovered = false;

    // Stores the exact screen position and size of the drawn scene texture
    ImVec2 m_sceneDrawPos = { 0.0f, 0.0f };
    ImVec2 m_sceneDrawSize = { 0.0f, 0.0f };

    // Toggleable FPS overlay for viewports (editor-only)
    bool m_showSceneFpsOverlay = false;
    
    // Toggleable camera frustum visualization
    bool m_showCameraFrustum = true;  // Default: enabled
    
    // Editor-specific entity manipulation
    bool m_isDragging = false;
    glm::vec2 m_dragStartMouseWorld = {0, 0};
    glm::vec3 m_dragStartEntityPos = {0, 0, 0};
    Quaternion m_dragStartEntityRot;
    Vector3D m_dragStartEntityScale;
    uint32_t m_lastSelectedEntityID = ECS::Entity::NPOS32;
    bool m_wasMouseDownLastFrame = false;

    // Event callback
    std::function<void(EntityId)> m_onSelectionChanged;

    // Undo system
    Editor::UndoSystem* m_undoSystem = nullptr;

    // Message system subscriptions
    Messaging::SubscriptionHandle m_transformChangedSubscription;
    Messaging::SubscriptionHandle m_sceneModifiedSubscription;

    // Viewport type (Scene vs Game)
    ViewportType m_viewportType = ViewportType::Scene;
};

#endif // BASE_VIEWPORT_H
