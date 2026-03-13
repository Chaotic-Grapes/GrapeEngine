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
        static void Create(const std::string& name, Engine::Camera* camera, int w, int h);
        static void Destroy(const std::string& name);
        static void Resize(const std::string& name, int w, int h);
        static void SetCamera(const std::string& name, Engine::Camera* camera);
        static uint32_t GetTexture(const std::string& name);
        static uint32_t Pick(const std::string& name, float localX, float localY);
        static bool GetPickResult(uint32_t requestId, uint32_t& outEntityId);
        static glm::vec2 ScreenToWorld(const std::string& name, float localX, float localY);
    };

}