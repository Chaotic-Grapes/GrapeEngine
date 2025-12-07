/* Start Header *****************************************************************/
/*!
\file   ViewportPicking.cpp
\author Choi Meng Yew
\par    choi.m@digipen.edu
\brief
Implementation of editor-side GPU picking utility.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ViewportPicking.h"
#include "ecs/systems/RendererSystem.h"
#include "graphics/framebuffer.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Editor {

    uint32_t ViewportPicking::PickEntityAtScreenPosition(
        float screenX, 
        float screenY,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        ECS::RendererSystem* rendererSystem)
    {
        if (!rendererSystem) {
            return INVALID_ENTITY_ID;
        }

        // Get the picking FBO from the renderer
        const Framebuffer* pickingFBO = rendererSystem->GetPickingFBO();
        if (!pickingFBO) {
            return INVALID_ENTITY_ID;
        }

        // Convert screen coordinates to viewport-local coordinates
        glm::vec2 localPos = glm::vec2(screenX, screenY) - viewportPos;

        // Check if position is within viewport bounds
        if (localPos.x < 0 || localPos.y < 0 || 
            localPos.x >= viewportSize.x || localPos.y >= viewportSize.y) {
            return INVALID_ENTITY_ID;
        }

        // Convert to FBO coordinates (flip Y for OpenGL)
        int x = static_cast<int>(localPos.x);
        int y = static_cast<int>(viewportSize.y - localPos.y);

        // Clamp to FBO bounds
        int vpWidth = static_cast<int>(viewportSize.x);
        int vpHeight = static_cast<int>(viewportSize.y);
        x = glm::clamp(x, 0, vpWidth - 1);
        y = glm::clamp(y, 0, vpHeight - 1);

        // Verify FBO size matches viewport
        if (pickingFBO->Width() != vpWidth || pickingFBO->Height() != vpHeight) {
            // FBO size mismatch, picking not ready
            return INVALID_ENTITY_ID;
        }

        // IMPORTANT: The RendererSystem uses Pixel Buffer Objects (PBOs) for async picking.
        // This synchronous read will stall the GPU pipeline. For proper implementation,
        // coordinate with RendererSystem's PBO-based async picking instead.
        // For now, we perform a synchronous read as a fallback.
        
        GLint previousFBO = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);

        // Read pixel from picking FBO
        // Note: We're const_cast'ing here because OpenGL's bind/unbind aren't const-correct
        // but we're only reading, not modifying the FBO
        Framebuffer* mutableFBO = const_cast<Framebuffer*>(pickingFBO);
        mutableFBO->Bind();
        
        // Ensure we're reading from the color attachment
        GLint readBuffer = 0;
        glGetIntegerv(GL_READ_BUFFER, &readBuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        
        unsigned char pixel[4] = {0, 0, 0, 0};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        
        // Restore read buffer and FBO
        glReadBuffer(readBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);

        // Decode entity ID from RGB channels
        // The picking pass encodes entity IDs as: ID + 1 (so 0 = no entity)
        uint32_t pickedID = pixel[0] | (pixel[1] << 8) | (pixel[2] << 16);

        if (pickedID > 0) {
            return pickedID - 1;  // Convert back to entity index
        }

        return INVALID_ENTITY_ID;
    }

}
