// Viewport.hpp
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