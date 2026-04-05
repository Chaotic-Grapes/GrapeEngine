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

    // Static storage for pending requests
    std::unordered_map<uint32_t, ViewportPicking::PendingRequest> ViewportPicking::s_pendingRequests;

    /**
     * @brief Submit an asynchronous GPU pick request and register a callback to receive the result.
     * @param screenX Screen-space X coordinate of the pick position.
     * @param screenY Screen-space Y coordinate of the pick position.
     * @param viewportPos Top-left position of the viewport in screen space.
     * @param viewportSize Dimensions of the viewport in pixels.
     * @param rendererSystem The renderer system to submit the pick request to.
     * @param callback Function invoked with the picked entity ID when the result is ready.
     * @param userData Arbitrary pointer passed through to the callback.
     * @return Request ID that can be used to track the pending pick, or 0 on failure.
     */
    uint32_t ViewportPicking::RequestAsyncPick(
        float screenX,
        float screenY,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        ECS::RendererSystem* rendererSystem,
        PickResultCallback callback,
        void* userData)
    {
        if (!rendererSystem || !callback) return 0;
        
        // Request pick from renderer (uses existing 4-parameter API)
        uint32_t requestId = rendererSystem->RequestPick(screenX, screenY, viewportPos, viewportSize);
        
        if (requestId != 0) {
            // Store callback and user data for this request
            s_pendingRequests[requestId] = PendingRequest{ requestId, callback, userData };
        }
        
        return requestId;
    }

    /**
     * @brief Poll all pending async pick requests and invoke their callbacks when results are ready.
     * @param rendererSystem The renderer system to query for completed pick results.
     */
    void ViewportPicking::Update(ECS::RendererSystem* rendererSystem) {
        if (!rendererSystem || s_pendingRequests.empty()) return;
        
        // Check all pending requests
        auto it = s_pendingRequests.begin();
        while (it != s_pendingRequests.end()) {
            uint32_t entityId = INVALID_ENTITY_ID;
            
            // Poll for result
            if (rendererSystem->TryGetPickResult(it->second.requestId, entityId)) {
                // Result ready - invoke callback
                it->second.callback(entityId, it->second.userData);
                
                // Remove from pending list
                it = s_pendingRequests.erase(it);
            } else {
                ++it;
            }
        }
    }

}
