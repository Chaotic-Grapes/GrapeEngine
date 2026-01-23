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
#include "ecs/Entity.h"
#include <unordered_map>

// Forward declarations
namespace ECS { class RendererSystem; }
class Framebuffer;

namespace Editor {

    // Type-safe callback wrapper for pick results
    using PickResultCallback = void(*)(uint32_t entityId, void* userData);

    /**
     * @brief GPU-based entity picking for editor viewports
     * 
     * This utility handles viewport-space to FBO-space coordinate conversion
     * and reads from the engine's picking framebuffer.
     * 
     * Internally manages polling of pending requests to provide a callback-based
     * API without requiring viewport to poll every frame.
     * 
     * Note: Uses ECS::Entity::NPOS32 as the "no entity picked" sentinel value
     * to allow picking entity 0.
     */
    class ViewportPicking {
    public:
        /**
         * @brief Pick entity at screen position within viewport bounds (callback-based)
         * @param screenX Screen X coordinate (absolute)
         * @param screenY Screen Y coordinate (absolute)
         * @param viewportPos Viewport position in screen space
         * @param viewportSize Viewport size in pixels
         * @param rendererSystem Renderer system to query picking FBO from
         * @param callback C-style callback invoked when result is ready (DLL-safe)
         * @param userData User data pointer passed to callback
         * @return Request ID (non-zero for valid requests), or 0 if request failed
         */
        static uint32_t RequestAsyncPick(
            float screenX,
            float screenY,
            const glm::vec2& viewportPos,
            const glm::vec2& viewportSize,
            ECS::RendererSystem* rendererSystem,
            PickResultCallback callback,
            void* userData);

        /**
         * @brief Update internal polling - call once per frame from editor update
         * Checks all pending pick requests and invokes callbacks when ready
         */
        static void Update(ECS::RendererSystem* rendererSystem);

    private:
        // Use NPOS32 as sentinel for "no entity picked" to allow picking entity 0
        static constexpr uint32_t INVALID_ENTITY_ID = ECS::Entity::NPOS32;
        
        // Track pending requests
        struct PendingRequest {
            uint32_t requestId;
            PickResultCallback callback;
            void* userData;
        };
        
        static std::unordered_map<uint32_t, PendingRequest> s_pendingRequests;
    };

}

#endif
