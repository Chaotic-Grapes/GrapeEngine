/* Start Header *****************************************************************/
/*!
\file   Viewport.hpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Defines the ViewportManager class for managing multiple viewports in the engine.
The ViewportManager allows for creating, resizing, and destroying viewports, as 
well as setting cameras and performing picking operations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once
#include "Export.h"
#include <string>
#include <cstdint>
#include <glm/vec2.hpp>

namespace Engine { class Camera; }

namespace Graphics {

    class GRAPEENGINE_API ViewportManager {
    public:
        /**
         * @brief Create a named viewport with an optional camera and initial size.
         * @param name Unique viewport name.
         * @param camera Camera used for rendering this viewport.
         * @param w Initial viewport width in pixels.
         * @param h Initial viewport height in pixels.
         * @return void
         */
        static void Create(const std::string& name, Engine::Camera* camera, int w, int h);

        /**
         * @brief Destroy a named viewport and release its render resources.
         * @param name Unique viewport name.
         * @return void
         */
        static void Destroy(const std::string& name);

        /**
         * @brief Resize a named viewport render targets.
         * @param name Unique viewport name.
         * @param w Target width in pixels.
         * @param h Target height in pixels.
         * @return void
         */
        static void Resize(const std::string& name, int w, int h);

        /**
         * @brief Assign the camera used by a named viewport.
         * @param name Unique viewport name.
         * @param camera Camera pointer (nullptr disables camera rendering).
         * @return void
         */
        static void SetCamera(const std::string& name, Engine::Camera* camera);

        /**
         * @brief Enable or disable bloom passes for a named viewport.
         * @param name Unique viewport name.
         * @param enabled True to run bloom extraction/blur; false to bypass bloom.
         * @return void
         */
        static void SetBloomEnabled(const std::string& name, bool enabled);

        /**
         * @brief Get the final LDR texture id for a named viewport.
         * @param name Unique viewport name.
         * @return Texture id, or 0 if unavailable.
         */
        static uint32_t GetTexture(const std::string& name);

        /**
         * @brief Queue an asynchronous pick request in viewport-local coordinates.
         * @param name Unique viewport name.
         * @param localX Local X position inside viewport.
         * @param localY Local Y position inside viewport.
         * @return Request id or UINT32_MAX when unavailable.
         */
        static uint32_t Pick(const std::string& name, float localX, float localY);

        /**
         * @brief Retrieve asynchronous pick completion.
         * @param requestId Pick request id returned by Pick.
         * @param outEntityId Output entity id hit by picking.
         * @return True when the result is ready.
         */
        static bool GetPickResult(uint32_t requestId, uint32_t& outEntityId);

        /**
         * @brief Convert viewport-local screen coordinates to world coordinates.
         * @param name Unique viewport name.
         * @param localX Local X position inside viewport.
         * @param localY Local Y position inside viewport.
         * @return World-space position in X/Y.
         */
        static glm::vec2 ScreenToWorld(const std::string& name, float localX, float localY);
    };

}