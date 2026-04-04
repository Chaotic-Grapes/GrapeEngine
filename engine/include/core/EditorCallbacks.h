/* Start Header *****************************************************************/
/*!
\file   EditorCallbacks.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Defines callback interfaces that allow the engine to communicate with editor
systems without directly depending on editor code. The engine exposes hook
points that the editor can implement to receive notifications and provide
editor-specific functionality.

Clean one-way dependency: Editor -> Engine (not Engine -> Editor)

Usage Pattern:
1. Engine defines callback interfaces (this file)
2. Engine provides registration methods for callbacks
3. Editor implements these callbacks
4. Editor registers callbacks during initialization
5. Engine invokes callbacks when needed

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef EDITORCALLBACKS_H
#define EDITORCALLBACKS_H

#include "Export.h"
#include <functional>
#include <memory>
#include <string>
#include <glm/glm.hpp>

// Forward declarations
namespace ECS {
    class World;
    struct Entity;
}

namespace Engine {

    // ========================================================================
    // Callback Function Types
    // ========================================================================

    /**
     * @brief Callback invoked when the engine wants to provide debug visualization data.
     * @param world Current ECS world
     * The editor can use this to render debug overlays (colliders, gizmos, etc.)
     */
    using DebugDrawCallback = std::function<void(ECS::World* world)>;

    /**
     * @brief Callback invoked when an entity should be selected in the editor.
     * @param entityId The entity to select (0 = clear selection)
     * @param addToSelection If true, add to existing selection (multi-select)
     */
    using EntitySelectionCallback = std::function<void(uint32_t entityId, bool addToSelection)>;

    /**
     * @brief Callback invoked when the inspector panel should refresh.
     * @param entityId The entity to inspect (0 = refresh current)
     */
    using InspectorRefreshCallback = std::function<void(uint32_t entityId)>;

    /**
     * @brief Callback invoked when the engine has rendered a frame to a texture.
     * @param textureId The OpenGL texture ID of the rendered scene
     * @param width Width of the rendered texture
     * @param height Height of the rendered texture
     * Editor can display this in viewport panels
     */
    using SceneRenderCallback = std::function<void(uint32_t textureId, int width, int height)>;

    /**
     * @brief Callback invoked when the engine wants to display a notification.
     * @param severity 0=Info, 1=Warning, 2=Error, 3=Success
     * @param title Notification title
     * @param message Notification message
     * @param duration Duration in seconds (0 = persistent)
     */
    using NotificationCallback = std::function<void(int severity, const std::string& title, 
                                                    const std::string& message, float duration)>;

    /**
     * @brief Callback invoked for viewport picking/raycasting.
     * @param screenX Mouse X position in screen coordinates
     * @param screenY Mouse Y position in screen coordinates
     * @return Entity ID of picked entity (0 = nothing picked)
     * Editor should perform raycast through its viewport camera
     */
    using PickCallback = std::function<uint32_t(float screenX, float screenY)>;

    /**
     * @brief Callback invoked when the engine needs editor camera information.
     * @param outPosition Output parameter for camera position
     * @param outViewMatrix Output parameter for view matrix
     * @param outProjectionMatrix Output parameter for projection matrix
     * @return true if editor camera is available, false otherwise
     */
    using EditorCameraCallback = std::function<bool(glm::vec3& outPosition, 
                                                    glm::mat4& outViewMatrix, 
                                                    glm::mat4& outProjectionMatrix)>;

    /**
     * @brief Callback invoked when a system wants to register custom ImGui windows.
     * Only called when USE_IMGUI is defined and editor is active.
     * Systems can use this to add debug panels, profiling windows, etc.
     */
    using CustomImGuiCallback = std::function<void()>;

    // ========================================================================
    // EditorCallbackRegistry
    // ========================================================================

    /**
     * @brief Central registry for editor callbacks.
     * 
     * The engine uses this to notify the editor without directly depending on it.
     * The editor registers its implementations during initialization.
     * 
     * Thread-safe: All callbacks are invoked from the main engine thread.
     */
    class GRAPEENGINE_API EditorCallbackRegistry {
    public:
        // Singleton access
        static EditorCallbackRegistry& Get();

        // ====================================================================
        // Registration Methods (Editor calls these)
        // ====================================================================

        /**
         * @brief Register the callback used for debug drawing in the editor viewport.
         * @param callback Callback receiving the current ECS world.
         */
        void RegisterDebugDraw(DebugDrawCallback callback) {
            m_debugDrawCallback = callback;
        }

        /**
         * @brief Register the callback used to update editor entity selection state.
         * @param callback Callback receiving entity id and multi-select flag.
         */
        void RegisterEntitySelection(EntitySelectionCallback callback) {
            m_entitySelectionCallback = callback;
        }

        /**
         * @brief Register the callback used to request inspector refreshes.
         * @param callback Callback receiving entity id to inspect.
         */
        void RegisterInspectorRefresh(InspectorRefreshCallback callback) {
            m_inspectorRefreshCallback = callback;
        }

        /**
         * @brief Register the callback used to present scene render output in editor UI.
         * @param callback Callback receiving texture id, width, and height.
         */
        void RegisterSceneRender(SceneRenderCallback callback) {
            m_sceneRenderCallback = callback;
        }

        /**
         * @brief Register the callback used to show editor notifications.
         * @param callback Callback receiving severity, title, message, and duration.
         */
        void RegisterNotification(NotificationCallback callback) {
            m_notificationCallback = callback;
        }

        /**
         * @brief Register the callback used for viewport picking queries.
         * @param callback Callback receiving screen coordinates and returning entity id.
         */
        void RegisterPick(PickCallback callback) {
            m_pickCallback = callback;
        }

        /**
         * @brief Register the callback used to query editor camera matrices.
         * @param callback Callback populating position, view, and projection matrices.
         */
        void RegisterEditorCamera(EditorCameraCallback callback) {
            m_editorCameraCallback = callback;
        }

        /**
         * @brief Register an extra ImGui draw callback invoked each editor frame.
         * @param callback Callback that renders additional ImGui content.
         */
        void RegisterCustomImGui(CustomImGuiCallback callback) {
            m_customImGuiCallbacks.push_back(callback);
        }

        // ====================================================================
        // Invocation Methods (Engine calls these)
        // ====================================================================

        /**
         * @brief Invoke the debug draw callback with the current ECS world.
         * @param world Current ECS world passed to the registered callback.
         */
        void InvokeDebugDraw(ECS::World* world) {
            if (m_debugDrawCallback) {
                m_debugDrawCallback(world);
            }
        }

        /**
         * @brief Notify editor that an entity selection change occurred.
         * @param entityId Entity to select (0 = clear selection).
         * @param addToSelection If true, add to existing selection (multi-select).
         */
        void InvokeEntitySelection(uint32_t entityId, bool addToSelection = false) {
            if (m_entitySelectionCallback) {
                m_entitySelectionCallback(entityId, addToSelection);
            }
        }

        /**
         * @brief Notify editor to refresh inspector views for one entity or full selection.
         * @param entityId Entity to inspect (0 = refresh current selection).
         */
        void InvokeInspectorRefresh(uint32_t entityId = 0) {
            if (m_inspectorRefreshCallback) {
                m_inspectorRefreshCallback(entityId);
            }
        }

        /**
         * @brief Forward the scene texture and frame dimensions to the editor rendering callback.
         * @param textureId OpenGL texture id of the rendered scene.
         * @param width Width of the rendered texture in pixels.
         * @param height Height of the rendered texture in pixels.
         */
        void InvokeSceneRender(uint32_t textureId, int width, int height) {
            if (m_sceneRenderCallback) {
                m_sceneRenderCallback(textureId, width, height);
            }
        }

        /**
         * @brief Notify the editor to display a notification message.
         * @param severity Severity level (0=Info, 1=Warning, 2=Error, 3=Success).
         * @param title Notification title string.
         * @param message Notification body text.
         * @param duration Display duration in seconds (0 = persistent).
         */
        void InvokeNotification(int severity, const std::string& title,
                               const std::string& message, float duration = 3.0f) {
            if (m_notificationCallback) {
                m_notificationCallback(severity, title, message, duration);
            }
        }

        /**
         * @brief Execute the picking callback and return the picked entity id.
         * @param screenX Mouse X position in screen coordinates.
         * @param screenY Mouse Y position in screen coordinates.
         * @return Entity id of the picked entity, or 0 when nothing is picked.
         */
        uint32_t InvokePick(float screenX, float screenY) {
            if (m_pickCallback) {
                return m_pickCallback(screenX, screenY);
            }
            return 0; // Nothing picked
        }

        /**
         * @brief Query editor camera data if an editor camera callback is registered.
         * @param outPosition Receives the editor camera world position.
         * @param outViewMatrix Receives the editor camera view matrix.
         * @param outProjectionMatrix Receives the editor camera projection matrix.
         * @return True if an editor camera callback was registered and succeeded.
         */
        bool InvokeEditorCamera(glm::vec3& outPosition, glm::mat4& outViewMatrix,
                               glm::mat4& outProjectionMatrix) {
            if (m_editorCameraCallback) {
                return m_editorCameraCallback(outPosition, outViewMatrix, outProjectionMatrix);
            }
            return false; // No editor camera available
        }

        /**
         * @brief Invoke all registered custom ImGui draw callbacks.
         */
        void InvokeCustomImGui() {
            for (auto& callback : m_customImGuiCallbacks) {
                if (callback) {
                    callback();
                }
            }
        }

        // ====================================================================
        // Utility Methods
        // ====================================================================

        /**
         * @brief Check if editor callbacks are registered (i.e., editor is active).
         * @return True when at least the entity selection callback is registered.
         */
        bool IsEditorActive() const {
            return m_entitySelectionCallback != nullptr;
        }

        /**
         * @brief Clear all registered callbacks (for shutdown)
         */
        void ClearAll() {
            m_debugDrawCallback = nullptr;
            m_entitySelectionCallback = nullptr;
            m_inspectorRefreshCallback = nullptr;
            m_sceneRenderCallback = nullptr;
            m_notificationCallback = nullptr;
            m_pickCallback = nullptr;
            m_editorCameraCallback = nullptr;
            m_customImGuiCallbacks.clear();
        }

    private:
        EditorCallbackRegistry() = default;
        ~EditorCallbackRegistry() = default;
        EditorCallbackRegistry(const EditorCallbackRegistry&) = delete;
        EditorCallbackRegistry& operator=(const EditorCallbackRegistry&) = delete;

        // Registered callbacks
        DebugDrawCallback m_debugDrawCallback;
        EntitySelectionCallback m_entitySelectionCallback;
        InspectorRefreshCallback m_inspectorRefreshCallback;
        SceneRenderCallback m_sceneRenderCallback;
        NotificationCallback m_notificationCallback;
        PickCallback m_pickCallback;
        EditorCameraCallback m_editorCameraCallback;
        std::vector<CustomImGuiCallback> m_customImGuiCallbacks;
    };

}

#endif
