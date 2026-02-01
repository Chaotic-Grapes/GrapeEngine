/* Start Header *****************************************************************/
/*!
\file   EditorCamera.cpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Implementation for the editor viewport camera.

Implements the navigation behaviours declared in `EditorCamera.hpp`.
The implementation handles platform input, smoothing, and viewport resize messages.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#define GLM_ENABLE_EXPERIMENTAL

#include "EditorCamera.hpp"

#include "services/Input.h"
#include "core/Application.h"
#include "platform/IPlatformContext.h"
#include "core/messaging/MessageTypes.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace Editor {

    static constexpr float RAD_CLAMP = 1.55334303f; // ~89 degrees in radians

    EditorCamera::EditorCamera() {
        // Sensible defaults
        m_camera.FOV = 45.0f;
        m_camera.NearPlane = 0.1f;
        m_camera.FarPlane = 10000.0f;
        m_camera.OrthoSize = 10.0f;

        // Initialise to 2D by default
        ResetTo2D();

        // smoothing initial state
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;

        // Set initial aspect using platform window if available
        auto* context = Engine::CORE->GetPlatformContext();
        if (context) {
            if (auto* win = context->GetMainWindow()) {
                m_camera.SetAspectRatio(static_cast<float>(win->GetWidth()),
                                        static_cast<float>(win->GetHeight()));
            }
        }

        // Subscribe to resize messages
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

    EditorCamera::~EditorCamera() { } // RAII unsubscription assumed

    void EditorCamera::BeginFrame() {
        // Reset frame input flag at the start of frame
        m_frameInputProcessed = false;
        
        // If viewport is not focused, reset mouse tracking
        if (!m_isViewportFocused) {
            m_firstMouse = true;
        }
    }

    void EditorCamera::Update(float dt) {
        if (!m_isViewportFocused) {
            // still keep mouse-first flag reset so mouse delta won't jump when focus returns
            m_firstMouse = true;
            return;
        }

        // Prevent duplicate input processing in the same frame
        if (m_frameInputProcessed) {
            return;
        }

        // Get mouse delta
        glm::vec2 mouseDelta(0.0f);
        _handleMouseDelta(mouseDelta);

        m_hadNavigationInput = false;

        // Process input
        _handleInput(dt, mouseDelta);

        // Cancel focus smoothing on direct navigation input.
        if (m_hadNavigationInput) {
            m_focusActive = false;
        } else {
            _applyFocusSmoothing(dt);
        }

        // Apply smoothing to camera transform
        _applySmoothing(dt);
        
        // Mark input as processed for this frame
        m_frameInputProcessed = true;
    }

    void EditorCamera::EndFrame() {
        // Finalize camera frustum for next frame's culling
        // The camera matrices are already updated in Update(), but we can
        // use this to perform any cleanup or state validation
        
        // Future: Could cache frustum planes here for next frame's culling
    }

    void EditorCamera::_handleMouseDelta(glm::vec2& outDelta) {
        double mx = 0.0, my = 0.0;
        Input::GetMousePosition(mx, my);
        glm::vec2 mousePos(static_cast<float>(mx), static_cast<float>(my));

        // First frame after focus: initialize last mouse position
        if (m_firstMouse) {
            m_lastMousePos = mousePos;
            m_firstMouse = false;
        }

        // Compute delta
        outDelta = mousePos - m_lastMousePos;
        m_lastMousePos = mousePos;
    }

    void EditorCamera::_handleInput(float dt, const glm::vec2& mouseDelta) {
        // Input queries
        const bool alt = Input::IsKeyDown(KEY_LEFT_ALT) || Input::IsKeyDown(KEY_RIGHT_ALT);
        const bool lmb = Input::IsMouseDown(MOUSE_LEFT);
        const bool mmb = Input::IsMouseDown(MOUSE_MIDDLE);
        const bool rmb = Input::IsMouseDown(MOUSE_RIGHT);
        const float scroll = static_cast<float>(Input::GetScrollY()); // vertical scroll

        // Shortcut keys (toggle modes)
        if (Input::IsKeyPressed(KEY_2)) { ResetTo2D(); }
        if (Input::IsKeyPressed(KEY_3)) { ResetTo3D(); }
        if (Input::IsKeyPressed(KEY_V)) { ToggleViewMode(); } // optional

        // Priority: Free-look (RMB), Orbit (Alt+LMB), Pan (MMB), Alt+RMB zoom drag
        if (rmb) {
            // Free-look (fly) mode: rotate camera by mouse and allow WASD movement
            m_hadNavigationInput = true;
            _handleFly(dt, mouseDelta);
        } else if (alt && lmb) {
            // Orbit around target
            m_hadNavigationInput = true;
            _handleOrbit(mouseDelta);
        } else if (mmb) {
            // Pan in camera plane
            m_hadNavigationInput = true;
            _handlePan(mouseDelta);
        } else if (alt && Input::IsMouseDown(MOUSE_RIGHT) && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)) {
            // Alt + RMB drag for zoom/dolly (vertical drag)
            m_hadNavigationInput = true;
            _handleZoom(mouseDelta.y * 0.01f);
        }
        
            // Handle movement keys (WASD/QE): in 2D they pan the camera, in 3D
            // they act as FPS movement. Do not call when RMB is held because
            // HandleFly already moves the camera in that case.
            if (!rmb) {
                _handleMovementKeys(dt);
            }

        // Always process scroll zoom (works both for ortho and perspective)
        if (scroll != 0.0f) {
            m_hadNavigationInput = true;
            _handleZoom(scroll);
        }
    }

    void EditorCamera::_handleFly(float dt, const glm::vec2& delta) {
        // Rotation from mouse delta
        const float lookSpeed = 0.0025f;
        m_yaw   -= delta.x * lookSpeed;
        m_pitch -= delta.y * lookSpeed;
        m_pitch = glm::clamp(m_pitch, -RAD_CLAMP, RAD_CLAMP);

        // Build rotation quaternion (pitch around X, yaw around Y)
        glm::quat rot = glm::quat(glm::vec3(m_pitch, m_yaw, 0.0f));
        m_camera.Rotation = rot;

        // Movement in camera local space
        glm::vec3 forward = m_camera.GetForward();
        glm::vec3 right   = m_camera.GetRight();
        glm::vec3 up      = m_camera.GetUp();

        // Movement speed
        float speed = Input::IsKeyDown(KEY_LEFT_SHIFT) ? m_fastMoveSpeed : m_moveSpeed;
        speed *= _getSpeedScale();

        // 3D FPS-style movement along camera axes
        if (Input::IsKeyDown(KEY_W)) m_camera.Position += forward * speed * dt;
        if (Input::IsKeyDown(KEY_S)) m_camera.Position -= forward * speed * dt;
        if (Input::IsKeyDown(KEY_A)) m_camera.Position -= right   * speed * dt;
        if (Input::IsKeyDown(KEY_D)) m_camera.Position += right   * speed * dt;
        if (Input::IsKeyDown(KEY_Q)) m_camera.Position -= up      * speed * dt;
        if (Input::IsKeyDown(KEY_E)) m_camera.Position += up      * speed * dt;

        // Keep orbit target in front of camera so toggling back to orbit behaves sensibly
        m_target = m_camera.Position + forward * m_distance;
    }

    void EditorCamera::_handleMovementKeys(float dt) {
        // 2D orthographic: WASD pans the target/camera in XY
        if (m_is2DMode || !m_camera.UsePerspective) {
            float panScale = m_camera.OrthoSize * m_panSpeed;
            float speedMod = Input::IsKeyDown(KEY_LEFT_SHIFT) ? 4.0f : 1.0f;

            // Compute right and up vectors
            glm::vec3 right = m_camera.GetRight();
            glm::vec3 up = m_camera.GetUp();
            glm::vec3 delta(0.0f);

            // Accumulate pan delta
            if (Input::IsKeyDown(KEY_W)) delta += up;
            if (Input::IsKeyDown(KEY_S)) delta -= up;
            if (Input::IsKeyDown(KEY_A)) delta -= right;
            if (Input::IsKeyDown(KEY_D)) delta += right;

            // Apply pan if any movement
            if (glm::length(delta) > 0.0f) {
                delta = glm::normalize(delta);
                m_target += delta * panScale * speedMod;
                m_hadNavigationInput = true;

                _apply2DTargetToCamera();
            }
            return;
        }

        // 3D FPS-style movement along camera axes
        glm::vec3 forward = m_camera.GetForward();
        glm::vec3 right = m_camera.GetRight();
        glm::vec3 up = m_camera.GetUp();

        // Movement speed
        float speed = Input::IsKeyDown(KEY_LEFT_SHIFT) ? m_fastMoveSpeed : m_moveSpeed;
        speed *= _getSpeedScale();
        bool moved = false;

        // 3D movement
        if (Input::IsKeyDown(KEY_W)) m_camera.Position += forward * speed * dt;
        if (Input::IsKeyDown(KEY_S)) m_camera.Position -= forward * speed * dt;
        if (Input::IsKeyDown(KEY_A)) m_camera.Position -= right   * speed * dt;
        if (Input::IsKeyDown(KEY_D)) m_camera.Position += right   * speed * dt;
        if (Input::IsKeyDown(KEY_Q)) m_camera.Position -= up      * speed * dt;
        if (Input::IsKeyDown(KEY_E)) m_camera.Position += up      * speed * dt;
        moved = Input::IsKeyDown(KEY_W) || Input::IsKeyDown(KEY_S) || Input::IsKeyDown(KEY_A)
            || Input::IsKeyDown(KEY_D) || Input::IsKeyDown(KEY_Q) || Input::IsKeyDown(KEY_E);
        if (moved) {
            m_hadNavigationInput = true;
        }

        // Keep orbit target consistent with camera
        m_target = m_camera.Position + forward * m_distance;
    }

    void EditorCamera::_handleOrbit(const glm::vec2& delta) {
        // Orbit speed scales with distance so controls feel consistent at different ranges
        float distanceFactor = glm::max(1.0f, m_distance);
        float yawDelta = delta.x * m_orbitSpeed * (distanceFactor * 0.02f);
        float pitchDelta = delta.y * m_orbitSpeed * (distanceFactor * 0.02f);

        m_yaw   -= yawDelta;
        m_pitch -= pitchDelta;
        m_pitch = glm::clamp(m_pitch, -RAD_CLAMP, RAD_CLAMP);

        _updateOrbitPosition();
    }

    void EditorCamera::_handlePan(const glm::vec2& delta) {
        // Pan along the camera's right and up vectors so Z-depth is respected
        // Pan amount scales with distance for perspective and with ortho size for orthographic
        float panScale = 1.0f;
        if (!m_is2DMode) {
            panScale = m_distance * m_panSpeed;
        } else {
            // scale with orthographic size so panning remains intuitive
            panScale = m_camera.OrthoSize * m_panSpeed;
        }

        // Compute right and up vectors
        glm::vec3 right = m_camera.GetRight();
        glm::vec3 up    = m_camera.GetUp();

        // Apply pan
        m_target -= right * delta.x * panScale;
        m_target += up    * delta.y * panScale;

        if (m_is2DMode || !m_camera.UsePerspective) {
            _apply2DTargetToCamera();
        } else {
            _updateOrbitPosition();
        }
    }

    void EditorCamera::_handleZoom(float scrollOrDelta) {
        if (scrollOrDelta == 0.0f) return;

        if (m_is2DMode || !m_camera.UsePerspective) {
            // Orthographic zoom: scale ortho size
            float factor = 1.0f - (scrollOrDelta * m_zoomSpeed);
            
            factor = glm::max(0.001f, factor);
            m_camera.OrthoSize *= factor;
            m_camera.OrthoSize = glm::clamp(m_camera.OrthoSize, m_minOrthoSize, m_maxOrthoSize);
        } else {
            // Perspective dolly: move camera toward/away from target along forward vector
            // Use multiplicative change so zoom feels consistent at different distances
            float factor = 1.0f - (scrollOrDelta * m_zoomSpeed);

            // Prevent inversion or negative distance
            factor = glm::max(0.001f, factor);
            m_distance *= factor;
            m_distance = glm::clamp(m_distance, m_minDistance, m_maxDistance);

            _updateOrbitPosition();
        }
    }

    void EditorCamera::_updateOrbitPosition() {
        // Compute offset in camera space using spherical coordinates
        glm::vec3 offset;
        offset.x = m_distance * cosf(m_pitch) * sinf(m_yaw);
        offset.y = m_distance * sinf(m_pitch);
        offset.z = m_distance * cosf(m_pitch) * cosf(m_yaw);

        m_camera.Position = m_target + offset;

        // Look at target - robustly compute rotation using quaternion lookAt
        glm::vec3 forward = glm::normalize(m_target - m_camera.Position);
        if (glm::length(forward) < 1e-6f) {
            // degenerate; do nothing
            return;
        }
        // Use engine Camera's LookAt to ensure the same rotation / basis is used
        // (Camera::LookAt builds a rotation that maps local -Z to the direction).
        m_camera.LookAt(m_target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void EditorCamera::_applySmoothing(float dt) {
        if (dt <= 0.0f) return;

        // Position
        float alphaPos = glm::clamp(dt * m_posLerpSpeed, 0.0f, 1.0f);
        m_smoothPosition = glm::mix(m_smoothPosition, m_camera.Position, alphaPos);

        // Rotation
        float alphaRot = glm::clamp(dt * m_rotLerpSpeed, 0.0f, 1.0f);
        m_smoothRotation = glm::slerp(m_smoothRotation, m_camera.Rotation, alphaRot);

        // Apply smoothed transforms to camera
        m_camera.Position = m_smoothPosition;
        m_camera.Rotation = m_smoothRotation;
    }

    void EditorCamera::Focus(const glm::vec3& worldPoint) {
        // Move pivot to worldPoint and maintain distance (or adjust if too small)
        glm::vec3 desiredTarget = worldPoint;
        if (m_is2DMode || !m_camera.UsePerspective) {
            desiredTarget.z = m_2DPlaneZ;
        }

        // If current distance is tiny, pick a sensible default
        float desiredDistance = m_distance;
        if (desiredDistance < 0.001f) {
            desiredDistance = 8.0f;
        }

        // Recompute yaw/pitch from position->target
        m_focusTarget = desiredTarget;
        m_focusDistance = desiredDistance;
        m_focusOrthoSize = m_camera.OrthoSize;
        m_focusActive = true;
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;
    }

    void EditorCamera::FocusBounds(const glm::vec3& min, const glm::vec3& max) {
        // Compute centre & radius of AABB
        glm::vec3 centre = (min + max) * 0.5f;
        glm::vec3 extents = (max - min) * 0.5f;
        float radius = glm::length(extents);
        // Use viewport aspect so framing fits both axes.
        const float aspect = (m_camera.AspectRatio > 0.0f) ? m_camera.AspectRatio : 1.0f;

        // Choose distance to frame bounding sphere comfortably (fov affects required distance)
        if (!m_is2DMode && m_camera.UsePerspective) {
            // perspective: distance depends on FOV such that object fits viewport
            // Use both vertical and horizontal FOV to ensure fit
            float fovRad = glm::radians(m_camera.FOV);              // vertical FOV
            float screenFactor = 1.2f;                              // padding
            float halfFovY = fovRad * 0.5f;                         // half vertical FOV
            float halfFovX = atanf(tanf(halfFovY) * aspect);        // half horizontal FOV
            float desiredDistanceY = (extents.y > 0.0f) ? (extents.y / glm::tan(halfFovY)) : 0.0f; // from vertical
            float desiredDistanceX = (extents.x > 0.0f) ? (extents.x / glm::tan(halfFovX)) : 0.0f; // from horizontal
            float desiredDistance = glm::max(desiredDistanceX, desiredDistanceY); // use the larger
            desiredDistance = glm::max(desiredDistance, radius / glm::tan(halfFovY)); // also consider sphere

            desiredDistance *= screenFactor;
            m_distance = glm::clamp(desiredDistance, m_minDistance, m_maxDistance);
        } else {
            // orthographic: keep ortho size large enough to encompass extents on XY plane
            float maxExtentXY = glm::max(extents.y, extents.x / aspect);
            m_camera.OrthoSize = glm::clamp(maxExtentXY * 1.1f, m_minOrthoSize, m_maxOrthoSize);
        }

        // Desired focus target for smooth framing
        glm::vec3 desiredTarget = centre;
        if (m_is2DMode || !m_camera.UsePerspective) {
            desiredTarget.z = m_2DPlaneZ;
        }

        // If current camera forward is degenerate, recompute yaw/pitch from position->target
        glm::vec3 toTarget = desiredTarget - m_camera.Position;
        float dist = glm::length(toTarget);
        if (dist > 1e-5f) {
            glm::vec3 n = glm::normalize(toTarget);
            // Reconstruct pitch/yaw from forward vector. Note: forward = normalize(target - position)
            // Our spherical orbit uses offsets where forward == -offset/distance, so invert signs.
            m_pitch = -asinf(glm::clamp(n.y, -1.0f, 1.0f));
            m_yaw = atan2(-n.x, -n.z);
        } else {
            // fallback to default orbit angles
            m_yaw = 0.785398f;   // 45 degrees
            m_pitch = -0.35f;    // slight downward
        }

        // Setup focus smoothing targets
        m_focusTarget = desiredTarget;
        m_focusDistance = m_distance;
        m_focusOrthoSize = m_camera.OrthoSize;
        m_focusActive = true;
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;
    }

    void EditorCamera::SetViewportSize(float width, float height) {
        m_camera.SetAspectRatio(width, height);
    }

    void EditorCamera::ResetTo2D() {
        m_is2DMode = true;
        m_camera.UsePerspective = false;

        // Preserve position but align to strict top-down orthographic view
        // Do not zero Z — keep Z so layers remain intact; only projection changes
        // We aim the camera at negative Z so 2D XY plane is visible
        glm::vec3 pos = m_camera.Position;
        // place camera above XY plane if it's too close to plane
        if (pos.z < 0.1f) pos.z = 10.0f;

        m_camera.Position = pos;
        m_2DPlaneZ = m_camera.Position.z;
        // Rotation flat: look down -Z; for consistency with 2D editors we'll look along -Z
        m_camera.Rotation = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));

        // Set default orthographic size if not set
        if (m_camera.OrthoSize <= 0.0f) m_camera.OrthoSize = 10.0f;

        // keep target under the camera projection centre
        m_target = glm::vec3(m_camera.Position.x, m_camera.Position.y, m_2DPlaneZ);
        m_distance = glm::length(m_camera.Position - m_target);
        _apply2DTargetToCamera();

        // recompute yaw/pitch from top-down orientation
        m_yaw = 0.0f;
        m_pitch = 0.0f;

        // smoothing resets
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;
    }

    void EditorCamera::ResetTo3D() {
        m_is2DMode = false;
        m_camera.UsePerspective = true;

        // Default 3D view: angled perspective looking at origin
        if (glm::length(m_camera.Position) < 1e-4f) {
            m_camera.Position = glm::vec3(5.0f, 5.0f, 5.0f);
        }

        // keep Z layers intact: don't zero Z or reproject objects
        m_target = glm::vec3(0.0f); // look at origin by default

        // compute orbit parameters from current camera transform
        glm::vec3 toTarget = m_target - m_camera.Position;
        m_distance = glm::length(toTarget);
        if (m_distance > 1e-5f) {
            glm::vec3 n = glm::normalize(toTarget);
            // Invert signs to match spherical coordinate convention used in _updateOrbitPosition
            m_pitch = -asinf(glm::clamp(n.y, -1.0f, 1.0f));
            m_yaw = atan2(-n.x, -n.z);
        } else {
            m_distance = 8.0f;
            m_yaw = 0.785398f; // 45 deg
            m_pitch = -0.35f;
        }

        // Update camera position and rotation to look at target
        _updateOrbitPosition();

        // smoothing resets
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;
    }

    void EditorCamera::ToggleViewMode() {
        if (m_is2DMode) ResetTo3D();
        else ResetTo2D();
    }

    void EditorCamera::_apply2DTargetToCamera() {
        // 2D mode pins camera to the XY plane and tracks target in XY only.
        m_target.z = m_2DPlaneZ;
        m_camera.Position.x = m_target.x;
        m_camera.Position.y = m_target.y;
        m_camera.Position.z = m_2DPlaneZ;
    }

    void EditorCamera::_applyFocusSmoothing(float dt) {
        if (!m_focusActive || dt <= 0.0f) {
            return;
        }

        // Smooth target and distance/ortho size towards focus goal.
        float alpha = glm::clamp(dt * m_focusLerpSpeed, 0.0f, 1.0f);
        m_target = glm::mix(m_target, m_focusTarget, alpha);

        if (m_is2DMode || !m_camera.UsePerspective) {
            m_camera.OrthoSize = glm::mix(m_camera.OrthoSize, m_focusOrthoSize, alpha);
            _apply2DTargetToCamera();
        } else {
            m_distance = glm::mix(m_distance, m_focusDistance, alpha);
            _updateOrbitPosition();
        }

        // Check if focus is done
        const bool targetDone = glm::length(m_focusTarget - m_target) < 0.001f;
        const bool distanceDone = std::abs(m_focusDistance - m_distance) < 0.001f;
        const bool orthoDone = std::abs(m_focusOrthoSize - m_camera.OrthoSize) < 0.001f;
        const bool done = targetDone && (m_is2DMode || !m_camera.UsePerspective ? orthoDone : distanceDone);

        // If done, snap to final values and disable focus
        if (done) {
            // Snap to finals
            m_target = m_focusTarget;
            m_distance = m_focusDistance;
            m_camera.OrthoSize = m_focusOrthoSize;

            // Final application to camera
            if (m_is2DMode || !m_camera.UsePerspective) {
                _apply2DTargetToCamera();
            } else {
                _updateOrbitPosition();
            }
            m_focusActive = false; // focus complete
        }
    }

    float EditorCamera::_getSpeedScale() const {
        if (m_is2DMode || !m_camera.UsePerspective) {
            return 1.0f;
        }

        // Scale fly speed by orbit distance for consistent feel.
        const float distance = glm::max(m_distance, m_minDistance);
        const float scale = distance * 0.1f;
        return glm::clamp(scale, 0.2f, 20.0f);
    }

}
