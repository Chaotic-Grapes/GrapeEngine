/* Start Header *****************************************************************/
/*!
\file   GodRayPass.hpp
\author Choi Meng Yew
\date   5th April 2026
\brief
Radial-blur god ray post-processing pass for simulating light shafts from a
directional light source.

Details:
This file defines the GodRayPass system, which performs a multi-stage
post-processing effect to generate volumetric light shafts ("god rays")
in screen space.

The pass operates entirely on GPU-rendered textures and integrates into
the RenderGraph after tone mapping. The light source position is derived
automatically from the scene’s directional light via LightManager,
removing the need for manual configuration.

The implementation consists of three stages:
- Occluder Mask: Extracts bright occluders from the HDR scene
- Radial Blur: Accumulates light scattering toward the projected light source
- Composite: Additively blends the resulting light shafts onto the LDR scene

Additionally, this file defines:
- GodRaySettings, a POD configuration structure controlling sampling,
  decay, density, tint, and compositing strength
- GPU-side execution logic using fullscreen quad rendering
- Framebuffer management for intermediate mask and blur passes
- Light projection logic from world space to screen-space UV coordinates

This system is rendering-focused and contains no gameplay or ECS logic.
It serves as a GPU-driven post-processing stage that operates on existing
render targets to enhance visual realism.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#include "graphics/framebuffer.hpp"
#include "graphics/LightManager.hpp"
#include "services/ResourceManager.h"
#include "services/TimeSystem.h"

#include <memory>
#include <string>
#include <cmath>
#include <algorithm>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Shader;
class Framebuffer;

// -----------------------------------------------------------------------------
struct GodRaySettings
{
    // Populated automatically by UpdateLightPos() from LightManager.
    // Can be overridden manually if needed.
    glm::vec2 lightPosNDC = { 0.5f, 0.85f };

    // Camera world position - anchors shaft noise to the world, not the screen.
    // Update this each frame from your active camera entity position.
    glm::vec2 cameraWorldPos = { 0.0f, 0.0f };

    // Mask stage - luminance below this = empty space (white mask)
    float occluderThreshold = 0.05f;

    // Blur/ray stage
    int   samples = 80;
    float decay = 0.97f;
    float density = 0.5f;
    float weight = 0.012f;

    // Composite stage
    glm::vec3 tint = { 1.0f, 0.90f, 0.70f }; // warm sunlight
    float strength = 0.45f;

    // When true, Execute() does nothing if the scene has no directional light.
    bool requireDirectionalLight = true;
};

// -----------------------------------------------------------------------------
class GodRayPass
{
public:
    GodRayPass() = default;
    ~GodRayPass() { Destroy(); }

    GodRayPass(const GodRayPass&) = delete;
    GodRayPass& operator=(const GodRayPass&) = delete;

    // -------------------------------------------------------------------------
    // Init - call once after the GL context is ready
    // -------------------------------------------------------------------------
    void Init(const std::string& vertPath = "assets/shaders/fullscreen.vert",
        const std::string& maskFrag = "assets/shaders/godray_mask.frag",
        const std::string& blurFrag = "assets/shaders/godray_blur.frag",
        const std::string& compFrag = "assets/shaders/godray_composite.frag")
    {
        m_maskShader = RM.GetShader(vertPath, maskFrag);
        m_blurShader = RM.GetShader(vertPath, blurFrag);
        m_compositeShader = RM.GetShader(vertPath, compFrag);

        m_maskFBO = std::make_unique<Framebuffer>();
        m_blurFBO = std::make_unique<Framebuffer>();
        m_ldrScratch = std::make_unique<Framebuffer>();

        m_maskFBO->Create(1, 1, false, false, 1);
        m_blurFBO->Create(1, 1, false, false, 1);
        m_ldrScratch->Create(1, 1, false, false, 1);

        BuildQuad();
    }

    // -------------------------------------------------------------------------
    // SetLightDirection - forward the directional light's travel direction here
    // from RendererSystem::CollectLights(), right after calling
    // m_lightManager.SetDirectionalLight(...).
    //
    //   direction: the direction the light TRAVELS toward the scene,
    //              e.g. glm::vec3(0,-1,0) = light coming from above.
    // -------------------------------------------------------------------------
    void SetLightDirection(const glm::vec3& direction)
    {
        if (glm::dot(direction, direction) > 1e-8f)
            m_cachedLightDir = glm::normalize(direction);
    }

    void SetLightProperties(const glm::vec3& direction, const glm::vec3& color, float intensity)
    {
        SetLightDirection(direction);
        m_settings.tint = color;
        m_settings.strength = intensity;
    }

    // -------------------------------------------------------------------------
    // UpdateLightPos - call once per frame, before Execute().
    //
    // Projects the reversed light direction to screen space to find the
    // "sun" position in NDC/UV coordinates for the radial blur origin.
    // -------------------------------------------------------------------------
    void UpdateLightPos(const glm::mat4& viewProj,
        const Graphics::LightManager& lightManager)
    {
        m_hasLight = lightManager.HasDirectionalLight();
        if (!m_hasLight) return;

        // For 2D ortho, project a point a short distance in the reverse light direction
        // Use a small distance that stays within the ortho near/far range
        const float kDist = 100.0f;  // was 100000 - that caused -426 NDC
        glm::vec4 lightWorldPos(
            -m_cachedLightDir.x * kDist,
            -m_cachedLightDir.y * kDist,
            -m_cachedLightDir.z * kDist,  // include Z for 2D ortho
            1.0f
        );

        glm::vec4 clip = viewProj * lightWorldPos;
        if (std::abs(clip.w) > 1e-6f)
        {
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            // NDC (-1..1) to UV (0..1) - clamp to reasonable off-screen range
            m_settings.lightPosNDC = glm::vec2(
                ndc.x * 0.5f + 0.5f,
                ndc.y * 0.5f + 0.5f
            );
        }
    }

    // -------------------------------------------------------------------------
    // Execute
    //   hdrTex - GL texture ID of the HDR scene (source for occluder mask)
    //   ldrTex - GL texture ID of the tone-mapped LDR scene (unused directly,
    //            kept for API symmetry; rays are blended additively on top)
    //   target - framebuffer to write into (the LDR FBO)
    // -------------------------------------------------------------------------
    void Execute(GLuint hdrTex, GLuint /*ldrTex*/,
        Framebuffer& target, int w, int h)
    {
        if (!m_enabled) return;
        if (!m_maskShader || !m_blurShader || !m_compositeShader) return;
        if (m_settings.requireDirectionalLight && !m_hasLight) return;

        const int hw = (w / 2 > 1) ? (w / 2) : 1;
        const int hh = (h / 2 > 1) ? (h / 2) : 1;

        EnsureFBO(*m_maskFBO, hw, hh);
        EnsureFBO(*m_blurFBO, hw, hh);

        const float t = TimeSystem::Instance().GetTotalTime();

        // -----------------------------------------------------------
        // Stage 1: occluder mask
        // Bright objects -> black, empty space -> white
        // -----------------------------------------------------------
        m_maskFBO->Bind();
        glViewport(0, 0, hw, hh);
        m_maskShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrTex);
        m_maskShader->setUniform("uScene", 0);
        m_maskShader->setUniform("uThreshold", m_settings.occluderThreshold);
        DrawQuad();
        Framebuffer::Unbind();

        // -----------------------------------------------------------
        // Stage 2: radial blur toward light source
        // -----------------------------------------------------------
        m_blurFBO->Bind();
        glViewport(0, 0, hw, hh);
        m_blurShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_maskFBO->GetColorTexture(0));
        m_blurShader->setUniform("uOccluder", 0);
        m_blurShader->setUniform("uLightPosNDC", m_settings.lightPosNDC);
        m_blurShader->setUniform("uSamples", m_settings.samples);
        m_blurShader->setUniform("uDecay", m_settings.decay);
        m_blurShader->setUniform("uDensity", m_settings.density);
        m_blurShader->setUniform("uWeight", m_settings.weight);
        m_blurShader->setUniform("uTime", t);
        m_blurShader->setUniform("uCameraWorldPos", m_settings.cameraWorldPos);
        DrawQuad();
        Framebuffer::Unbind();

        // -----------------------------------------------------------
        // Stage 3: additive composite onto target (LDR FBO)
        // The composite shader only outputs the ray contribution (alpha 0),
        // so GL_ONE/GL_ONE blend adds it cleanly on top of existing content.
        // -----------------------------------------------------------
        target.Bind();
        glViewport(0, 0, w, h);

        GLboolean blendWas = glIsEnabled(GL_BLEND);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        m_compositeShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_blurFBO->GetColorTexture(0));
        m_compositeShader->setUniform("uGodRays", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_maskFBO->GetColorTexture(0));
        m_compositeShader->setUniform("uOccluder", 1);

        m_compositeShader->setUniform("uTint", m_settings.tint);
        m_compositeShader->setUniform("uStrength", m_settings.strength);

        DrawQuad();

        if (!blendWas) glDisable(GL_BLEND);
        else           glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        Framebuffer::Unbind();
    }

    // Runtime settings
    GodRaySettings& Settings() { return m_settings; }

    void SetEnabled(bool v) { m_enabled = v; }
    bool IsEnabled()  const { return m_enabled; }

    // -------------------------------------------------------------------------
    void Destroy()
    {
        if (m_quadVAO) { glDeleteVertexArrays(1, &m_quadVAO); m_quadVAO = 0; }
        if (m_quadVBO) { glDeleteBuffers(1, &m_quadVBO);  m_quadVBO = 0; }
        m_maskFBO.reset();
        m_blurFBO.reset();
        m_ldrScratch.reset();
        m_maskShader.reset();
        m_blurShader.reset();
        m_compositeShader.reset();
    }

private:
    // -------------------------------------------------------------------------
    static void EnsureFBO(Framebuffer& fbo, int w, int h)
    {
        if (fbo.Width() != w || fbo.Height() != h)
            fbo.Resize(w, h, false, false);
    }

    // -------------------------------------------------------------------------
    void DrawQuad() const
    {
        glBindVertexArray(m_quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

    // -------------------------------------------------------------------------
    void BuildQuad()
    {
        // location 0 = aPos (vec2), location 1 = aUV (vec2)
        // Matches fullscreen.vert attribute layout
        static const float kVerts[] = {
            -1.f, -1.f,  0.f, 0.f,
             1.f, -1.f,  1.f, 0.f,
            -1.f,  1.f,  0.f, 1.f,
             1.f,  1.f,  1.f, 1.f,
        };

        glGenVertexArrays(1, &m_quadVAO);
        glGenBuffers(1, &m_quadVBO);

        glBindVertexArray(m_quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kVerts), kVerts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

        glBindVertexArray(0);
    }

    // -------------------------------------------------------------------------
    std::shared_ptr<Shader>      m_maskShader;
    std::shared_ptr<Shader>      m_blurShader;
    std::shared_ptr<Shader>      m_compositeShader;

    std::unique_ptr<Framebuffer> m_maskFBO;
    std::unique_ptr<Framebuffer> m_blurFBO;
    std::unique_ptr<Framebuffer> m_ldrScratch;

    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;

    GodRaySettings m_settings;
    glm::vec3      m_cachedLightDir = { 0.0f, -1.0f, 0.0f }; // light from above
    bool           m_hasLight = false;
    bool           m_enabled = true;
};