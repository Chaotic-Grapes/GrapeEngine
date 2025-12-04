/* Start Header *****************************************************************/
/*!
\file   Viewport.h
\author Samantha Leong (50%)
        Foo Rui Qin    (50%)
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
#include "EditorCamera.h"
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

    // Set the currently selected entity programmatically (e.g. when selection
    // is changed from the hierarchy). This updates internal state and the
    // renderer's selected entity so the gizmo / outline appear.
    void SetSelectedEntity(EntityId id);

    // Undo System
    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }
    void FocusOnEntity(EntityId entityId);

    // Get viewport bounds for UI click detection
    ImVec2 GetSceneDrawPos() const { return m_sceneDrawPos; }
    ImVec2 GetSceneDrawSize() const { return m_sceneDrawSize; }

    // Get the editor camera
    Editor::EditorCamera* GetEditorCamera() { return m_editorCamera.get(); }

    // Toggle camera frustum visualization
    void SetShowCameraFrustum(bool show) { m_showCameraFrustum = show; }
    bool GetShowCameraFrustum() const { return m_showCameraFrustum; }

private:
    void _renderViewport();
    void _renderCameraFrustum();

    ECS::World* m_world = nullptr;
    EditorFileMenu* m_fileMenu = nullptr;

    // UI fonts
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // Editor camera (owned by viewport)
    std::unique_ptr<Editor::EditorCamera> m_editorCamera;

    // State
    EntityId m_selectedEntityId = 0;
    bool m_isViewportHovered = false;
    int m_activeTab = 0; // 0 = Scene, 1 = Game

    // Stores the exact screen position and size of the drawn scene texture. M3<<<<<<<<<<<<<<<<<<<<<<<
    ImVec2 m_sceneDrawPos = { 0.0f, 0.0f };
    ImVec2 m_sceneDrawSize = { 0.0f, 0.0f };

    // Toggleable FPS overlay for the Scene viewport (editor-only)
    void _drawFpsOverlay(const ImVec2& viewportPos, const ImVec2& viewportSize);
    bool m_showSceneFpsOverlay = false;
    
    // Toggleable camera frustum visualization
    bool m_showCameraFrustum = true;  // Default: enabled
    
    // Helper to access global RendererSystem from SystemManager
    ECS::RendererSystem* _getRendererSystem();
    
    // Editor-specific entity manipulation
    void _handleEntityDragToMove();
    bool m_isDragging = false;
    glm::vec2 m_dragStartMouseWorld = {0, 0};
    glm::vec3 m_dragStartEntityPos = {0, 0, 0};
    Quaternion m_dragStartEntityRot;
    Vector3D m_dragStartEntityScale;
    uint32_t m_lastSelectedEntityID = 0;
    bool m_wasMouseDownLastFrame = false;
    
    // Game window aspect ratio settings
    int m_selectedAspectRatio = 0; // Index into aspect ratio list
    bool m_freeAspect = true;      // Whether to use free aspect or fixed ratio

    // Event callback
    std::function<void(EntityId)> m_onSelectionChanged;

    // Undo system
    Editor::UndoSystem* m_undoSystem = nullptr;

    // Message system subscriptions
    Messaging::SubscriptionHandle m_transformChangedSubscription;
    Messaging::SubscriptionHandle m_sceneModifiedSubscription;
};

#endif // VIEWPORT_H