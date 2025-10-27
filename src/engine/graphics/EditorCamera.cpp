/* Start Header *****************************************************************/
/*!
\file   EditorCamera.cpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   24th October 2025
\brief
Implements a simplified EditorCamera for panning and zooming within the
editor viewport. Left-click drag pans the view, right-click drag orbits,
and scroll input zooms in/out.
*/
/* End Header *******************************************************************/

#define GLM_ENABLE_EXPERIMENTAL

#include "graphics/EditorCamera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

namespace Engine {

    // ============================================================================
    // Lifecycle
    // ============================================================================

    EditorCamera::EditorCamera(ECS::World& world)
        : m_cameraEntity(world.Create())    // Create an empty entity
    {
        // Attach components
        m_transform = &world.Add<ECS::Components::LocalTransform>(m_cameraEntity);
        m_camera = &world.Add<ECS::Components::Camera3D>(m_cameraEntity);

        // --- Default orthographic camera setup (pixel-space) ---
        m_camera->UsePerspective = false;

        auto* window = WindowManager::GetMainWindow();
        float width = static_cast<float>(window->Width());
        float height = static_cast<float>(window->Height());

        // Make 1 world unit = 1 pixel vertically
        m_camera->OrthoSize = height;
        m_camera->AspectRatio = width / height;
        // keep target in sync so we don't lerp toward 20 on startup
        m_targetOrthoSize = m_camera->OrthoSize;

        // Keep near/far wide enough for both 2D and 3D
        m_camera->NearPlane = 0.1f;
        m_camera->FarPlane = 1000.0f;

        m_camera->Active = true;

        // --- Position the camera slightly off the XY plane ---
        m_cameraPosition = glm::vec3(width * 0.5f, height * 0.5f, 10.0f);
        m_target = glm::vec3(width * 0.5f, height * 0.5f, 0.0f);
        m_distance = 10.0f; // Match initial Z offset

        std::cout << "[EditorCamera] Initialized\n";
    }

    EditorCamera::~EditorCamera() {
        std::cout << "[EditorCamera] Destroyed\n";
    }

    void EditorCamera::Update(float dt) {
        HandleInput(dt);
    }

    // ============================================================================
    // Input Handling
    // ============================================================================

    void EditorCamera::HandleInput(float dt) {
        // ------------------------------
        // BLOCK INPUT DURING RETURN-TO-ORTHO
        // ------------------------------
        if (m_returningToOrtho) {
            constexpr float returnSpeed = 5.0f;

            // Interpolate back to default orthographic state
            m_pitch = glm::mix(m_pitch, 0.0f, returnSpeed * dt);
            m_yaw = glm::mix(m_yaw, 0.0f, returnSpeed * dt);
            m_distance = glm::mix(m_distance, 10.0f, returnSpeed * dt);
            m_perspectiveBlend = glm::mix(m_perspectiveBlend, 0.0f, returnSpeed * dt);

            if (glm::abs(m_pitch) < glm::radians(0.1f) &&
                glm::abs(m_yaw) < glm::radians(0.1f) &&
                m_perspectiveBlend < 0.01f)
            {
                m_pitch = 0.0f;
                m_yaw = 0.0f;
                m_perspectiveBlend = 0.0f;
                m_returningToOrtho = false;
                std::cout << "[EditorCamera] Returned to flat orthographic view\n";
            }

            UpdateTransform();
            return; // Stop here (no other input processed this frame)
        }

        // ------------------------------
        // PANNING (LMB drag)
        // ------------------------------
        if (Input::IsMouseDown(MOUSE_LEFT)) {
            if (!m_panning) {
                m_panning = true;
                double px, py;
                Input::GetMousePosition(px, py);
                m_lastMousePos = glm::vec2(px, py);
                return;
            }

            double px, py;
            Input::GetMousePosition(px, py);
            const glm::vec2 currPan(px, py);
            const glm::vec2 delta = currPan - m_lastMousePos;
            m_lastMousePos = currPan;

            const auto window = WindowManager::GetMainWindow();
            const float worldPerPxY = m_camera->OrthoSize / static_cast<float>(window->Height());
            const float worldPerPxX = worldPerPxY * m_camera->AspectRatio;

            const glm::vec2 offset(-delta.x * worldPerPxX,
                delta.y * worldPerPxY);

            m_target.x += offset.x;
            m_target.y += offset.y;
            m_cameraPosition.x += offset.x;
            m_cameraPosition.y += offset.y;
        }
        else {
            m_panning = false;
        }

        // ------------------------------
        // ZOOM (mouse wheel - when not orbiting)
        // ------------------------------
        const double scrollY = Input::GetScrollY();

        // Only zoom ortho size if we're not orbiting
        if (scrollY != 0.0 && !m_orbiting) {
            constexpr float zoomFactor = 1.1f;

            if (scrollY > 0.0)
                m_targetOrthoSize /= zoomFactor;
            else
                m_targetOrthoSize *= zoomFactor;

            m_targetOrthoSize = glm::clamp(m_targetOrthoSize, 0.2f, 5000.0f);
        }

        constexpr float zoomLerpSpeed = 10.0f;
        m_camera->OrthoSize = glm::mix(m_camera->OrthoSize, m_targetOrthoSize, zoomLerpSpeed * dt);

        // ------------------------------
        // ORBIT (RMB drag)
        // ------------------------------
        const bool rightHeld = Input::IsMouseDown(MOUSE_RIGHT);

        if (rightHeld && !m_orbiting) {
            m_orbiting = true;
            double ox, oy;
            Input::GetMousePosition(ox, oy);
            m_lastMousePos = glm::vec2(ox, oy);
            return;
        }
        else if (!rightHeld && m_orbiting) {
            m_orbiting = false;
        }

        if (m_orbiting) {
            double ox, oy;
            Input::GetMousePosition(ox, oy);
            const glm::vec2 currOrbit(ox, oy);
            const glm::vec2 delta = currOrbit - m_lastMousePos;
            m_lastMousePos = currOrbit;

            constexpr float sensitivity = 0.005f;

            m_yaw += delta.x * sensitivity;
            m_pitch -= delta.y * sensitivity;

            // Wrap yaw so the camera doesn't spin excessively when resetting to ortho
            if (m_yaw > glm::pi<float>())
                m_yaw -= glm::two_pi<float>();
            else if (m_yaw < -glm::pi<float>())
                m_yaw += glm::two_pi<float>();

            constexpr float maxPitch = glm::radians(20.0f);
            m_pitch = glm::clamp(m_pitch, -maxPitch, maxPitch);
        }

        // ------------------------------
        // DOLLY (distance via scroll during orbit)
        // ------------------------------
        if (scrollY != 0.0 && m_orbiting) {
            m_distance -= static_cast<float>(scrollY) * 0.8f;
            m_distance = glm::max(m_distance, 1.0f);
        }

        // ------------------------------
        // RESET TO FLAT ORTHOGRAPHIC (KEY_O)
        // ------------------------------
        if (Input::IsKeyPressed(KEY_O)) {
            m_returningToOrtho = true;
        }

        // ------------------------------
        // SMOOTH BLEND: Interpolate between ortho/perspective based on rotation from flat view
        // ------------------------------
        // Calculate angular distance from default flat view (yaw=0, pitch=0)
        const float rotationMagnitude = glm::sqrt(m_pitch * m_pitch + m_yaw * m_yaw);

        constexpr float blendStart = glm::radians(0.5f);
        constexpr float blendEnd = glm::radians(5.0f);

        float targetBlend = glm::smoothstep(blendStart, blendEnd, rotationMagnitude);

        // Smooth interpolation of blend factor
        constexpr float blendSpeed = 8.0f;
        m_perspectiveBlend = glm::mix(m_perspectiveBlend, targetBlend, blendSpeed * dt);

        // ------------------------------
        // Depth range
        // ------------------------------
        m_camera->NearPlane = 0.1f;
        m_camera->FarPlane = std::max(1000.0f, m_distance * 50.0f);

        UpdateTransform();
    }

    // ============================================================================
    // Transform / Matrices
    // ============================================================================

    void EditorCamera::UpdateTransform() {
        // Canonical OpenGL forward: -Z when yaw=0, pitch=0
        const float cp = cosf(m_pitch), sp = sinf(m_pitch);
        const float sy = sinf(m_yaw), cy = cosf(m_yaw);

        // Orbit direction
        const glm::vec3 dir(cp * sy,  // x
            sp,         // y
            -cp * cy);  // z (note the minus)

        // Eye position on orbit sphere around target
        const glm::vec3 cameraPos = m_target - dir * m_distance;

        // ECS transform only cares about 2D position
        m_transform->Position.X = cameraPos.x;
        m_transform->Position.Y = cameraPos.y;

        // 2D rotation angle (around Z) for editor gizmos, etc.
        const float angle = atan2f(-dir.x, -dir.z);
        m_transform->Rotation = Quaternion::FromAxisAngle(Vector3D(0.f, 0.f, 1.f), angle);

        // Store eye for view matrix
        m_cameraPosition = cameraPos;
    }

    glm::mat4 EditorCamera::GetViewMatrix() const {
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        return glm::lookAt(m_cameraPosition, m_target, up);
    }

    glm::mat4 EditorCamera::GetProjectionMatrix() const {
        const float aspect = m_camera->AspectRatio;
        const float halfSize = m_camera->OrthoSize * 0.5f;

        if (m_perspectiveBlend < 0.001f) {
            // Pure orthographic
            return glm::ortho(
                -halfSize * aspect, halfSize * aspect,
                -halfSize, halfSize,
                m_camera->NearPlane, m_camera->FarPlane
            );
        }
        else if (m_perspectiveBlend > 0.999f) {
            // Pure perspective
            const float fovY = 2.0f * atan(m_camera->OrthoSize / (2.0f * m_distance));
            return glm::perspective(fovY, aspect, m_camera->NearPlane, m_camera->FarPlane);
        }
        else {
            // Blend between both projections
            glm::mat4 orthoProj = glm::ortho(
                -halfSize * aspect, halfSize * aspect,
                -halfSize, halfSize,
                m_camera->NearPlane, m_camera->FarPlane
            );

            const float fovY = 2.0f * atan(m_camera->OrthoSize / (2.0f * m_distance));
            glm::mat4 perspProj = glm::perspective(fovY, aspect, m_camera->NearPlane, m_camera->FarPlane);

            // Manual matrix interpolation
            glm::mat4 result;
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    result[i][j] = glm::mix(orthoProj[i][j], perspProj[i][j], m_perspectiveBlend);
                }
            }
            return result;
        }
    }

    glm::vec3 EditorCamera::GetPosition() const {
        return {
            m_transform->Position.X,
            m_transform->Position.Y,
            m_transform->Position.Z
        };
    }

    void EditorCamera::Focus(const glm::vec3& target) {
        m_transform->Position.X = target.x;
        m_transform->Position.Y = target.y;
    }

} // namespace Engine
