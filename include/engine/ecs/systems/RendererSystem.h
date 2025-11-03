/* Start Header *****************************************************************/
/*!
\file    RendererSystem.h
\authors Muhammad Nur Fadzly Bin Zulkifli (75%), Choi Meng Yew (25%)
\par     muhammadnurfadzly.b@digipen.edu, choi.m@digipen.edu
\date    20th October 2025
\brief
High-level rendering system for the ECS. Manages shaders, camera, and
orchestrates the render graph for multi-pass rendering.

Responsibilities:
- Initialize and manage rendering resources (shaders, camera, render graph)
- Process ECS entities with renderable components
- Execute the render graph each frame
- Provide temporary accessors for stress testing

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef RENDERER2D_H
#define RENDERER2D_H

// ============================================================================
// Third-Party Includes
// ============================================================================
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/matrix_transform.hpp>

// ============================================================================
// Engine Includes
// ============================================================================
#include "Color.h"
#include "ecs/World.h"
#include "Math/Vector2D.h"

// ============================================================================
// Graphics Includes
// ============================================================================
#include "graphics/shader.hpp"
#include "graphics/renderer.hpp"
#include "graphics/debugDraw2D.hpp"
#include "graphics/EditorCamera.hpp"
#include "graphics/RenderGraph.hpp"
#include "graphics/graphicsConfig.hpp"
#include "graphics/PixelBufferObject.hpp"

namespace ECS {

    // ========================================================================
    // RendererSystem
    // ========================================================================
    /*!
    \class RendererSystem
    \brief Manages all 2D rendering through a modern render graph architecture.

    This system owns the render graph, which in turn owns all framebuffers.
    It processes ECS entities with renderable components and executes multi-pass
    rendering each frame.
    */
    class RendererSystem {
    public:
        // ====================================================================
        // Public Interface
        // ====================================================================

        /*!
        \brief Initialize the rendering system with all required resources.
        \param world The ECS world to render.
        */
        void Initialize(World& world);

        /*!
        \brief Update and render all entities in the world.
        \param world The ECS world to render.
        \param dt Delta time (currently unused).
        */
        void Update(World& world, float dt);

        /*!
        \brief Get the number of batch flushes this frame (for profiling).
        \return Flush count, or -1 if renderer not initialized.
        */
        int GetFlushCount() const {
            return m_renderer ? m_renderer->flushCountThisFrame : -1;
        }

        // ====================================================================
        // Temporary Accessors (For Stress Testing - Remove Later)
        // ====================================================================
        /*! \brief Direct access to low-level renderer for bypass testing. */
        Renderer* GetRenderer() { return m_renderer.get(); }

        /*! \brief Direct access to main shader for bypass testing. */
        Shader* GetShader() { return m_shader.get(); }

        /*! \brief Direct access to text shader for bypass testing. */
        Shader* GetTextShader() { return m_textShader.get(); }

        /*! \brief Get the current projection matrix. */
        const glm::mat4& GetProjection() const { return m_projection; }

    private:
        // ====================================================================
        // Conversion Helpers
        // ====================================================================

        /*! \brief Convert engine Vector2D to GLM vec2. */
        glm::vec2 ToGlm(const Vector2D& v) {
            return glm::vec2{ v.X, v.Y };
        }

        /*!
        \brief Convert engine Color (0-255) to GLM vec4 (0.0-1.0).
        \param c The Color to convert.
        \return Normalized glm::vec4 suitable for shaders.
        \note GLSL expects floats in [0.0-1.0]. Forgetting normalization
              will cause washed-out or grayscale rendering.
        */
        glm::vec4 ToGlm(const Color& c) {
            return glm::vec4{ c.R, c.G, c.B, c.A };
        }

        // ====================================================================
        // Member Variables - State
        // ====================================================================

        bool m_initialized = false;                                 ///< Has Initialize() been called?
        bool m_useEditorCamera = true;                              ///< Use editor vs ECS cameras
        int m_activeCameraIndex = 0;                                ///< Active ECS camera (future use)
        glm::mat4x4 m_projection = glm::identity<glm::mat4x4>();    ///< Projection matrix

        /*!
        \brief Cached current camera orthographic size (world units).
        Updated every frame by RendererSystem::Update().
        */
        float m_cameraOrthoSize = 1000.0f;

        /*!
        \brief Reference ortho size considered "default zoom" for bloom scaling.
        */
        static constexpr float kReferenceOrthoSize = 1080.0f;

        /*!
        \brief Desired bloom spread (in world-space units) at reference zoom.
        E.g., 48 corresponds roughly to 48 pixels at 1080p.
        */
        static constexpr float kDesiredBloomWorldSpread = 40.0f;


        // ====================================================================
        // Member Variables - Core Systems
        // ====================================================================

        std::unique_ptr<Renderer> m_renderer;                   ///< Low-level batch renderer
        std::unique_ptr<RenderGraph> m_renderGraph;             ///< Render graph (owns framebuffers)
        std::unique_ptr<Engine::EditorCamera> m_editorCamera;   ///< Editor camera

        // ====================================================================
        // Member Variables - Shaders
        // ====================================================================

        std::unique_ptr<Shader> m_shader;          ///< Main batched geometry shader
        std::unique_ptr<Shader> m_textShader;      ///< SDF text rendering shader
        std::unique_ptr<Shader> m_sdfCircleShader; ///< SDF circle rendering shader

        // Post-process shaders
        std::unique_ptr<Shader> m_bloomBlurShader;      ///< Bloom blur pass
        std::unique_ptr<Shader> m_bloomExtractShader;   ///< Bloom extraction pass
        std::unique_ptr<Shader> m_bloomCombineShader;   ///< Bloom composite pass

        // ====================================================================
        // Member Variables - Object Picking
        // ====================================================================
        Framebuffer m_pickingFBO;
        PixelBufferObject m_pbos[2];
        int m_currentPBO = 0;
        uint32_t m_selectedEntityID = 0;  // Currently selected entity

        // ====================================================================
        // Member Variables - UI Scaling
        // ====================================================================

        /*!
        \brief Reference resolution for UI design (1920�1080).
        All UI elements are designed at this resolution and scaled proportionally.
        */
        static constexpr float kReferenceWidth = 1920.0f;
        static constexpr float kReferenceHeight = 1080.0f;

        // ====================================================================
        // Helper Methods - Text Positioning
        // ====================================================================

        /*!
        \brief Calculate screen position for anchored UI text.
        \param transform Entity's transform (offset from anchor point).
        \param anchor Anchor point (TopLeft, Center, etc.).
        \param screenWidth Current window width in pixels.
        \param screenHeight Current window height in pixels.
        \param scaleFactor UI scale factor for proportional positioning.
        \return Absolute screen position in pixels.
        */
        glm::vec2 CalculateAnchoredPosition(
            const Components::LocalTransform& transform,
            Components::TextAnchor anchor,
            float screenWidth,
            float screenHeight,
            float scaleFactor) const;
    };

} // namespace ECS

#endif // RENDERER2D_H