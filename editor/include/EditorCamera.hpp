/* Start Header *****************************************************************/
/*!
\file   EditorCamera.hpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Editor-specific camera wrapper with input handling.
This is NOT part of the engine - it's an editor tool.

The EditorCamera wraps a standalone Engine::Camera and adds editor-specific
input handling (WASD movement, mouse orbit/pan, zoom, etc.).

Responsibilities:
- Handle editor camera input (keyboard, mouse)
- Maintain orbit target and distance
- Provide smooth camera transitions
- Save/restore editor camera state

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_CAMERA_H
#define EDITOR_CAMERA_H

#include "graphics/Camera.h"
#include "core/messaging/MessageSystem.h"
#include <glm/glm.hpp>

namespace Editor {

    /**
     * @brief Editor camera with input handling
     * 
     * This is an editor-only tool that wraps Engine::Camera.
     * Does NOT use ECS - purely standalone for editor viewports.
     */
    class EditorCamera {
    public:
        EditorCamera();
        ~EditorCamera();

        /**
         * @brief Update camera based on input
         * @param deltaTime Time since last frame
         */
        void Update(float deltaTime);

        /**
         * @brief Enable/disable input processing
         * @param allow Whether to process input
         */
        void SetAllowInput(bool allow) { m_allowInput = allow; }

        /**
         * @brief Get the underlying camera for rendering
         * @return Pointer to the standalone camera
         */
        Engine::Camera* GetCamera() { return &m_camera; }
        const Engine::Camera* GetCamera() const { return &m_camera; }

        /**
         * @brief Focus camera on a specific point
         * @param target World position to focus on
         */
        void Focus(const glm::vec3& target);

        /**
         * @brief Update viewport size for aspect ratio
         * @param width Viewport width
         * @param height Viewport height
         */
        void SetViewportSize(float width, float height);

        /**
         * @brief Set camera to orthographic or perspective
         * @param perspective True for perspective, false for ortho
         */
        void SetPerspective(bool perspective) { m_camera.UsePerspective = perspective; }

        /**
         * @brief Get current orbit target
         * @return Target position
         */
        const glm::vec3& GetTarget() const { return m_target; }

        /**
         * @brief Set orbit target
         * @param target New target position
         */
        void SetTarget(const glm::vec3& target) { m_target = target; }

    private:
        // Core camera data
        Engine::Camera m_camera;

        // Orbit controls
        glm::vec3 m_target = glm::vec3(0.0f, 0.0f, 0.0f);
        float m_distance = 10.0f;
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;

        // Input state
        glm::vec2 m_lastMousePos = glm::vec2(0.0f);
        bool m_firstMouse = true;
        bool m_allowInput = true;

        // Message subscriptions
        Messaging::SubscriptionHandle m_windowResizedSub;
        Messaging::SubscriptionHandle m_viewportResizedSub;

        // Input handling
        void HandleKeyboardInput(float deltaTime);
        void HandleMouseInput(float deltaTime);
        void UpdateCameraPosition();

        // Disable copy/move
        EditorCamera(const EditorCamera&) = delete;
        EditorCamera& operator=(const EditorCamera&) = delete;
    };

} // namespace Editor

#endif // EDITOR_CAMERA_H
