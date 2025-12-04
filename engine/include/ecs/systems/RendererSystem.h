/* Start Header *****************************************************************/
/*!
\file   RendererSystem.h
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\brief
Declares the RendererSystem which manages 2D rendering in the ECS framework.

Responsibilities:
- Initialize and manage rendering resources (shaders, camera, render graph)
- Process ECS entities with renderable components
- Execute the render graph each frame with multi-pass rendering
- Handle object picking and selection highlighting

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef RENDERERSYSTEM_H
#define RENDERERSYSTEM_H

#include "ecs/ISystem.h"
#include "ecs/World.h"
#include "Color.h"
#include "Math/Vector2D.h"
#include "graphics/shader.hpp"
#include "graphics/renderer.hpp"
#include "graphics/debugDraw2D.hpp"
#include "graphics/Camera.h"
#include "graphics/RenderGraph.hpp"
#include "graphics/graphicsConfig.hpp"
#include "graphics/PixelBufferObject.hpp"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <functional>

namespace ECS {
    /**
     * @brief System for managing 2D rendering
     * Executes in Render phase with executionOrder=0
     */
    class RendererSystem : public ISystem {
    public:
        RendererSystem() = default;
        ~RendererSystem() override = default;

        // ====================================================================
        // Public Interface
        // ====================================================================

        // ISystem interface
        void OnCreate(World& world) override;
        void OnUpdate(World& world, float deltaTime) override;
        void OnDestroy(World& world) override;
        
        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Render; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }

        // Legacy compatibility (will be removed)
        void Initialize(World& world) { OnCreate(world); }
        void Update(World& world, float dt) { OnUpdate(world, dt); }

        /*!
        \brief Get the number of batch flushes this frame (for profiling).
        \return Flush count, or -1 if renderer not initialized.
        */
        int GetFlushCount() const {
            return m_renderer ? m_renderer->flushCountThisFrame : -1;
        }

        // Compatibility accessors for editor integration
        float GetCameraOrthoSize() const { return m_cameraOrthoSize; }
        RenderGraph* GetRenderGraph() { return m_renderGraph.get(); }
        uint32_t GetSelectedEntityID() const { return m_selectedEntityID; }

        // Static accessor for global access
        static RendererSystem* GetInstance() { return s_instance; }

        /**
         * @brief Set the camera to use for rendering
         * @param camera Pointer to camera (can be editor or game camera)
         * @note Pass nullptr to use game camera from ECS
         */
        void SetCamera(Engine::Camera* camera) { m_activeCamera = camera; }

        /**
         * @brief Get the currently active camera
         * @return Pointer to active camera (may be nullptr)
         */
        Engine::Camera* GetCamera() { return m_activeCamera; }

        // Allow external systems (editor panels) to set the currently selected entity
        void SetSelectedEntityID(uint32_t id) { m_selectedEntityID = id; }

        // Rebind the renderer to a new world
        void BindWorld(World& world);
        
        // Force the renderer to always use scene camera (for game window)
        void SetForceSceneCamera(bool force) { m_forceSceneCamera = force; }

        void SetUILayer(uint16_t layerId) { m_uiLayerId = layerId; }

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
        bool m_forceSceneCamera = false;                            ///< Force use of scene camera (for game window)
        int m_activeCameraIndex = 0;                                ///< Active ECS camera (future use)
        glm::mat4x4 m_projection = glm::identity<glm::mat4x4>();    ///< Projection matrix
        uint16_t m_uiLayerId = 0xFFFF;  // Default invalid value

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
        Engine::Camera* m_activeCamera = nullptr;               ///< Active camera (editor or game)


        // ====================================================================
        // Member Variables - Shaders
        // ====================================================================

        std::unique_ptr<Shader> m_shader;          ///< Main batched geometry shader
        std::unique_ptr<Shader> m_textShader;      ///< SDF text rendering shader
        std::unique_ptr<Shader> m_sdfCircleShader; ///< SDF circle rendering shader
        std::unique_ptr<Shader> m_blitShader;

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

        // Static instance pointer
        static RendererSystem* s_instance;

        // Drag-to-move state
        bool m_isDragging = false;
        glm::vec2 m_dragStartMouseWorld = {0, 0};
        glm::vec3 m_dragStartEntityPos = { 0, 0, 0};
        uint32_t m_lastSelectedEntityID = Entity::NPOS32;
        bool m_wasMouseDownLastFrame = false;


        // ====================================================================
        // Member Variables - UI Scaling
        // ====================================================================

        /*!
        \brief Reference resolution for UI design (1920x1080).
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

        // ====================================================================
        // Drag state for editor entity manipulation
        // ==================================================================== 
        Quaternion m_dragStartEntityRot;
        Vector3D m_dragStartEntityScale;
    };

} // namespace ECS

#endif // RENDERER2D_H
