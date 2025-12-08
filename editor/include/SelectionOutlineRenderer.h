/* Start Header *****************************************************************/
/*!
\file   SelectionOutlineRenderer.h
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu

\brief
Editor utility for rendering selection wireframe outlines around selected entities.

This is editor-only visualization that draws a yellow wireframe box around the
currently selected entity to provide visual feedback.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SELECTION_OUTLINE_RENDERER_H
#define SELECTION_OUTLINE_RENDERER_H

#include "ecs/World.h"
#include <glm/glm.hpp>

// Forward declarations
class Renderer;
class Shader;

namespace Editor {

    /**
     * @brief Renders a wireframe outline around the selected entity
     */
    class SelectionOutlineRenderer {
    public:
        /**
         * @brief Render selection outline for the given entity
         * 
         * @param world ECS world containing entities
         * @param selectedEntityID ID of selected entity (0 = none)
         * @param renderer Batch renderer to use
         * @param shader Shader to use for rendering
         * @param viewProj Combined view-projection matrix
         * @param cameraOrthoSize Camera orthographic size for thickness calculation
         * @param windowHeight Window height for pixel-perfect thickness
         */
        static void RenderOutline(
            ECS::World& world,
            uint32_t selectedEntityID,
            Renderer* renderer,
            Shader* shader,
            const glm::mat4& viewProj,
            float cameraOrthoSize,
            float windowWidth,
            float windowHeight
        );
    };

}

#endif
