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
#include "ViewportInteractionManager.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "Math/Quaternion.h"
#include "Math/Vector3D.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <memory>
#include <functional>
#include <string>

// Forward declarations
namespace ECS { class RendererSystem; }
namespace Scenes { class SceneManager; }
namespace Editor { class EditorCamera; }

using EntityId = uint32_t;
class EditorFileMenu;

class BaseViewport {
public:
    /**
     * @brief Destroy viewport and release owned resources.
     */
    virtual ~BaseViewport();

    /**
     * @brief Distinguishes editor scene viewport from game viewport.
     */
    enum class ViewportType {
        Scene,
        Game
    };

    /**
     * @brief Set logical viewport role.
     * @param type Viewport type to apply.
     */
    void SetViewportType(ViewportType type) { m_viewportType = type; }

    /**
     * @brief Get current viewport role.
     * @return Current viewport type.
     */
    ViewportType GetViewportType() const { return m_viewportType; }

    /**
     * @brief Check whether this is the scene viewport.
     * @return True when viewport type is Scene.
     */
    bool IsSceneViewport() const { return m_viewportType == ViewportType::Scene; }

    /**
     * @brief Check whether this is the game viewport.
     * @return True when viewport type is Game.
     */
    bool IsGameViewport() const { return m_viewportType == ViewportType::Game; }

    /**
     * @brief Set user-facing viewport window name.
     * @param name Display name shown in editor UI.
     */
    void SetViewportName(const std::string& name) { m_viewportName = name; }

    /**
     * @brief Get user-facing viewport window name.
     * @return Viewport name string.
     */
    const std::string& GetViewportName() const { return m_viewportName; }

    /**
     * @brief Initialize shared viewport dependencies.
     * @param mainFont Primary text font.
     * @param boldFont Bold text font.
     * @param symbolsFont Symbol/icon font.
     * @param world Active ECS world.
     * @param sceneManager Scene manager used for scene operations.
     */
    virtual void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont,
        ECS::World* world, Scenes::SceneManager* sceneManager);

    /**
     * @brief Update world reference for viewport operations.
     * @param world New active ECS world.
     */
    virtual void SetWorld(ECS::World* world);

    /**
     * @brief Per-frame hook called before viewport-specific work.
     */
    virtual void BeginFrame() {}

    /**
     * @brief Handle in-world interactions for this viewport.
     */
    virtual void HandleInWorldInteraction() = 0;

    /**
     * @brief Render/editor UI windows associated with this viewport.
     */
    virtual void ShowEditorWindows() = 0;

    /**
     * @brief Per-frame hook called after viewport-specific work.
     */
    virtual void EndFrame() {}

    // Event registration
    /**
     * @brief Register callback for entity selection changes.
     * @param callback Function receiving selected entity id.
     */
    virtual void OnSelectionChanged(std::function<void(EntityId)> callback);

    /**
     * @brief Inject file menu dependency for viewport actions.
     * @param fileMenu File menu instance to use.
     */
    virtual void SetFileMenu(EditorFileMenu* fileMenu);

    // Accessors
    /**
     * @brief Get currently selected entity id.
     * @return Selected entity id, or null-equivalent when none.
     */
    EntityId GetSelectedEntityId() const;

    /**
     * @brief Check whether viewport window is currently hovered.
     * @return True when cursor is over viewport.
     */
    bool IsViewportHovered() const;

    /**
     * @brief Check whether viewport has a valid world pointer.
     * @return True when world pointer is set.
     */
    bool HasValidWorld() const { return m_world != nullptr; }

    // Set the currently selected entity programmatically (e.g. when selection
    // is changed from the hierarchy). This updates internal state and the
    // renderer's selected entity so the gizmo / outline appear.
    /**
     * @brief Set selected entity from external editor tools.
     * @param id Entity id to select.
     */
    virtual void SetSelectedEntity(EntityId id);

    // Undo System
    /**
     * @brief Attach undo system used by viewport interactions.
     * @param undoSystem Undo system instance.
     */
    void SetUndoSystem(Editor::UndoSystem* undoSystem) { m_undoSystem = undoSystem; }

    /**
     * @brief Move camera focus to a target entity.
     * @param entityId Entity id to frame in viewport.
     */
    virtual void FocusOnEntity(EntityId entityId);

    // Get viewport bounds for UI click detection
    /**
     * @brief Get top-left draw position of rendered scene texture.
     * @return Scene draw position in screen coordinates.
     */
    ImVec2 GetSceneDrawPos() const { return m_sceneDrawPos; }

    /**
     * @brief Get draw size of rendered scene texture.
     * @return Scene draw dimensions.
     */
    ImVec2 GetSceneDrawSize() const { return m_sceneDrawSize; }

    // Get the editor camera
    /**
     * @brief Access viewport-owned editor camera.
     * @return Pointer to editor camera, or null if uninitialized.
     */
    Editor::EditorCamera* GetEditorCamera() const { return m_editorCamera.get(); }

    // Toggle camera frustum visualization
    /**
     * @brief Enable or disable camera frustum visualization.
     * @param show True to draw frustum guides.
     */
    void SetShowCameraFrustum(bool show) { m_showCameraFrustum = show; }

    /**
     * @brief Query camera frustum visualization flag.
     * @return True when frustum visualization is enabled.
     */
    bool GetShowCameraFrustum() const { return m_showCameraFrustum; }

protected:
    /**
     * @brief Render world-space camera frustum overlay.
     */
    void _renderCameraFrustum();

    /**
     * @brief Draw FPS overlay inside viewport.
     * @param viewportPos Top-left viewport position.
     * @param viewportSize Viewport size in pixels.
     */
    void _drawFpsOverlay(const ImVec2& viewportPos, const ImVec2& viewportSize);

    /**
     * @brief Resolve renderer system from current world.
     * @return Pointer to renderer system, or null if unavailable.
     */
    ECS::RendererSystem* _getRendererSystem();

    ECS::World* m_world = nullptr;
    EditorFileMenu* m_fileMenu = nullptr;

    // UI fonts
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    // Editor camera (owned by viewport)
    std::unique_ptr<Editor::EditorCamera> m_editorCamera;

    // Interaction manager (owned by viewport)
    Editor::ViewportInteractionManager m_interactionMgr;

    // State
    ECS::Entity m_selectedEntity = ECS::NULL_ENTITY;  // Store full entity with generation to validate IsAlive()
    bool m_isViewportHovered = false;

    // Stores the exact screen position and size of the drawn scene texture
    ImVec2 m_sceneDrawPos = { 0.0f, 0.0f };
    ImVec2 m_sceneDrawSize = { 0.0f, 0.0f };

    // Toggleable FPS overlay for viewports (editor-only)
    bool m_showSceneFpsOverlay = false;
    
    // Toggleable camera frustum visualization
    bool m_showCameraFrustum = true;  // Default: enabled

    // Event callback
    std::function<void(EntityId)> m_onSelectionChanged;

    // Undo system
    Editor::UndoSystem* m_undoSystem = nullptr;

    // Message system subscriptions
    Messaging::SubscriptionHandle m_transformChangedSubscription;
    Messaging::SubscriptionHandle m_sceneModifiedSubscription;

    // Viewport type (Scene vs Game)
    ViewportType m_viewportType = ViewportType::Scene;
    std::string m_viewportName;
};

#endif // BASE_VIEWPORT_H
