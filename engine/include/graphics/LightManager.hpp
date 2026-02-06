/* Start Header *****************************************************************/
/*!
\file   LightManager.hpp
\author Choi Meng Yew
\date   31st January 2026
\brief
Declaration of the LightManager, responsible for managing dynamic lighting data
and binding GPU-ready light buffers for the renderer.

Details:
The LightManager acts as the CPU-side controller for forward-rendered lighting.
It collects light data from ECS components, converts them into GPU-compatible
layouts, uploads them to Shader Storage Buffer Objects (SSBOs), and binds the
required buffers and uniforms to shaders each frame.

Specifically, this class:
- Manages directional and point lights for physically based rendering
- Maintains per-frame CPU-side light collections
- Uploads light data to the GPU using stable, POD lighting structures
- Exposes a clean authoring API for the renderer to submit lights
- Binds lighting buffers and counts to shaders at render time

This class contains no rendering logic or shading code; it exists purely to
mediate data flow between ECS light components and GPU shaders.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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
