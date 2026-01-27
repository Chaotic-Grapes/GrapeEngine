/* Start Header *****************************************************************/
/*!
\file   PickingQueryManager.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Manager for GPU-based entity picking queries with result caching.

This utility wraps the RendererSystem's async picking API (necessary for DLL
crossing) and provides a simpler interface with result caching to reduce the
complexity of viewport interaction code.

Internally, it manages pending pick requests and caches results to avoid
redundant queries on the same frame.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef PICKING_QUERY_MANAGER_H
#define PICKING_QUERY_MANAGER_H

#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_map>
#include <queue>

// Forward declarations
namespace ECS { class RendererSystem; }

namespace Editor {

    /**
     * @brief Manages entity picking queries with caching and validation
     * 
     * This class provides a simplified interface over RendererSystem's async
     * picking API. It handles:
     * - Request management (track pending queries)
     * - Result caching (avoid redundant queries per-frame)
     * - Viewport validation (bounds checking)
     * - Callback routing (when result becomes available)
     * 
     * Usage:
     * ```cpp
     * // Request a pick at screen position
     * uint32_t requestId = manager.RequestPick(
     *     screenX, screenY,
     *     viewportPos, viewportSize,
     *     rendererSystem);
     * 
     * // Later, check if result is ready
     * uint32_t entityId;
     * if (manager.TryGetPickResult(requestId, entityId)) {
     *     // Result is ready, entityId contains the picked entity (or NPOS32 if nothing)
     * }
     * ```
     */
    class PickingQueryManager {
    public:
        PickingQueryManager();
        ~PickingQueryManager();

        // ====================================================================
        // Picking Operations
        // ====================================================================

        /**
         * @brief Request a pick at the given screen position
         * 
         * @param screenX Absolute screen X coordinate (pixels)
         * @param screenY Absolute screen Y coordinate (pixels)
         * @param viewportPos Viewport top-left in screen space
         * @param viewportSize Viewport size in pixels
         * @param rendererSystem Renderer system to query picking from
         * @return Request ID (non-zero) if valid; 0 if request failed
         * 
         * Note: Result will be available one frame after rendering completes
         * (due to PBO readback latency in the renderer).
         */
        uint32_t RequestPick(
            float screenX,
            float screenY,
            const glm::vec2& viewportPos,
            const glm::vec2& viewportSize,
            ECS::RendererSystem* rendererSystem);

        /**
         * @brief Try to get the result of a previous pick request
         * 
         * @param requestId ID returned from RequestPick()
         * @param outEntityId [OUT] ID of picked entity (ECS::Entity::NPOS32 if nothing picked)
         * @return true if result is ready; false if still pending
         */
        bool TryGetPickResult(uint32_t requestId, uint32_t& outEntityId);

        // ====================================================================
        // Cache Management
        // ====================================================================

        /**
         * @brief Clear all cached pick results
         * 
         * Call this when viewport is resized, camera moves significantly, or
         * when cached results should be invalidated for any reason.
         */
        void ClearCache();

        /**
         * @brief Notify manager that viewport bounds have changed
         * 
         * Automatically invalidates cached results if bounds change significantly.
         * 
         * @param viewportPos New viewport top-left position in screen space
         * @param viewportSize New viewport size in pixels
         */
        void SetViewportBounds(const glm::vec2& viewportPos, const glm::vec2& viewportSize);

        // ====================================================================
        // State Accessors
        // ====================================================================

        /**
         * @brief Get the number of pending pick requests
         */
        size_t GetPendingRequestCount() const;

        /**
         * @brief Get the current cache size
         */
        size_t GetCacheSize() const { return m_resultCache.size(); }

    private:
        // ====================================================================
        // Internal Helpers
        // ====================================================================

        /**
         * @brief Validate that a screen position is within viewport bounds
         * @return true if position is valid; false if outside viewport
         */
        bool _isScreenPositionValid(float screenX, float screenY) const;

        /**
         * @brief Convert screen space position to viewport space
         */
        glm::vec2 _screenToViewportSpace(float screenX, float screenY) const;

        // ====================================================================
        // State
        // ====================================================================

        // Pending pick requests: maps request ID → renderer system request ID
        struct PendingRequest {
            uint32_t rendererRequestId;
            // Could extend with callback, timestamp, etc.
        };
        std::unordered_map<uint32_t, PendingRequest> m_pendingRequests;

        // Cached results: maps request ID → entity ID
        std::unordered_map<uint32_t, uint32_t> m_resultCache;

        // Next request ID (non-zero)
        uint32_t m_nextRequestId = 1;

        // Current viewport bounds (for validation)
        glm::vec2 m_viewportPos{0.0f, 0.0f};
        glm::vec2 m_viewportSize{1.0f, 1.0f};

        // Renderer system reference (may be null)
        ECS::RendererSystem* m_rendererSystem = nullptr;

        // Track last viewport bounds for invalidation
        glm::vec2 m_lastValidatedViewportPos{0.0f, 0.0f};
        glm::vec2 m_lastValidatedViewportSize{1.0f, 1.0f};
    };

}  // namespace Editor

#endif  // PICKING_QUERY_MANAGER_H
