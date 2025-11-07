/* Start Header *****************************************************************/
/*!
\file   EditorCamera.cpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Implements a simplified EditorCamera for panning and zooming within the
editor viewport. Left-click drag pans the view, right-click drag orbits,
and scroll input zooms in/out.
*/
/* End Header *******************************************************************/

#define GLM_ENABLE_EXPERIMENTAL

// Graphics
#include "graphics/EditorCamera.hpp"
#include "graphics/graphicsConfig.hpp"

// Core systems
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"

// Third-Party Libraries
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

// Standard Library
#include <iostream>

namespace {
    // Camera initialization constants
    constexpr float kDefaultWorldViewHeight = 9.0f;
    constexpr float kDefaultDistance = 10.0f;
    constexpr float kDefaultNearPlane = 0.1f;
    constexpr float kDefaultFarPlane = 1000.0f;
    constexpr float kDefaultAspectRatio = 16.0f / 9.0f;

    // Return to ortho constants
    constexpr float kReturnToOrthoSpeed = 5.0f;
    constexpr float kReturnToOrthoThresholdDegrees = 0.1f;
    constexpr float kReturnToOrthoBlendThreshold = 0.01f;

    // Panning constants
    constexpr float kPanningMinimumDelta = 0.001f;

    // Zoom constants
    constexpr float kZoomFactor = 1.1f;
    constexpr float kMinOrthoSize = 0.2f;
    constexpr float kMaxOrthoSize = 5000.0f;
    constexpr float kZoomLerpSpeed = 10.0f;

    // Orbit constants
    constexpr float kOrbitSensitivity = 0.005f;
    constexpr float kMaxPitchDegrees = 20.0f;
    constexpr float kMinDistance = 1.0f;
    constexpr float kDollySpeed = 0.8f;

    // Blend constants (degrees)
    constexpr float kBlendStartDegrees = 0.5f;
    constexpr float kBlendEndDegrees = 5.0f;
    constexpr float kBlendSpeed = 8.0f;
    constexpr float kPureOrthoThreshold = 0.001f;
    constexpr float kPurePerspectiveThreshold = 0.999f;

    // Far plane scaling
    constexpr float kFarPlaneDistanceMultiplier = 50.0f;
} // anonymous namespace

namespace Engine {
    // ============================================================================
    // Lifecycle
    // ============================================================================
    EditorCamera::EditorCamera(ECS::World& world)
        : m_cameraEntity(world.Create())
    {
        m_transform = &world.Add<ECS::Components::LocalTransform>(m_cameraEntity);
        m_camera = &world.Add<ECS::Components::Camera3D>(m_cameraEntity);

        m_camera->UsePerspective = false;

        auto* window = WindowManager::GetMainWindow();
        const float screenWidth = static_cast<float>(window->Width());
        const float screenHeight = static_cast<float>(window->Height());

        // Set orthographic size in world units
        m_camera->OrthoSize = kDefaultWorldViewHeight;
        m_camera->AspectRatio = screenWidth / screenHeight;
        m_targetOrthoSize = m_camera->OrthoSize;

        // Position camera at world origin
        m_cameraPosition = glm::vec3(0.0f, 0.0f, kDefaultDistance);
        m_target = glm::vec3(0.0f, 0.0f, 0.0f);
        m_distance = kDefaultDistance;

        m_camera->NearPlane = kDefaultNearPlane;
        m_camera->FarPlane = kDefaultFarPlane;
        m_camera->Active = true;

        std::cout << "[EditorCamera] Initialized\n"
            << "  Scale: 1 world unit = " << graphicsConfig::PIXELS_PER_WORLD_UNIT << " pixels\n"
            << "  World viewport: " << kDefaultWorldViewHeight << " units tall\n";

        // Subscribe to window resize events
        Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
            [this](const Messaging::WindowResized& msg)
            {
                OnWindowResize(msg.Width, msg.Height);
            });
    }

    EditorCamera::~EditorCamera() {
        std::cout << "[EditorCamera] Destroyed\n";
    }

    void EditorCamera::OnWindowResize(int newWidth, int newHeight) {
        const float screenWidth = static_cast<float>(newWidth);
        const float screenHeight = static_cast<float>(newHeight);

        m_camera->AspectRatio = kDefaultAspectRatio;
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
            // Interpolate back to default orthographic state
            m_pitch = glm::mix(m_pitch, 0.0f, kReturnToOrthoSpeed * dt);
            m_yaw = glm::mix(m_yaw, 0.0f, kReturnToOrthoSpeed * dt);
            m_distance = glm::mix(m_distance, kDefaultDistance, kReturnToOrthoSpeed * dt);
            m_perspectiveBlend = glm::mix(m_perspectiveBlend, 0.0f, kReturnToOrthoSpeed * dt);

            if (glm::abs(m_pitch) < glm::radians(kReturnToOrthoThresholdDegrees) &&
                glm::abs(m_yaw) < glm::radians(kReturnToOrthoThresholdDegrees) &&
                m_perspectiveBlend < kReturnToOrthoBlendThreshold)
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
        if (Input::IsMouseDown(MOUSE_MIDDLE)) {
            if (!m_panning) {
                m_panning = true;
                double px, py;
                Input::GetMousePosition(px, py);
                m_lastMousePos = glm::vec2(px, py);
                return; // Don't pan on the first frame of the drag
            }

            double px, py;
            Input::GetMousePosition(px, py);
            const glm::vec2 currPan(px, py);
            const glm::vec2 delta = currPan - m_lastMousePos;
            m_lastMousePos = currPan;

            // Don't pan if there's no movement
            if (glm::length(delta) < kPanningMinimumDelta)
                return;

            const auto window = WindowManager::GetMainWindow();

            // World units visible on screen vertically
            const float worldHeight = m_camera->OrthoSize;
            const float worldWidth = worldHeight * m_camera->AspectRatio;

            // Screen dimensions in pixels
            const float screenHeight = static_cast<float>(window->Height());
            const float screenWidth = static_cast<float>(window->Width());

            // Convert pixel delta to world delta
            const float worldDeltaX = -(delta.x / screenWidth) * worldWidth;
            const float worldDeltaY = (delta.y / screenHeight) * worldHeight;

            // Apply panning offset
            m_target.x += worldDeltaX;
            m_target.y += worldDeltaY;
            m_cameraPosition.x += worldDeltaX;
            m_cameraPosition.y += worldDeltaY;
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
            if (scrollY > 0.0)
                m_targetOrthoSize /= kZoomFactor;
            else
                m_targetOrthoSize *= kZoomFactor;

            m_targetOrthoSize = glm::clamp(m_targetOrthoSize, kMinOrthoSize, kMaxOrthoSize);
        }

        m_camera->OrthoSize = glm::mix(m_camera->OrthoSize, m_targetOrthoSize, kZoomLerpSpeed * dt);

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

            m_yaw += delta.x * kOrbitSensitivity;
            m_pitch -= delta.y * kOrbitSensitivity;

            // Wrap yaw so the camera doesn't spin excessively when resetting to ortho
            if (m_yaw > glm::pi<float>())
                m_yaw -= glm::two_pi<float>();
            else if (m_yaw < -glm::pi<float>())
                m_yaw += glm::two_pi<float>();

            constexpr float maxPitch = glm::radians(kMaxPitchDegrees);
            m_pitch = glm::clamp(m_pitch, -maxPitch, maxPitch);
        }

        // ------------------------------
        // DOLLY (distance via scroll during orbit)
        // ------------------------------
        if (scrollY != 0.0 && m_orbiting) {
            m_distance -= static_cast<float>(scrollY) * kDollySpeed;
            m_distance = glm::max(m_distance, kMinDistance);
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

        constexpr float blendStart = glm::radians(kBlendStartDegrees);
        constexpr float blendEnd = glm::radians(kBlendEndDegrees);

        float targetBlend = glm::smoothstep(blendStart, blendEnd, rotationMagnitude);

        // Smooth interpolation of blend factor
        m_perspectiveBlend = glm::mix(m_perspectiveBlend, targetBlend, kBlendSpeed * dt);

        // ------------------------------
        // Depth range
        // ------------------------------
        m_camera->NearPlane = kDefaultNearPlane;
        m_camera->FarPlane = std::max(kDefaultFarPlane, m_distance * kFarPlaneDistanceMultiplier);

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
        const glm::vec3 dir(cp * sy,    // x
            sp,                         // y
            -cp * cy);                  // z (note the minus)

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

        if (m_perspectiveBlend < kPureOrthoThreshold) {
            // Pure orthographic
            return glm::ortho(
                -halfSize * aspect, halfSize * aspect,
                -halfSize, halfSize,
                m_camera->NearPlane, m_camera->FarPlane
            );
        }
        else if (m_perspectiveBlend > kPurePerspectiveThreshold) {
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
