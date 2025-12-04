/* Start Header *****************************************************************/
/*!
\file   Camera.h
\author Choi Meng Yew
\par    choi.m@digipen.edu
\date   4th December 2025
\brief
A refactored class. Standalone camera class for rendering. This is NOT an ECS 
component. Used by both editor cameras and as a base for game camera rendering.

Responsibilities:
- Provide view and projection matrices
- Support both perspective and orthographic projections
- Calculate screen-to-world rays for picking
- Pure data structure with no dependencies on ECS or editor

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {

    /**
     * @brief Ray structure for screen-to-world picking
     */
    struct Ray {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    /**
     * @brief Standalone camera class (non-ECS)
     * 
     * This camera is a pure data structure that can be used by:
     * - Editor cameras (one per viewport)
     * - Game cameras (converted from ECS CameraComponent)
     * - Any system that needs camera matrices
     */
    class Camera {
    public:
        // ====================================================================
        // Camera Parameters
        // ====================================================================
        glm::vec3 Position = glm::vec3(0.0f, 0.0f, 10.0f);
        glm::quat Rotation = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
        
        // Perspective parameters
        float FOV = 45.0f;        // Field of view in degrees
        float NearPlane = 0.1f;
        float FarPlane = 1000.0f;
        
        // Orthographic parameters
        float OrthoSize = 10.0f;  // Half-height of orthographic view
        
        // Projection settings
        float AspectRatio = 16.0f / 9.0f;
        bool UsePerspective = false;  // false = orthographic, true = perspective

        // ====================================================================
        // Matrix Generation
        // ====================================================================

        /**
         * @brief Get the view matrix for this camera
         * @return View matrix (world to camera space)
         */
        glm::mat4 GetViewMatrix() const {
            glm::vec3 forward = GetForward();
            glm::vec3 up = GetUp();
            return glm::lookAt(Position, Position + forward, up);
        }

        /**
         * @brief Get the projection matrix for this camera
         * @return Projection matrix (camera to clip space)
         */
        glm::mat4 GetProjectionMatrix() const {
            if (UsePerspective) {
                return glm::perspective(glm::radians(FOV), AspectRatio, NearPlane, FarPlane);
            } else {
                float halfWidth = OrthoSize * AspectRatio;
                float halfHeight = OrthoSize;
                return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, NearPlane, FarPlane);
            }
        }

        /**
         * @brief Get combined view-projection matrix
         * @return View-projection matrix
         */
        glm::mat4 GetViewProjectionMatrix() const {
            return GetProjectionMatrix() * GetViewMatrix();
        }

        // ====================================================================
        // Direction Vectors
        // ====================================================================

        /**
         * @brief Get the forward direction vector
         * @return Normalized forward vector in world space
         */
        glm::vec3 GetForward() const {
            return Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        }

        /**
         * @brief Get the right direction vector
         * @return Normalized right vector in world space
         */
        glm::vec3 GetRight() const {
            return Rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        }

        /**
         * @brief Get the up direction vector
         * @return Normalized up vector in world space
         */
        glm::vec3 GetUp() const {
            return Rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        }

        // ====================================================================
        // Picking / Ray Casting
        // ====================================================================

        /**
         * @brief Convert screen coordinates to a world-space ray
         * @param screenPos Screen position (in pixels)
         * @param viewportWidth Viewport width in pixels
         * @param viewportHeight Viewport height in pixels
         * @return Ray from camera through the screen point
         */
        Ray ScreenPointToRay(glm::vec2 screenPos, uint32_t viewportWidth, uint32_t viewportHeight) const {
            // Normalize to [-1, 1]
            float x = (2.0f * screenPos.x) / viewportWidth - 1.0f;
            float y = 1.0f - (2.0f * screenPos.y) / viewportHeight;

            // Clip space
            glm::vec4 clipCoords(x, y, -1.0f, 1.0f);

            // Eye space
            glm::mat4 invProj = glm::inverse(GetProjectionMatrix());
            glm::vec4 eyeCoords = invProj * clipCoords;
            eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f);

            // World space
            glm::mat4 invView = glm::inverse(GetViewMatrix());
            glm::vec4 rayWorld = invView * eyeCoords;
            glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld));

            return Ray{ Position, rayDir };
        }

        // ====================================================================
        // Utility Methods
        // ====================================================================

        /**
         * @brief Set camera to look at a specific point
         * @param target Point to look at in world space
         * @param up Up vector (default: world up)
         */
        void LookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f)) {
            glm::vec3 direction = glm::normalize(target - Position);
            glm::vec3 right = glm::normalize(glm::cross(direction, up));
            glm::vec3 actualUp = glm::cross(right, direction);
            
            glm::mat3 rotationMatrix;
            rotationMatrix[0] = right;
            rotationMatrix[1] = actualUp;
            rotationMatrix[2] = -direction;
            
            Rotation = glm::quat_cast(rotationMatrix);
        }

        /**
         * @brief Set camera aspect ratio based on viewport size
         * @param width Viewport width
         * @param height Viewport height
         */
        void SetAspectRatio(float width, float height) {
            if (height > 0.0f) {
                AspectRatio = width / height;
            }
        }
    };

}

#endif
