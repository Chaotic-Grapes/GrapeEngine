/* Start Header *****************************************************************/
/*!
\file   EditorCamera.cpp
\author Choi Meng Yew (95%)
        Foo Rui Qin (5%)
\par    choi.m@digipen.edu
        ruiqin.foo@digipen.edu
\date   12th March 2026
\brief
Implementation for the editor viewport camera.

Implements the navigation behaviours declared in 'EditorCamera.hpp'.
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

    // ~89 degrees in radians
    static constexpr float RAD_CLAMP = 1.55334303f; 

    // Initializes editor camera defaults, enters 2D mode, and registers resize listeners
    EditorCamera::EditorCamera() {
        // Set practical defaults so the camera is immediately usable before any scene setup
        m_camera.FOV = 45.0f;
        m_camera.NearPlane = 0.1f;
        m_camera.FarPlane = 10000.0f;
        m_camera.OrthoSize = 10.0f;

        // Start in 2D mode because editor workflows often begin with layout tasks
        ResetTo2D();

        // Seed smoothing buffers from current transform to avoid the first-frame snap
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;

        // Apply startup aspect from main window when platform context is available
        auto* context = Engine::CORE->GetPlatformContext();
        if (context) {
            if (auto* win = context->GetMainWindow()) {
                m_camera.SetAspectRatio(static_cast<float>(win->GetWidth()),
                                        static_cast<float>(win->GetHeight()));
            }
        }

        // Keep projection aspect synchronized with host window resize events
        m_windowResizedSub = Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
            [this](const Messaging::WindowResized& e) {
                m_camera.SetAspectRatio(static_cast<float>(e.Width), static_cast<float>(e.Height));
            }
        );

        // Keep projection aspect synchronized with viewport panel resize events
        m_viewportResizedSub = Messaging::MessageSystem::Subscribe<Messaging::ViewportResized>(
            [this](const Messaging::ViewportResized& e) {
                m_camera.SetAspectRatio(e.Width, e.Height);
            }
        );
    }

    // Unsubscribes resize callbacks to prevent stale captures after destruction
    EditorCamera::~EditorCamera() {
        Messaging::MessageSystem::Unsubscribe<Messaging::WindowResized>(m_windowResizedSub);
        Messaging::MessageSystem::Unsubscribe<Messaging::ViewportResized>(m_viewportResizedSub);
    }

    // Resets per-frame input guards and mouse baseline handling
    void EditorCamera::BeginFrame() {
        // Allow one input processing pass in this frame
        m_frameInputProcessed = false;

        // Re-arm first-mouse mode when viewport is not focused so next delta starts cleanly
        if (!m_isViewportFocused) {
            m_firstMouse = true;
        }
    }

    // Handles input, focus animation, and smoothing for this frame
    void EditorCamera::Update(float dt) {
        // Ignore direct navigation while viewport is unfocused to avoid accidental camera movement
        if (!m_isViewportFocused) {
            // Keep mouse state primed so returning focus does not produce a large cursor jump
            m_firstMouse = true;

            // Continue focus animation while unfocused so hierarchy-driven focus still completes
            if (m_focusActive) {
                _applyFocusSmoothing(dt);
                _applySmoothing(dt);
            }
            return;
        }

        // Guard against double processing when update hooks run more than once in a frame
        if (m_frameInputProcessed) {
            return;
        }

        // Compute one mouse delta sample and reuse it across interaction branches
        glm::vec2 mouseDelta(0.0f);
        _handleMouseDelta(mouseDelta);

        // Reset marker so this frame can decide if user explicitly navigated
        m_hadNavigationInput = false;

        // Dispatch interaction modes and keyboard movement
        _handleInput(dt, mouseDelta);

        // Cancel scripted focus as soon as user takes manual control
        if (m_hadNavigationInput) {
            m_focusActive = false;
        }
        else {
            // Continue scripted focus only when there is no manual override
            _applyFocusSmoothing(dt);
        }

        // Smooth transform after all target state updates have been applied
        _applySmoothing(dt);

        // Mark update consumed for this frame
        m_frameInputProcessed = true;
    }

    // Reserved hook for future end-of-frame camera work
    void EditorCamera::EndFrame() {
        // Intentionally empty for now
    }

    // Produces mouse delta with safe first-sample behavior after focus changes
    void EditorCamera::_handleMouseDelta(glm::vec2& outDelta) {
        double mx = 0.0;
        double my = 0.0;
        Input::GetMousePosition(mx, my);
        glm::vec2 mousePos(static_cast<float>(mx), static_cast<float>(my));

        // On first sample, seed previous position to current position so output delta stays zero
        if (m_firstMouse) {
            m_lastMousePos = mousePos;
            m_firstMouse = false;
        }

        // Delta is current sample minus previous sample, then previous sample is advanced
        outDelta = mousePos - m_lastMousePos;
        m_lastMousePos = mousePos;
    }

    // Chooses navigation path by priority and applies keyboard and wheel controls
    void EditorCamera::_handleInput(float dt, const glm::vec2& mouseDelta) {
        // Cache state once so all decisions use one consistent input snapshot
        const bool alt = Input::IsKeyDown(KEY_LEFT_ALT) || Input::IsKeyDown(KEY_RIGHT_ALT);
        const bool lmb = Input::IsMouseDown(MOUSE_LEFT);
        const bool mmb = Input::IsMouseDown(MOUSE_MIDDLE);
        const bool rmb = Input::IsMouseDown(MOUSE_RIGHT);
        const float scroll = static_cast<float>(Input::GetScrollY()); // Vertical scroll amount

        // Allow instant camera mode switching by keyboard
        if (Input::IsKeyPressed(KEY_2)) {
            ResetTo2D();
        }

        if (Input::IsKeyPressed(KEY_3)) {
            ResetTo3D();
        }

        if (Input::IsKeyPressed(KEY_V)) {
            ToggleViewMode();
        }

        // Resolve mouse mode conflicts using explicit priority order
        if (rmb) {
            // RMB takes fly mode priority because it combines look and movement controls
            m_hadNavigationInput = true;
            _handleFly(dt, mouseDelta);
        }
        else if (alt && lmb) {
            // Alt + LMB orbits around pivot without translating pivot
            m_hadNavigationInput = true;
            _handleOrbit(mouseDelta);
        }
        else if (mmb) {
            // MMB pans pivot in camera plane
            m_hadNavigationInput = true;
            _handlePan(mouseDelta);
        }
        else if (alt && Input::IsMouseDown(MOUSE_RIGHT) && (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)) {
            // Alt + RMB drag performs dolly-like zoom using vertical drag magnitude
            m_hadNavigationInput = true;
            _handleZoom(mouseDelta.y * 0.01f);
        }

        // Process keyboard movement when not in RMB fly mode to avoid duplicate movement application
        if (!rmb) {
            _handleMovementKeys(dt);
        }

        // Process wheel zoom in all modes for quick framing adjustments
        if (scroll != 0.0f) {
            m_hadNavigationInput = true;
            _handleZoom(scroll);
        }
    }

    // Applies FPS-like look and movement while RMB is held
    void EditorCamera::_handleFly(float dt, const glm::vec2& delta) {
        // Convert cursor delta to angular yaw and pitch updates
        const float lookSpeed = 0.0025f;
        m_yaw -= delta.x * lookSpeed;
        m_pitch -= delta.y * lookSpeed;

        // Clamp pitch near vertical to avoid unstable orientation around poles
        m_pitch = glm::clamp(m_pitch, -RAD_CLAMP, RAD_CLAMP);

        // Build rotation from Euler values so camera basis vectors update immediately
        glm::quat rot = glm::quat(glm::vec3(m_pitch, m_yaw, 0.0f));
        m_camera.Rotation = rot;

        // Extract camera basis for local-space movement
        glm::vec3 forward = m_camera.GetForward();
        glm::vec3 right = m_camera.GetRight();
        glm::vec3 up = m_camera.GetUp();

        // Combine base speed, sprint modifier, and distance scaling for consistent feel across scene sizes
        float speed = Input::IsKeyDown(KEY_LEFT_SHIFT) ? m_fastMoveSpeed : m_moveSpeed;
        speed *= _getSpeedScale();

        // Apply per-axis translation using dt for frame-rate-independent movement
        if (Input::IsKeyDown(KEY_W)) m_camera.Position += forward * speed * dt;
        if (Input::IsKeyDown(KEY_S)) m_camera.Position -= forward * speed * dt;
        if (Input::IsKeyDown(KEY_A)) m_camera.Position -= right * speed * dt;
        if (Input::IsKeyDown(KEY_D)) m_camera.Position += right * speed * dt;
        if (Input::IsKeyDown(KEY_Q)) m_camera.Position -= up * speed * dt;
        if (Input::IsKeyDown(KEY_E)) m_camera.Position += up * speed * dt;

        // Keep orbit pivot ahead of camera so switching from fly to orbit does not snap unexpectedly
        m_target = m_camera.Position + forward * m_distance;
    }

    // Applies keyboard-only movement when RMB fly is not active
    void EditorCamera::_handleMovementKeys(float dt) {
        // 2D and orthographic modes treat movement keys as planar panning controls
        if (m_is2DMode || !m_camera.UsePerspective) {
            // Ortho size scales world-units-per-screen-unit so pan speed stays intuitive while zooming
            float panScale = m_camera.OrthoSize * m_panSpeed;
            float speedMod = Input::IsKeyDown(KEY_LEFT_SHIFT) ? 4.0f : 1.0f;

            // Use camera local basis so controls follow current view orientation
            glm::vec3 right = m_camera.GetRight();
            glm::vec3 up = m_camera.GetUp();
            glm::vec3 delta(0.0f);

            // Build one direction vector from all active keys
            if (Input::IsKeyDown(KEY_W)) delta += up;
            if (Input::IsKeyDown(KEY_S)) delta -= up;
            if (Input::IsKeyDown(KEY_A)) delta -= right;
            if (Input::IsKeyDown(KEY_D)) delta += right;

            // Normalize to prevent faster diagonal panning then apply scaled movement
            if (glm::length(delta) > 0.0f) {
                delta = glm::normalize(delta);
                m_target += delta * panScale * speedMod;
                m_hadNavigationInput = true;

                // Mirror target back to camera because 2D mode pins camera to pivot on fixed plane
                _apply2DTargetToCamera();
            }
            return;
        }

        // Perspective mode uses local basis translation similar to fly mode
        glm::vec3 forward = m_camera.GetForward();
        glm::vec3 right = m_camera.GetRight();
        glm::vec3 up = m_camera.GetUp();

        float speed = Input::IsKeyDown(KEY_LEFT_SHIFT) ? m_fastMoveSpeed : m_moveSpeed;
        speed *= _getSpeedScale();

        if (Input::IsKeyDown(KEY_W)) m_camera.Position += forward * speed * dt;
        if (Input::IsKeyDown(KEY_S)) m_camera.Position -= forward * speed * dt;
        if (Input::IsKeyDown(KEY_A)) m_camera.Position -= right * speed * dt;
        if (Input::IsKeyDown(KEY_D)) m_camera.Position += right * speed * dt;
        if (Input::IsKeyDown(KEY_Q)) m_camera.Position -= up * speed * dt;
        if (Input::IsKeyDown(KEY_E)) m_camera.Position += up * speed * dt;

        // Track whether any movement key was active so scripted focus can be interrupted upstream
        const bool moved = Input::IsKeyDown(KEY_W) || Input::IsKeyDown(KEY_S) || Input::IsKeyDown(KEY_A)
            || Input::IsKeyDown(KEY_D) || Input::IsKeyDown(KEY_Q) || Input::IsKeyDown(KEY_E);

        if (moved) {
            m_hadNavigationInput = true;
        }

        // Keep pivot coherent with translated camera for smooth transition back to orbit interaction
        m_target = m_camera.Position + forward * m_distance;
    }

    // Rotates camera around target pivot using spherical yaw and pitch deltas
    void EditorCamera::_handleOrbit(const glm::vec2& delta) {
        // Scale orbit sensitivity with distance so angular response feels consistent at different ranges
        float distanceFactor = glm::max(1.0f, m_distance);
        float yawDelta = delta.x * m_orbitSpeed * (distanceFactor * 0.02f);
        float pitchDelta = delta.y * m_orbitSpeed * (distanceFactor * 0.02f);

        m_yaw -= yawDelta;
        m_pitch -= pitchDelta;

        // Clamp pitch to stay away from pole singularities
        m_pitch = glm::clamp(m_pitch, -RAD_CLAMP, RAD_CLAMP);

        _updateOrbitPosition();
    }

    // Moves target in camera plane from cursor drag
    void EditorCamera::_handlePan(const glm::vec2& delta) {
        // Use projection-specific scale so pan speed feels uniform relative to what user sees
        float panScale = 1.0f;
        if (!m_is2DMode) {
            // Perspective pan should scale with orbit distance because farther pivots map more world space per pixel
            panScale = m_distance * m_panSpeed;
        }
        else {
            // Orthographic pan should scale with ortho size because zoom level changes world-space pixel density
            panScale = m_camera.OrthoSize * m_panSpeed;
        }

        // Apply drag along camera right and up vectors
        glm::vec3 right = m_camera.GetRight();
        glm::vec3 up = m_camera.GetUp();

        m_target -= right * delta.x * panScale;
        m_target += up * delta.y * panScale;

        // Rebuild camera from updated target according to active projection mode
        if (m_is2DMode || !m_camera.UsePerspective) {
            _apply2DTargetToCamera();
        }
        else {
            _updateOrbitPosition();
        }
    }

    // Applies orthographic size zoom or perspective distance dolly
    void EditorCamera::_handleZoom(float scrollOrDelta) {
        if (scrollOrDelta == 0.0f) {
            return;
        }

        if (m_is2DMode || !m_camera.UsePerspective) {
            // Orthographic zoom uses multiplicative scaling for smooth zoom progression
            float factor = 1.0f - (scrollOrDelta * m_zoomSpeed);

            // Prevent zero or negative scale factors that would invert zoom behavior
            factor = glm::max(0.001f, factor);
            m_camera.OrthoSize *= factor;

            // Keep ortho size inside configured usability range
            m_camera.OrthoSize = glm::clamp(m_camera.OrthoSize, m_minOrthoSize, m_maxOrthoSize);
        }
        else {
            // Perspective zoom adjusts orbit distance to preserve FOV and perspective feel
            float factor = 1.0f - (scrollOrDelta * m_zoomSpeed);

            // Clamp multiplier to avoid crossing target and producing inverted orbit distance
            factor = glm::max(0.001f, factor);
            m_distance *= factor;
            m_distance = glm::clamp(m_distance, m_minDistance, m_maxDistance);

            _updateOrbitPosition();
        }
    }

    // Converts yaw, pitch, and distance into camera position and orientation around target
    void EditorCamera::_updateOrbitPosition() {
        // Convert spherical coordinates to cartesian offset around pivot
        glm::vec3 offset;

        // X combines horizontal yaw and horizontal radius component from cos(pitch)
        offset.x = m_distance * cosf(m_pitch) * sinf(m_yaw);

        // Y is vertical component directly from sin(pitch)
        offset.y = m_distance * sinf(m_pitch);

        // Z completes horizontal circle with cos(yaw) and horizontal radius
        offset.z = m_distance * cosf(m_pitch) * cosf(m_yaw);

        // Final position is pivot plus offset
        m_camera.Position = m_target + offset;

        // Early out when target and position coincide to avoid unstable look direction normalization
        glm::vec3 forward = glm::normalize(m_target - m_camera.Position);
        if (glm::length(forward) < 1e-6f) {
            return;
        }

        // Use camera LookAt helper so basis construction matches runtime camera conventions
        m_camera.LookAt(m_target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // Interpolates camera transform toward current target transform
    void EditorCamera::_applySmoothing(float dt) {
        if (dt <= 0.0f) {
            return;
        }

        // Position lerp smooths abrupt jumps while preserving deterministic target value
        float alphaPos = glm::clamp(dt * m_posLerpSpeed, 0.0f, 1.0f);
        m_smoothPosition = glm::mix(m_smoothPosition, m_camera.Position, alphaPos);

        // Quaternion slerp preserves normalized rotation and shortest arc interpolation
        float alphaRot = glm::clamp(dt * m_rotLerpSpeed, 0.0f, 1.0f);
        m_smoothRotation = glm::slerp(m_smoothRotation, m_camera.Rotation, alphaRot);

        // Write filtered transform back to camera state
        m_camera.Position = m_smoothPosition;
        m_camera.Rotation = m_smoothRotation;
    }

    // Starts smooth focus transition toward a world point
    void EditorCamera::Focus(const glm::vec3& worldPoint) {
        // Build desired pivot from requested point, with 2D plane lock when not in perspective
        glm::vec3 desiredTarget = worldPoint;
        if (m_is2DMode || !m_camera.UsePerspective) {
            desiredTarget.z = m_2DPlaneZ;
        }

        // Ensure valid perspective distance so focus destination can be constructed robustly
        float desiredDistance = m_distance;
        if (desiredDistance < 0.001f) {
            desiredDistance = 8.0f;
        }

        // Store focus goals consumed incrementally by _applyFocusSmoothing
        m_focusTarget = desiredTarget;
        m_focusDistance = desiredDistance;
        m_focusOrthoSize = m_camera.OrthoSize;
        m_focusActive = true;

        // Re-seed smoothing state from current transform to prevent jump at focus start
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;
    }

    // Frames an axis-aligned bounds box by computing target and distance or ortho size
    void EditorCamera::FocusBounds(const glm::vec3& min, const glm::vec3& max) {
        // Compute center and half-extents for framing calculations
        glm::vec3 centre = (min + max) * 0.5f;
        glm::vec3 extents = (max - min) * 0.5f;
        float radius = glm::length(extents);

        // Use fallback aspect of 1 when not initialized to keep formulas numerically stable
        const float aspect = (m_camera.AspectRatio > 0.0f) ? m_camera.AspectRatio : 1.0f;

        if (!m_is2DMode && m_camera.UsePerspective) {
            // Convert vertical FOV to radians for trigonometric fit math
            float fovRad = glm::radians(m_camera.FOV);

            // Add margin so framed object is not touching viewport edges
            float screenFactor = 1.2f;

            // Compute half-angles to simplify tan-based distance formulas
            float halfFovY = fovRad * 0.5f;
            float halfFovX = atanf(tanf(halfFovY) * aspect);

            // Solve distance needed to fit vertical extent using opposite / tan(theta)
            float desiredDistanceY = (extents.y > 0.0f) ? (extents.y / glm::tan(halfFovY)) : 0.0f;

            // Solve distance needed to fit horizontal extent with horizontal half FOV
            float desiredDistanceX = (extents.x > 0.0f) ? (extents.x / glm::tan(halfFovX)) : 0.0f;

            // Choose larger requirement so both axes fit inside viewport
            float desiredDistance = glm::max(desiredDistanceX, desiredDistanceY);

            // Also enforce sphere-based fit so diagonal dimensions still fit robustly
            desiredDistance = glm::max(desiredDistance, radius / glm::tan(halfFovY));

            desiredDistance *= screenFactor;
            m_distance = glm::clamp(desiredDistance, m_minDistance, m_maxDistance);
        }
        else {
            // Orthographic fit uses whichever axis needs larger half-size once aspect is considered
            float maxExtentXY = glm::max(extents.y, extents.x / aspect);
            m_camera.OrthoSize = glm::clamp(maxExtentXY * 1.1f, m_minOrthoSize, m_maxOrthoSize);
        }

        // Focus target is bounds center, then pinned to 2D plane when required
        glm::vec3 desiredTarget = centre;
        if (m_is2DMode || !m_camera.UsePerspective) {
            desiredTarget.z = m_2DPlaneZ;
        }

        // Derive yaw and pitch from current camera to desired target vector when possible
        glm::vec3 toTarget = desiredTarget - m_camera.Position;
        float dist = glm::length(toTarget);
        if (dist > 1e-5f) {
            glm::vec3 n = glm::normalize(toTarget);

            // Sign inversion matches this camera's spherical convention used in _updateOrbitPosition
            m_pitch = -asinf(glm::clamp(n.y, -1.0f, 1.0f));
            m_yaw = atan2(-n.x, -n.z);
        }
        else {
            // Fallback angles produce a stable default orbit orientation
            m_yaw = 0.785398f; // 45 degrees
            m_pitch = -0.35f;
        }

        // Configure smooth transition targets
        m_focusTarget = desiredTarget;
        m_focusDistance = m_distance;
        m_focusOrthoSize = m_camera.OrthoSize;
        m_focusActive = true;
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;
    }

    // Pushes viewport dimensions into camera aspect ratio
    void EditorCamera::SetViewportSize(float width, float height) {
        m_camera.SetAspectRatio(width, height);
    }

    // Configures camera for orthographic XY-plane editing
    void EditorCamera::ResetTo2D() {
        m_is2DMode = true;
        m_camera.UsePerspective = false;

        // Preserve current XY location and enforce a safe positive Z distance from plane
        glm::vec3 pos = m_camera.Position;
        if (pos.z < 0.1f) {
            pos.z = 10.0f;
        }

        m_camera.Position = pos;

        // Lock active 2D plane to current Z so panning and focusing stay on one plane
        m_2DPlaneZ = m_camera.Position.z;

        // Use flat orientation aligned to world axes for predictable 2D controls
        m_camera.Rotation = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));

        // Ensure orthographic size is valid when switching from unknown previous state
        if (m_camera.OrthoSize <= 0.0f) {
            m_camera.OrthoSize = 10.0f;
        }

        // Place pivot under camera center then apply 2D projection constraints
        m_target = glm::vec3(m_camera.Position.x, m_camera.Position.y, m_2DPlaneZ);
        m_distance = glm::length(m_camera.Position - m_target);
        _apply2DTargetToCamera();

        // Reset orbit angles because 2D mode does not use orbit orientation
        m_yaw = 0.0f;
        m_pitch = 0.0f;

        // Reset smoothing buffers to current transform to avoid blended leftovers
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;
    }

    // Configures camera for perspective orbit navigation
    void EditorCamera::ResetTo3D() {
        m_is2DMode = false;
        m_camera.UsePerspective = true;

        // Provide a practical default position when camera is near origin
        if (glm::length(m_camera.Position) < 1e-4f) {
            m_camera.Position = glm::vec3(5.0f, 5.0f, 5.0f);
        }

        // Use origin as default pivot for broad scene visibility
        m_target = glm::vec3(0.0f);

        // Derive orbit parameters from current camera-to-target vector
        glm::vec3 toTarget = m_target - m_camera.Position;
        m_distance = glm::length(toTarget);
        if (m_distance > 1e-5f) {
            glm::vec3 n = glm::normalize(toTarget);

            // Sign inversion matches this file's spherical orbit equations
            m_pitch = -asinf(glm::clamp(n.y, -1.0f, 1.0f));
            m_yaw = atan2(-n.x, -n.z);
        }
        else {
            // Fallback orbit parameters guarantee a valid perspective framing state
            m_distance = 8.0f;
            m_yaw = 0.785398f; // 45 degrees
            m_pitch = -0.35f;
        }

        _updateOrbitPosition();

        // Reset smoothing buffers to current transform so transition is stable
        m_smoothPosition = m_camera.Position;
        m_smoothRotation = m_camera.Rotation;
    }

    // Flips between 2D and 3D camera configurations
    void EditorCamera::ToggleViewMode() {
        if (m_is2DMode) {
            ResetTo3D();
        }
        else {
            ResetTo2D();
        }
    }

    // Projects pivot and camera position onto fixed 2D plane depth
    void EditorCamera::_apply2DTargetToCamera() {
        // Keep target and camera on the same locked plane depth for consistent 2D editing
        m_target.z = m_2DPlaneZ;
        m_camera.Position.x = m_target.x;
        m_camera.Position.y = m_target.y;
        m_camera.Position.z = m_2DPlaneZ;
    }

    // Advances focus transition toward stored targets and stops when close enough
    void EditorCamera::_applyFocusSmoothing(float dt) {
        if (!m_focusActive || dt <= 0.0f) {
            return;
        }

        // Build interpolation factor from dt and configured focus smoothing speed
        float alpha = glm::clamp(dt * m_focusLerpSpeed, 0.0f, 1.0f);
        m_target = glm::mix(m_target, m_focusTarget, alpha);

        // Interpolate projection-specific zoom state and rebuild camera transform
        if (m_is2DMode || !m_camera.UsePerspective) {
            m_camera.OrthoSize = glm::mix(m_camera.OrthoSize, m_focusOrthoSize, alpha);
            _apply2DTargetToCamera();
        }
        else {
            m_distance = glm::mix(m_distance, m_focusDistance, alpha);
            _updateOrbitPosition();
        }

        // Completion checks use epsilons so floating-point residuals do not block finalization
        const bool targetDone = glm::length(m_focusTarget - m_target) < 0.001f;
        const bool distanceDone = std::abs(m_focusDistance - m_distance) < 0.001f;
        const bool orthoDone = std::abs(m_focusOrthoSize - m_camera.OrthoSize) < 0.001f;
        const bool done = targetDone && (m_is2DMode || !m_camera.UsePerspective ? orthoDone : distanceDone);

        if (done) {
            // Snap exact final values to remove minor interpolation error accumulation
            m_target = m_focusTarget;
            m_distance = m_focusDistance;
            m_camera.OrthoSize = m_focusOrthoSize;

            // Re-apply final state through mode-specific transform update path
            if (m_is2DMode || !m_camera.UsePerspective) {
                _apply2DTargetToCamera();
            }
            else {
                _updateOrbitPosition();
            }

            m_focusActive = false;
        }
    }

    // Returns distance-aware speed multiplier for perspective movement
    float EditorCamera::_getSpeedScale() const {
        if (m_is2DMode || !m_camera.UsePerspective) {
            return 1.0f;
        }

        // Scale by distance so large scenes remain navigable without making close work too twitchy
        const float distance = glm::max(m_distance, m_minDistance);
        const float scale = distance * 0.1f;
        return glm::clamp(scale, 0.2f, 20.0f);
    }

}
