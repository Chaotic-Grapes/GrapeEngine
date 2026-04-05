#pragma once

/* WaterDistortionPass.h
 *
 * Drop-in post-processing pass. Reads fullscreen.vert + underwater.frag
 * and the Perlin noise texture you already have.
 *
 * Quick-start in RendererSystem:
 *
 *  // OnCreate:
 *  m_waterPass = std::make_unique<WaterDistortionPass>();
 *  m_waterPass->Init("assets/textures/Noise.png",
 *                    "assets/shaders/fullscreen.vert",
 *                    "assets/shaders/underwater.frag");
 *
 *  // In the RenderGraph, add a pass AFTER "LDR" is written:
 *  m_renderGraph->AddPass("WaterDistort", {"LDR"}, {"LDR"},
 *      [this](ResourceAccessor& res)
 *      {
 *          auto* ldr = res.GetFramebuffer("LDR");
 *          if (!ldr || !m_waterPass->IsEnabled()) return;
 *          m_waterPass->Execute(ldr->GetColorTexture(0), *ldr,
 *                               ldr->Width(), ldr->Height());
 *      });
 *
 *  // OnDestroy:
 *  m_waterPass.reset();
 */

#include "graphics/texture.hpp"
#include "graphics/framebuffer.hpp"
#include "services/ResourceManager.h"
#include "services/TimeSystem.h"

#include <memory>
#include <string>
#include <glad/glad.h>

 // Forward-declared from your engine
class Shader;
class Texture;
class Framebuffer;

// -----------------------------------------------------------------------------
struct WaterDistortionSettings
{
    // Layer A - broad, slow swell
    float driftStrength = 0.004f;
    float driftSpeed = 0.04f;
    float driftScale = 1.8f;

    // Layer B - tight, fast ripple
    float rippleStrength = 0.002f;
    float rippleSpeed = 0.10f;
    float rippleScale = 3.6f;

    // Chromatic aberration
    float chromaticStrength = 0.003f;

    // Caustics
    float causticStrength = 0.08f;
    float causticScale = 2.2f;
    float causticSpeed = 0.06f;

    // Tint
    glm::vec3 waterTint = { 0.05f, 0.22f, 0.35f };
    float tintStrength = 0.18f;

    // Depth fade (in UV.y space; 0..1 = full screen underwater)
    float depthFadeStart = 0.0f;
    float depthFadeEnd = 1.0f;
    float surfaceFoamBand = 0.05f;
};

// -----------------------------------------------------------------------------
class WaterDistortionPass
{
public:
    WaterDistortionPass() = default;
    ~WaterDistortionPass() { Destroy(); }

    // Non-copyable
    WaterDistortionPass(const WaterDistortionPass&) = delete;
    WaterDistortionPass& operator=(const WaterDistortionPass&) = delete;

    // Init - call once after OpenGL context is ready
    void Init(const std::string& noisePath = "assets/textures/Noise.png",
        const std::string& vertPath = "assets/shaders/fullscreen.vert",
        const std::string& fragPath = "assets/shaders/underwater.frag")
    {
        m_shader = RM.GetShader(vertPath, fragPath);
        m_noiseTexture = RM.Get<Texture>(noisePath);

        // Noise MUST tile - force GL_REPEAT regardless of RM defaults
        if (m_noiseTexture)
        {
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_noiseTexture->ID()));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        BuildQuad();

        // Scratch FBO for ping-pong (avoids reading and writing the same texture)
        m_scratch = std::make_unique<Framebuffer>();
        m_scratch->Create(1, 1, false, false, 1);
    }

    // Execute - full-screen, no mask
    void Execute(GLuint sceneTexID, Framebuffer& target, int w, int h)
    {
        ExecuteInternal(sceneTexID, 0u, false, target, w, h);
    }

    // Execute - with greyscale water mask (white = water, black = dry land)
    void Execute(GLuint sceneTexID, GLuint maskTexID,
        Framebuffer& target, int w, int h)
    {
        ExecuteInternal(sceneTexID, maskTexID, true, target, w, h);
    }

    // Runtime settings access
    WaterDistortionSettings& Settings() { return m_settings; }

    void SetEnabled(bool v) { m_enabled = v; }
    bool IsEnabled()  const { return m_enabled; }

    // Cleanup
    void Destroy()
    {
        if (m_quadVAO) { glDeleteVertexArrays(1, &m_quadVAO); m_quadVAO = 0; }
        if (m_quadVBO) { glDeleteBuffers(1, &m_quadVBO);  m_quadVBO = 0; }
        m_scratch.reset();
        m_shader.reset();
        m_noiseTexture.reset();
    }

private:
    // -------------------------------------------------------------------------
    void ExecuteInternal(GLuint sceneTexID, GLuint maskTexID, bool useMask,
        Framebuffer& target, int w, int h)
    {
        if (!m_enabled || !m_shader || !m_noiseTexture) return;

        // Resize scratch to match target if needed
        if (m_scratch->Width() != w || m_scratch->Height() != h)
            m_scratch->Resize(w, h, false, false);

        // Ping-pong: copy scene into scratch so we can read and write simultaneously
        m_scratch->Bind();
        glViewport(0, 0, w, h);
        {
            static std::shared_ptr<Shader> blitShader;
            if (!blitShader)
                blitShader = RM.GetShader("assets/shaders/fullscreen.vert",
                    "assets/shaders/blit.frag");
            if (blitShader)
            {
                blitShader->use();
                blitShader->setUniform("uTex", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sceneTexID);
                DrawQuad();
            }
        }
        Framebuffer::Unbind();

        // Distortion pass
        target.Bind();
        glViewport(0, 0, w, h);
        m_shader->use();

        UploadUniforms(w, h, useMask);

        // unit 0 - scene (from scratch)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_scratch->GetColorTexture(0));
        m_shader->setUniform("uScene", 0);

        // unit 1 - perlin noise
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_noiseTexture->ID()));
        m_shader->setUniform("uNoiseMap", 1);

        // unit 2 - optional mask
        m_shader->setUniform("uUseMask", useMask ? 1 : 0);
        if (useMask && maskTexID != 0)
        {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, maskTexID);
            m_shader->setUniform("uMask", 2);
        }

        DrawQuad();
        Framebuffer::Unbind();
    }

    // -------------------------------------------------------------------------
    void UploadUniforms(int w, int h, bool /*useMask*/) const
    {
        const auto& s = m_settings;
        const float t = static_cast<float>(TimeSystem::Instance().GetTotalTime());

        m_shader->setUniform("uTime", t);
        m_shader->setUniform("uResolution", glm::vec2(static_cast<float>(w), static_cast<float>(h)));

        m_shader->setUniform("uDriftStrength", s.driftStrength);
        m_shader->setUniform("uDriftSpeed", s.driftSpeed);
        m_shader->setUniform("uDriftScale", s.driftScale);

        m_shader->setUniform("uRippleStrength", s.rippleStrength);
        m_shader->setUniform("uRippleSpeed", s.rippleSpeed);
        m_shader->setUniform("uRippleScale", s.rippleScale);

        m_shader->setUniform("uChromaticStrength", s.chromaticStrength);

        m_shader->setUniform("uCausticStrength", s.causticStrength);
        m_shader->setUniform("uCausticScale", s.causticScale);
        m_shader->setUniform("uCausticSpeed", s.causticSpeed);

        m_shader->setUniform("uWaterTint", s.waterTint);
        m_shader->setUniform("uTintStrength", s.tintStrength);

        m_shader->setUniform("uDepthFadeStart", s.depthFadeStart);
        m_shader->setUniform("uDepthFadeEnd", s.depthFadeEnd);
        m_shader->setUniform("uSurfaceFoamBand", s.surfaceFoamBand);
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
        // Matches the attribute layout in fullscreen.vert:
        //   location 0 = aPos  (vec2)
        //   location 1 = aUV   (vec2)
        static const float kVerts[] = {
            // aPos      aUV
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

        // aPos
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            4 * sizeof(float), reinterpret_cast<void*>(0));
        // aUV
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

        glBindVertexArray(0);
    }

    // -------------------------------------------------------------------------
    std::shared_ptr<Shader>      m_shader;
    std::shared_ptr<Texture>     m_noiseTexture;
    std::unique_ptr<Framebuffer> m_scratch;

    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;

    WaterDistortionSettings m_settings;
    bool m_enabled = true;
};