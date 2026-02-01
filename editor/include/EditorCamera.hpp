/* Start Header *****************************************************************/
/*!
\file   EditorCamera.hpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Editor camera for viewport navigation and input handling.

Provides orbit/pan/dolly controls and a 2D orthographic mode that preserves
object Z positions so layering is maintained when switching between 2D/3D.

Controls:
 - Alt + LMB  : orbit around pivot
 - MMB        : pan
 - RMB + WASD : free-look (fly) + movement
 - Scroll     : zoom (dolly / ortho scale)
 - F          : focus/frame selection (editor should call FocusBounds)

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_CAMERA_HPP
#define EDITOR_CAMERA_HPP

#include "graphics/Camera.h"
#include "core/messaging/MessageSystem.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Editor {

    /**
     * @brief Editor camera wrapper used by editor viewports.
     *
     * This class wraps an `Engine::Camera` and implements editor-style
     * navigation (orbit, pan, dolly, and free-look). Input is only processed
     * when the viewport is focused via `SetViewportFocused`.
     *
     * The camera supports both perspective and orthographic (2D) views. The
     * 2D mode preserves object Z values so switching between 2D/3D won't
     * reproject objects onto different depths.
     */
    class EditorCamera {
    public:
        /**
         * @brief Construct a new EditorCamera
         *
         * Initializes sensible defaults and subscribes to window/viewport
         * resize messages to keep the camera aspect ratio up-to-date.
         */
        EditorCamera();

        /**
         * @brief Destroy the EditorCamera
         *
         * Unsubscribes from messaging (RAII unsubscription is expected
         * from the message system implementation).
         */
        ~EditorCamera();

        /**
         * @brief Begin frame processing - prepare input state
         * Call this at the start of the frame before systems update
         */
        void BeginFrame();

        /**
         * @brief Main update called once per frame by the editor viewport.
         *
         * Processes input (when viewport is focused), updates internal
         * orbit/target state and applies smoothing to the final camera
         * transform.
         *
         * @param deltaTime Elapsed time in seconds since last frame.
         */
        void Update(float deltaTime);

        /**
         * @brief End frame processing - finalize camera state for next frame
         * Call this at the end of the frame after rendering
         */
        void EndFrame();

        /**
         * @brief Set whether the editor viewport has input focus.
         *
         * When false, input is ignored and internal mouse state is reset so
         * the first mouse movement after focus returns does not jump.
         *
         * @param focused True if viewport is focused.
         */
        void SetViewportFocused(bool focused) { m_isViewportFocused = focused; }

        /**
         * @brief Update the viewport size used to compute the camera aspect.
         *
         * @param width Viewport width in pixels.
         * @param height Viewport height in pixels.
         */
        void SetViewportSize(float width, float height);

        /**
         * @brief Get a pointer to the underlying engine camera.
         *
         * The returned pointer is owned by this class.
         *
         * @return Engine::Camera* Pointer to the camera.
         */
        Engine::Camera* GetCamera() { return &m_camera; }
        const Engine::Camera* GetCamera() const { return &m_camera; }

        /**
         * @brief Focus the camera pivot on a specific world point.
         *
         * Recentres the orbit target at `worldPoint` and preserves the
         * current orbit distance where possible.
         *
         * @param worldPoint World-space point to focus on.
         */
        void Focus(const glm::vec3& worldPoint);

        /**
         * @brief Frame an axis-aligned bounding box.
         *
         * Computes the AABB center and radius and adjusts the camera's
         * orbit distance (perspective) or orthographic size (2D/ortho) so
         * the box comfortably fits the viewport.
         *
         * @param min Minimum corner of the AABB.
         * @param max Maximum corner of the AABB.
         */
        void FocusBounds(const glm::vec3& min, const glm::vec3& max);

        /**
         * @brief Switch camera to an orthographic top-down 2D mode.
         *
         * Preserves world-space Z of the camera position so object layering
         * is retained. The camera will look along -Z with an orthographic
         * projection.
         */
        void ResetTo2D();

        /**
         * @brief Switch camera to a 3D perspective mode (orbit-capable).
         *
         * Orbit parameters are computed from the current camera transform
         * and a sensible default is chosen if the position is near-zero.
         */
        void ResetTo3D();

        /**
         * @brief Toggle between 2D and 3D view modes.
         */
        void ToggleViewMode();

    private:
        // ====================================================================
        // Core Camera
        // ====================================================================
        Engine::Camera m_camera;
        bool m_isViewportFocused = false;
        bool m_is2DMode = false; // default: 3D by ResetTo3D in ctor
        float m_2DPlaneZ = 0.0f;

        // ====================================================================
        // Orbit / target data
        // ====================================================================
        glm::vec3 m_target = glm::vec3(0.0f);
        float m_distance = 8.0f; // orbit distance from target
        float m_yaw = 0.0f;      // radians
        float m_pitch = 0.0f;    // radians

        // ====================================================================
        // Input state
        // ====================================================================
        glm::vec2 m_lastMousePos = glm::vec2(0.0f);
        bool m_firstMouse = true;
        bool m_frameInputProcessed = false; // Track if input was processed this frame

        // ====================================================================
        // Movement & feel
        // ====================================================================
        float m_moveSpeed = 6.0f;
        float m_fastMoveSpeed = 16.0f;
        float m_orbitSpeed = 0.0035f;
        float m_panSpeed = 0.0016f;
        float m_zoomSpeed = 0.12f;

        // Smoothing
        glm::vec3 m_smoothPosition;
        glm::quat m_smoothRotation;
        float m_posLerpSpeed = 12.0f;
        float m_rotLerpSpeed = 18.0f;
        bool m_focusActive = false;
        glm::vec3 m_focusTarget = glm::vec3(0.0f);
        float m_focusDistance = 0.0f;
        float m_focusOrthoSize = 0.0f;
        float m_focusLerpSpeed = 10.0f;
        bool m_hadNavigationInput = false;

        // Constraints
        float m_minDistance = 0.2f;
        float m_maxDistance = 1000.0f;
        float m_minOrthoSize = 0.01f;
        float m_maxOrthoSize = 10000.0f;

        // Messaging subscriptions (viewport/window resizing)
        Messaging::SubscriptionHandle m_windowResizedSub;
        Messaging::SubscriptionHandle m_viewportResizedSub;

        // ====================================================================
        // Internal handlers
        // ====================================================================
        
        // Compute mouse movement delta since last frame.
        void _handleMouseDelta(glm::vec2& outDelta);

        // Handle keyboard-only movement (WASD/QE).
        // In 2D mode this performs panning (left/right/up/down). 
        // In 3D mode this performs FPS-style movement along the camera axes.
        void _handleMovementKeys(float dt);

        // High level input processing that routes to specific handlers.
        void _handleInput(float dt, const glm::vec2& mouseDelta);

        // Free-look (fly) behaviour when RMB is held.
        void _handleFly(float dt, const glm::vec2& delta);   // RMB free-look + WASD

        // Orbit the camera pivot using Alt + LMB drag.
        void _handleOrbit(const glm::vec2& delta);           // Alt + LMB

        // Pan the camera/pivot in the camera plane (MMB).
        void _handlePan(const glm::vec2& delta);             // MMB

        // Zoom/dolly the camera either by scroll or Alt+RMB drag.
        void _handleZoom(float scrollOrDelta);               // scroll wheel or Alt+RMB drag

        // Recompute camera world position from yaw/pitch/distance
        void _updateOrbitPosition();                         // recompute position from yaw/pitch/distance

        // Smooth the camera's position and rotation towards target transforms.
        void _applySmoothing(float dt);                      // lerp position/rotation for smoothness
        void _apply2DTargetToCamera();
        void _applyFocusSmoothing(float dt);
        float _getSpeedScale() const;

        // Non-copyable
        EditorCamera(const EditorCamera&) = delete;
        EditorCamera& operator=(const EditorCamera&) = delete;
    };

} // namespace Editor

#endif // EDITOR_CAMERA_HPP
