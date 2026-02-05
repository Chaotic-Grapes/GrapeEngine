/* Start Header *****************************************************************/
/*!
\file   CameraFrustumRenderer.h
\author Choi Meng Yew
\par    choi.m@digipen.edu
\brief
Editor utility to visualize game camera frustum in the scene viewport.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef CAMERA_FRUSTUM_RENDERER_H
#define CAMERA_FRUSTUM_RENDERER_H

#include "ecs/World.h"
#include <glm/glm.hpp>
#include <cstdint>

// Forward declarations
namespace ECS { class RendererSystem; }

namespace Editor {

    /**
     * @brief Utility for rendering game camera frustum visualization in editor
     * 
     * Draws a cyan rectangle showing what the game camera will capture,
     * useful for level design and camera placement.
     */
    class CameraFrustumRenderer {
    public:
        /**
         * @brief Render game camera frustum overlay
         * @param world ECS world containing camera entities
         * @param rendererSystem Renderer system used to queue wireframe overlays
         * @param cameraOrthoSize Editor camera's orthographic size (for thickness calculation)
         * @param windowHeight Window height in pixels (for thickness calculation)
         * @param excludeEntityId Entity ID to exclude from rendering (typically editor camera)
         */
        static void RenderFrustum(
            ECS::World& world,
            ECS::RendererSystem* rendererSystem,
            float cameraOrthoSize,
            float windowHeight,
            uint32_t excludeEntityId = 0);
    };

}

#endif
