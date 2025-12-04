/* Start Header *****************************************************************/
/*!
\file   ViewportPicking.h
\author Choi Meng Yew
\par    choi.m@digipen.edu
\brief
Editor-side GPU picking utility for viewport interaction.
This provides viewport-aware entity picking without coupling the engine to editor.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef VIEWPORT_PICKING_H
#define VIEWPORT_PICKING_H

#include <glm/glm.hpp>
#include <cstdint>

// Forward declarations
namespace ECS { class RendererSystem; }
class Framebuffer;

namespace Editor {

    /**
     * @brief GPU-based entity picking for editor viewports
     * 
     * This utility handles viewport-space to FBO-space coordinate conversion
     * and reads from the engine's picking framebuffer.
     */
    class ViewportPicking {
    public:
        /**
         * @brief Pick entity at screen position within viewport bounds
         * @param screenX Screen X coordinate (absolute)
         * @param screenY Screen Y coordinate (absolute)
         * @param viewportPos Viewport position in screen space
         * @param viewportSize Viewport size in pixels
         * @param rendererSystem Renderer system to query picking FBO from
         * @return Entity ID at that position, or 0 if none
         */
        static uint32_t PickEntityAtScreenPosition(
            float screenX, 
            float screenY,
            const glm::vec2& viewportPos,
            const glm::vec2& viewportSize,
            ECS::RendererSystem* rendererSystem);

    private:
        static constexpr uint32_t INVALID_ENTITY_ID = 0;
    };

}

#endif
