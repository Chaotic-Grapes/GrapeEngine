#pragma once

#include "LightingStructures.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

// Forward declaration
class Shader;

namespace Graphics {

    // Forward lighting manager.
    // Collects ECS lights, converts them to GPU layouts,
    // uploads buffers, and binds lighting data for shaders.
    class LightManager {
    public:
        LightManager();
        ~LightManager();

        // GPU resource lifecycle
        void Initialize();     // create GPU buffers once
        void Shutdown();       // destroy GPU buffers

        // Per-frame flow
        void BeginFrame();          // clear collected lights for the frame
        void Upload();              // upload CPU light data to GPU if changed
        void Bind(Shader& shader);  // bind buffers + uniforms to active shader

        // Authoring API (called by RendererSystem after reading ECS Light2D)
        void SetDirectionalLight(const glm::vec3& direction,
            const glm::vec3& color,
            float intensity);

        void ClearDirectionalLight();

        void AddPointLight(const glm::vec3& position,
            float range,
            const glm::vec3& color,
            float intensity);

        // Debug / stats stuff
        uint32_t GetPointLightCount() const {
            return static_cast<uint32_t>(m_pointLights.size());
        }

        bool HasDirectionalLight() const {
            return m_hasDirectional;
        }

    private:
        void UpdatePointLightBuffer();

    private:
        // CPU-side collected data (per-frame)
        std::vector<GPUPointLight> m_pointLights;

        // Directional light (usually 0 or 1)
        GPUDirectionalLight m_directional{};
        bool m_hasDirectional = false;

        // GPU buffers
        uint32_t m_pointLightSSBO = 0;

        // State
        bool m_initialized = false;
        bool m_dirty = false;
    };

} // namespace Graphics
