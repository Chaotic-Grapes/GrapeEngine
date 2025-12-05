/* Start Header *****************************************************************/
/*!
\file   EditorCamera.cpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Implements a 3D EditorCamera for panning and zooming within the
editor viewport. Left-click drag pans the view, right-click drag orbits,
WASD keys move the camera, and scroll input zooms in/out.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#define GLM_ENABLE_EXPERIMENTAL

// Graphics
#include "EditorCamera.hpp"

// Services
#include "services/Input.h"
#include "services/WindowManager.h"

// Core systems
#include "core/messaging/MessageTypes.h"

// Third-Party Libraries
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Editor {

    EditorCamera::EditorCamera() {
        // Initialize camera with sensible defaults
        m_camera.Position = glm::vec3(0.0f, 5.0f, 10.0f);
        m_camera.Rotation = glm::quat(glm::vec3(-0.3f, 0.0f, 0.0f));
        m_camera.UsePerspective = false;  // Start with ortho
        m_camera.OrthoSize = 10.0f;
        m_camera.FOV = 45.0f;
        m_camera.NearPlane = 0.1f;
        m_camera.FarPlane = 1000.0f;

        // Set initial aspect ratio
        const auto& window = WindowManager::GetMainWindow();
        if (window) {
            m_camera.SetAspectRatio(
                static_cast<float>(window->GetWidth()),
                static_cast<float>(window->GetHeight())
            );
        }

        // Subscribe to resize events
        m_windowResizedSub = Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
            [this](const Messaging::WindowResized& e) {
                m_camera.SetAspectRatio(static_cast<float>(e.Width), static_cast<float>(e.Height));
            }
        );

        m_viewportResizedSub = Messaging::MessageSystem::Subscribe<Messaging::ViewportResized>(
            [this](const Messaging::ViewportResized& e) {
                m_camera.SetAspectRatio(e.Width, e.Height);
            }
        );
    }

    EditorCamera::~EditorCamera() {
        // Subscriptions auto-unsubscribe via RAII
    }

    void EditorCamera::Update(float deltaTime) {
        if (!m_allowInput) {
            return;
        }

        HandleKeyboardInput(deltaTime);
        HandleMouseInput(deltaTime);
        UpdateCameraPosition();
    }

    void EditorCamera::HandleKeyboardInput(float deltaTime) {
        const float moveSpeed = 5.0f;

        // WASD movement
        if (Input::IsKeyDown(KEY_W)) {
            m_target += m_camera.GetForward() * moveSpeed * deltaTime;
        }
        if (Input::IsKeyDown(KEY_S)) {
            m_target -= m_camera.GetForward() * moveSpeed * deltaTime;
        }
        if (Input::IsKeyDown(KEY_A)) {
            m_target -= m_camera.GetRight() * moveSpeed * deltaTime;
        }
        if (Input::IsKeyDown(KEY_D)) {
            m_target += m_camera.GetRight() * moveSpeed * deltaTime;
        }

        // Q/E for vertical movement
        if (Input::IsKeyDown(KEY_Q)) {
            m_target.y -= moveSpeed * deltaTime;
        }
        if (Input::IsKeyDown(KEY_E)) {
            m_target.y += moveSpeed * deltaTime;
        }
    }

    void EditorCamera::HandleMouseInput(float deltaTime) {
        double x, y;
        Input::GetMousePosition(x, y);
        glm::vec2 mousePos(static_cast<float>(x), static_cast<float>(y));
        
        if (m_firstMouse) {
            m_lastMousePos = mousePos;
            m_firstMouse = false;
        }

        glm::vec2 delta = mousePos - m_lastMousePos;
        m_lastMousePos = mousePos;

        // Right-click to orbit
        if (Input::IsMouseDown(MOUSE_RIGHT)) {
            const float rotationSpeed = 0.005f;
            m_yaw -= delta.x * rotationSpeed;
            m_pitch -= delta.y * rotationSpeed;

            // Clamp pitch to avoid gimbal lock
            m_pitch = glm::clamp(m_pitch, -glm::half_pi<float>() + 0.01f, glm::half_pi<float>() - 0.01f);
        }

        // Middle mouse or Alt+Left to pan
        if (Input::IsMouseDown(MOUSE_MIDDLE) ||
            (Input::IsKeyDown(KEY_LEFT_ALT) && Input::IsMouseDown(MOUSE_LEFT))) {
            const float panSpeed = 0.01f * m_distance;
            m_target -= m_camera.GetRight() * delta.x * panSpeed;
            m_target += m_camera.GetUp() * delta.y * panSpeed;
        }

        // Mouse wheel to zoom
        float scroll = static_cast<float>(Input::GetScrollY());
        if (scroll != 0.0f) {
            const float zoomSpeed = 0.1f;
            m_distance -= scroll * m_distance * zoomSpeed;
            m_distance = glm::clamp(m_distance, 0.1f, 1000.0f);
        }
    }

    void EditorCamera::UpdateCameraPosition() {
        // Calculate position based on orbit parameters
        glm::vec3 offset;
        offset.x = m_distance * cos(m_pitch) * sin(m_yaw);
        offset.y = m_distance * sin(m_pitch);
        offset.z = m_distance * cos(m_pitch) * cos(m_yaw);

        m_camera.Position = m_target + offset;
        m_camera.LookAt(m_target);
    }

    void EditorCamera::Focus(const glm::vec3& target) {
        m_target = target;
        UpdateCameraPosition();
    }

    void EditorCamera::SetViewportSize(float width, float height) {
        m_camera.SetAspectRatio(width, height);
    }

} // namespace Editor
