/* Start Header *****************************************************************/
/*!
\file   LightManager.cpp
\author Choi Meng Yew
\date   31st January 2026
\brief
Implementation of the LightManager, responsible for uploading and binding
dynamic lighting data to the GPU for forward rendering.

Details:
This file implements the CPU-side logic for managing scene lighting each frame.
The LightManager collects directional and point light data, converts it into
GPU-compatible layouts, uploads it to Shader Storage Buffer Objects (SSBOs),
and binds the required buffers and uniforms to active shaders.

Specifically, this implementation handles:
- Creation and destruction of GPU buffers for point lights
- Per-frame light collection, clearing, and dirty-state tracking
- Upload of point light data to SSBOs using memcpy-safe POD structures
- Binding of SSBOs and small lighting uniforms (counts, flags, directional light)
  to shaders prior to rendering

The LightManager contains no shading logic itself; it serves purely as the data
bridge between ECS light components and the GPU lighting stage.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "graphics/LightManager.hpp"
#include "graphics/Shader.hpp"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>
#include <cassert>

namespace Graphics {

    // ------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------

    LightManager::LightManager() = default;

    LightManager::~LightManager()
    {
        Shutdown();
    }

    void LightManager::Initialize()
    {
        if (m_initialized)
            return;

        // Create SSBO for point lights
        glGenBuffers(1, &m_pointLightSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pointLightSSBO);
        glBufferData(
            GL_SHADER_STORAGE_BUFFER,
            sizeof(GPUPointLight) * LightingConfig::MAX_POINT_LIGHTS,
            nullptr,
            GL_DYNAMIC_DRAW
        );
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        m_initialized = true;
    }

    void LightManager::Shutdown()
    {
        if (m_pointLightSSBO) {
            glDeleteBuffers(1, &m_pointLightSSBO);
            m_pointLightSSBO = 0;
        }

        m_initialized = false;
    }

    // ------------------------------------------------------------
    // Per-frame flow
    // ------------------------------------------------------------

    void LightManager::BeginFrame()
    {
        m_pointLights.clear();
        m_hasDirectional = false;
        m_dirty = false;
    }

    void LightManager::SetDirectionalLight(const glm::vec3& direction,
        const glm::vec3& color,
        float intensity)
    {
        m_directional.Direction = glm::vec4(glm::normalize(direction), 0.0f);
        m_directional.ColorAndIntensity = glm::vec4(color, intensity);

        m_hasDirectional = true;
        m_dirty = true;
    }

    void LightManager::ClearDirectionalLight()
    {
        m_hasDirectional = false;
        m_dirty = true;
    }

    void LightManager::AddPointLight(const glm::vec3& position,
        float range,
        const glm::vec3& color,
        float intensity)
    {
        if (m_pointLights.size() >= LightingConfig::MAX_POINT_LIGHTS)
            return;

        GPUPointLight light;
        light.PositionAndRange = glm::vec4(position, range);
        light.ColorAndIntensity = glm::vec4(color, intensity);

        m_pointLights.push_back(light);
        m_dirty = true;
    }

    void LightManager::Upload()
    {
        if (!m_initialized || !m_dirty)
            return;

        UpdatePointLightBuffer();
        m_dirty = false;
    }

    // ------------------------------------------------------------
    // Binding
    // ------------------------------------------------------------

    void LightManager::Bind(Shader& shader)
    {
        if (!m_initialized)
            return;

        shader.use();

        // Bind point light SSBO at binding = 0
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0,
            m_pointLightSSBO
        );

        // Small uniforms
        shader.setUniform("uPointLightCount",
            static_cast<int>(m_pointLights.size()));
        shader.setUniform("uHasDirectional",
            m_hasDirectional ? 1 : 0);
        shader.setUniform("uLightingEnabled", 1);

        if (m_hasDirectional) {
            shader.setUniform("uDirLight.Direction",
                glm::vec3(m_directional.Direction));
            shader.setUniform("uDirLight.Color",
                glm::vec3(m_directional.ColorAndIntensity));
            shader.setUniform("uDirLight.Intensity",
                m_directional.ColorAndIntensity.w);
        }
    }

    // ------------------------------------------------------------
    // Internals
    // ------------------------------------------------------------

    void LightManager::UpdatePointLightBuffer()
    {
        if (m_pointLights.empty())
            return;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pointLightSSBO);
        glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            sizeof(GPUPointLight) * m_pointLights.size(),
            m_pointLights.data()
        );
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

} // namespace Graphics
