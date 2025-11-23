/* Start Header *****************************************************************/
/*!
\file   EditorCamera.hpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Defines the EditorCamera class, a protected ECS camera wrapper designed
for editor viewports. It supports orbit, pan, and zoom interactions around a
target point and prevents external modification of its underlying entity.

The EditorCamera encapsulates an internal ECS entity that owns both a
Transform and a CameraEditor3D component. No additional components can be attached
to ensure that the editor view remains isolated from gameplay logic.

Responsibilities:
- Maintain camera position, orientation, and projection for the editor viewport.
- Process mouse and keyboard input for orbit, pan, and zoom.
- Provide view and projection matrices for rendering.
- Synchronize internal ECS components each frame.

Dependencies:
- Requires Input and WindowManager services.
- Uses glm for matrix and vector math.

Usage Example:
    EditorCamera editorCam(world);
    editorCam.Update(deltaTime);
    renderer->SetCamera(editorCam);
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/World.h"
#include "ecs/Entity.h"
#include "ecs/Components.h"
#include "core/messaging/MessageSystem.h"
#include "services/Input.h"
#include "services/WindowManager.h"
#include <glm/glm.hpp>

namespace Engine {

    class EditorCamera {
    public:
        explicit EditorCamera(ECS::World& world);
        ~EditorCamera();

        /// Updates camera position and projection based on user input.
        void Update(float deltaTime);

        /// Enable/disable processing of input (pan/orbit/zoom). When disabled,
        /// camera state remains stable regardless of mouse/scroll activity.
        void SetAllowInput(bool allow) { m_allowInput = allow; }

        /// Returns the view matrix derived from the camera�s transform.
        [[nodiscard]] glm::mat4 GetViewMatrix() const;

        /// Returns the projection matrix from the internal CameraEditor3D component.
        [[nodiscard]] glm::mat4 GetProjectionMatrix() const;


        /// Focuses the camera on a given world-space point.
        void Focus(const glm::vec3& target);

        /// Accessor for the internal CameraEditor3D component.
        ECS::Components::CameraEditor3D* GetCameraComponent() const { return m_camera; }

        /// Accessor for the internal Transform component.
        ECS::Components::LocalTransform* GetTransform() const { return m_transform; }

        /// Accessor for the internal entity (for filtering in debug visualization)
        ECS::Entity GetEntity() const { return m_cameraEntity; }

        /// Rebind the camera to a different ECS world without recreating the object
        void BindWorld(ECS::World& world);

        void OnWindowResize(int newWidth, int newHeight);

        /// Update viewport size (for aspect ratio) independently of window size
        void SetViewportSize(float width, float height);

    private:
        // --------------------------------------------------------------------
        // Internal state
        // --------------------------------------------------------------------
        ECS::World* m_world = nullptr;                      //!< Owning ECS world (for cleanup)
        ECS::Entity m_cameraEntity;                         //!< Wrapped ECS entity
        ECS::Components::LocalTransform* m_transform{};     //!< Pointer to Transform component
        ECS::Components::CameraEditor3D* m_camera{};        //!< Pointer to CameraEditor3D component
        Messaging::SubscriptionHandle m_windowResizedSub{}; //!< Subscription to window resize
        Messaging::SubscriptionHandle m_viewportResizedSub{}; //!< Subscription to viewport resize

        glm::vec3 m_target = { 0.f, 0.f, 0.f };             //!< Orbit center
        glm::vec3 m_cameraPosition = { 0.f, 0.f, 10.f };    //!< Cached position

        float m_distance = 10.f;                            //!< Distance from orbit center
        float m_yaw = 0.f;                                  //!< Horizontal rotation angle
        float m_pitch = 0.f;                                //!< Vertical rotation angle
        float m_targetOrthoSize = 1000.0f;                  //!< Target orthographic size
        float m_perspectiveBlend = 0.0f;                    //!< 0=ortho, 1=perspective
        bool  m_returningToOrtho = false;                   //!< Whether camera is transitioning to ortho mode

        // --------------------------------------------------------------------
        // Mouse state and interaction flags
        // --------------------------------------------------------------------
        glm::vec2 m_lastMousePos = { 0.f, 0.f };
        bool m_firstMouse = true;
        bool m_panning = false;
        bool m_orbiting = false;

        // Whether to process input in Update(). Controlled by editor viewport hover.
        bool m_allowInput = true;

        // --------------------------------------------------------------------
        // Internal helpers
        // --------------------------------------------------------------------
        void HandleInput(float dt);
        void UpdateTransform();

        // Disable copy/move and external exposure
        EditorCamera(const EditorCamera&) = delete;
        EditorCamera& operator=(const EditorCamera&) = delete;
    };

} // namespace Engine
