/* Start Header *****************************************************************/
/*!
\file   PickingQueryManager.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of picking query manager with caching and validation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "PickingQueryManager.h"
#include "ecs/systems/RendererSystem.h"
#include "core/Logger.h"
#include <algorithm>

namespace Editor {

    PickingQueryManager::PickingQueryManager()
        : m_nextRequestId(1)
        , m_viewportPos(0.0f, 0.0f)
        , m_viewportSize(1.0f, 1.0f)
        , m_rendererSystem(nullptr)
        , m_lastValidatedViewportPos(0.0f, 0.0f)
        , m_lastValidatedViewportSize(1.0f, 1.0f) {
    }

    PickingQueryManager::~PickingQueryManager() {
        ClearCache();
    }

    // Submit a pick request at the given screen position; returns a request ID to poll with TryGetPickResult, or 0 on failure
    uint32_t PickingQueryManager::RequestPick(
        float screenX,
        float screenY,
        const glm::vec2& viewportPos,
        const glm::vec2& viewportSize,
        ECS::RendererSystem* rendererSystem) {

        // Update viewport bounds if changed
        if (viewportPos != m_viewportPos || viewportSize != m_viewportSize) {
            SetViewportBounds(viewportPos, viewportSize);
        }

        m_rendererSystem = rendererSystem;

        // Validate screen position is within viewport bounds
        if (!_isScreenPositionValid(screenX, screenY)) {
            LOG_WARNING("PickingQueryManager: Pick request outside viewport bounds");
            return 0;  // Invalid request
        }

        // Validate renderer system is available
        if (!rendererSystem) {
            LOG_WARNING("PickingQueryManager: No renderer system available");
            return 0;  // Invalid request
        }

        // Submit pick request to renderer
        // NOTE: RendererSystem::RequestPick expects screen coordinates, not viewport-relative coordinates!
        uint32_t rendererRequestId = rendererSystem->RequestPick(
            screenX,
            screenY,
            viewportPos,
            viewportSize);

        // Check if renderer accepted the request (invalid if 0 or UINT32_MAX)
        if (rendererRequestId == 0 || rendererRequestId == UINT32_MAX) {
            LOG_WARNING("PickingQueryManager: Renderer rejected pick request");
            return 0;  // Request failed
        }

        // Create our own request ID and track it
        uint32_t ourRequestId = m_nextRequestId++;
        if (m_nextRequestId == 0) m_nextRequestId = 1;  // Wrap around, skip 0

        PendingRequest pending;
        pending.rendererRequestId = rendererRequestId;
        m_pendingRequests[ourRequestId] = pending;

        return ourRequestId;
    }

    // Check the cache or poll the renderer for a completed pick result; returns true when ready
    bool PickingQueryManager::TryGetPickResult(uint32_t requestId, uint32_t& outEntityId) {
        // Check cache first
        auto cacheIt = m_resultCache.find(requestId);
        if (cacheIt != m_resultCache.end()) {
            outEntityId = cacheIt->second;
            return true;
        }

        // Check if request is pending
        auto pendingIt = m_pendingRequests.find(requestId);
        if (pendingIt == m_pendingRequests.end()) {
            LOG_WARNING("PickingQueryManager: Unknown request ID: " << requestId);
            return false;  // Unknown request
        }

        // Try to get result from renderer
        if (!m_rendererSystem) {
            return false;  // No renderer system, can't get result
        }

        uint32_t rendererRequestId = pendingIt->second.rendererRequestId;
        uint32_t entityId;
        if (!m_rendererSystem->TryGetPickResult(rendererRequestId, entityId)) {
            return false;  // Result not ready yet
        }

        // Result is ready! Cache it and remove from pending
        m_resultCache[requestId] = entityId;
        m_pendingRequests.erase(pendingIt);

        outEntityId = entityId;
        return true;
    }

    // Discard all cached results and pending pick requests
    void PickingQueryManager::ClearCache() {
        m_resultCache.clear();
        m_pendingRequests.clear();
    }

    // Update the viewport rectangle used for position validation; clears the cache if bounds changed significantly
    void PickingQueryManager::SetViewportBounds(const glm::vec2& viewportPos, const glm::vec2& viewportSize) {
        // Check if bounds changed significantly (more than 1 pixel)
        glm::vec2 posDiff = glm::abs(viewportPos - m_lastValidatedViewportPos);
        glm::vec2 sizeDiff = glm::abs(viewportSize - m_lastValidatedViewportSize);

        if (posDiff.x > 1.0f || posDiff.y > 1.0f || sizeDiff.x > 1.0f || sizeDiff.y > 1.0f) {
            // Bounds changed significantly, invalidate cache
            ClearCache();
        }

        m_viewportPos = viewportPos;
        m_viewportSize = viewportSize;
        m_lastValidatedViewportPos = viewportPos;
        m_lastValidatedViewportSize = viewportSize;
    }

    // Return the number of pick requests that have been submitted but not yet resolved
    size_t PickingQueryManager::GetPendingRequestCount() const {
        return m_pendingRequests.size();
    }

    // Return true if the given screen position falls within the current viewport bounds
    bool PickingQueryManager::_isScreenPositionValid(float screenX, float screenY) const {
        // Check if position is within viewport bounds
        if (screenX < m_viewportPos.x || screenX > m_viewportPos.x + m_viewportSize.x) {
            return false;
        }
        if (screenY < m_viewportPos.y || screenY > m_viewportPos.y + m_viewportSize.y) {
            return false;
        }
        return true;
    }

    // Convert absolute screen coordinates to coordinates relative to the viewport's top-left corner
    glm::vec2 PickingQueryManager::_screenToViewportSpace(float screenX, float screenY) const {
        // Convert absolute screen position to viewport-relative position
        float viewportX = screenX - m_viewportPos.x;
        float viewportY = screenY - m_viewportPos.y;
        return glm::vec2(viewportX, viewportY);
    }

}  // namespace Editor
