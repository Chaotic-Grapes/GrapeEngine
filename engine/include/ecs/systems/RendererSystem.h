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
- Provide GPU-based picking framebuffer for external queries

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef RENDERERSYSTEM_H
#define RENDERERSYSTEM_H

#include "Export.h"
#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"
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
#include "graphics/LightManager.hpp"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <functional>
#include <optional>
#include <unordered_map>
#include <queue>

#include "graphics/TileMapRenderer.hpp"

class TileMap;
class Tileset;
class TileMapRenderer;

namespace ECS {
    /**
     * @brief System for managing 2D rendering
     * Executes in Render phase with executionOrder=0
     */
    class GRAPEENGINE_API RendererSystem : public ISystem {
    public:
        RendererSystem() = default;
        ~RendererSystem() override = default;

        // ====================================================================
        // Public Interface
        // ====================================================================

        // ISystem interface
        void OnCreate(World& world) override;
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;
        
        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Render; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }

        /**
         * @brief Get the number of batch flushes this frame (for profiling).
         * @return Flush count, or -1 if renderer not initialized.
        */
        int GetFlushCount() const {
            return m_renderer ? m_renderer->flushCountThisFrame : -1;
        }

        // Compatibility accessors for editor integration
        float GetCameraOrthoSize() const { return m_cameraOrthoSize; }
        RenderGraph* GetRenderGraph() { return m_renderGraph.get(); }

        // Static accessor for global access
        static RendererSystem* GetInstance();


        /**
         * @brief Set the camera to use for rendering
         * @param camera Pointer to camera for rendering (editor, game, or any other)
         */
        void SetCamera(Engine::Camera* camera) { m_activeCamera = camera; }

        /**
         * @brief Get the currently active external camera
         * @return Pointer to external camera (may be nullptr)
         */
        Engine::Camera* GetCamera() { return m_activeCamera; }

        // Rebind the renderer to a new world
        void BindWorld(World& world);

        // -----------------------
        // Async Picking API
        // -----------------------
        /**
         * @brief Request an async GPU pick at the given screen position (absolute coordinates).
         * @returns Request id which can be polled via TryGetPickResult()
         * @note The result will become available one frame later when the renderer's PBO readback completes.
         */
        uint32_t RequestPick(float screenX, float screenY, const glm::vec2& viewportPos, const glm::vec2& viewportSize);

        /**
         * @brief Try to retrieve a completed pick result for a previously requested pick.
         * @returns true and sets outEntityId when ready; otherwise returns false.
         */
        bool TryGetPickResult(uint32_t requestId, uint32_t& outEntityId);

        /**
         * @brief Get the picking framebuffer for external picking queries
         * @return Pointer to picking FBO (read-only access)
         */
        const Framebuffer* GetPickingFBO() const { return &m_pickingFBO; }

        /**
         * @brief Get the renderer for external rendering (editor overlays)
         * @return Pointer to renderer
         */
        Renderer* GetRenderer() { return m_renderer.get(); }

        /**
         * @brief Get the main shader for external rendering (editor overlays)
         * @return Pointer to batch shader
         */
        Shader* GetShader() { return m_shader.get(); }

        // ====================================================================
        // Wireframe/Debug Rendering APIs
        // ====================================================================
        /**
         * @brief Submit a wireframe quad outline
         * @param min Bottom-left corner in world space
         * @param max Top-right corner in world space
         * @param color RGBA color for the wireframe
         * @param thickness Line thickness in world units
         */
        void SubmitWireframeQuad(const glm::vec2& min, const glm::vec2& max, 
                                 const glm::vec4& color, float thickness);

        /**
         * @brief Submit a wireframe circle outline
         * @param center Center position in world space
         * @param radius Circle radius in world units
         * @param color RGBA color for the wireframe
         * @param thickness Line thickness in world units
         */
        void SubmitWireframeCircle(const glm::vec2& center, float radius,
                                   const glm::vec4& color, float thickness);

        /**
         * @brief Submit a wireframe polygon outline
         * @param vertices Array of vertex positions (world space)
         * @param vertexCount Number of vertices
         * @param color RGBA color for the wireframe
         * @param thickness Line thickness in world units
         * @param closed Whether to connect last vertex to first
         */
        void SubmitWireframePolygon(const glm::vec2* vertices, size_t vertexCount,
                                    const glm::vec4& color, float thickness, bool closed = true);

        /**
         * @brief Submit a wireframe line segment
         * @param p1 Start position in world space
         * @param p2 End position in world space
         * @param color RGBA color for the line
         * @param thickness Line thickness in world units
         */
        void SubmitWireframeLine(const glm::vec2& p1, const glm::vec2& p2,
                                 const glm::vec4& color, float thickness);

        /**
         * @brief Submit a wireframe mesh (collection of connected vertices)
         * @param vertices Array of vertex positions (world space)
         * @param vertexCount Number of vertices
         * @param indices Index buffer (optional, if null assumes sequential)
         * @param indexCount Number of indices
         * @param color RGBA color for the wireframe
         * @param thickness Line thickness in world units
         */
        void SubmitWireframeMesh(const glm::vec2* vertices, size_t vertexCount,
                                 const uint32_t* indices, size_t indexCount,
                                 const glm::vec4& color, float thickness);

        /**
         * @brief Submit collider debug visualizations for an entity
         * @param world ECS world containing the entity
         * @param entityID ID of entity with colliders to render
         * @param color RGBA color for collider wireframes
         * @param thickness Line thickness in world units
         */
        void SubmitColliderDebugDraw(ECS::World& world, uint32_t entityID,
                                     const glm::vec4& color);

        // ====================================================================
        // GUI Rendering APIs
        // ====================================================================
        /**
         * @brief Submit a GUI panel/quad to be rendered
         * @param position Top-left corner in screen space
         * @param size Width and height of the panel
         * @param color RGBA color for the panel
         * @param cornerRadius Corner radius for rounded rectangles (0 = sharp corners)
         */
        void SubmitGUIPanel(const Vector2D& position, const Vector2D& size,
                           const Color& color, float cornerRadius = 0.0f);

        /**
         * @brief Submit a GUI text to be rendered with SDF
         * @param fontPath Path to font file
         * @param text Text string to render
         * @param position Top-left corner in screen space
         * @param color Text color
         * @param fontSize Font size in pixels
         * @param shadowColor Optional shadow color (alpha=0 for no shadow)
         * @param shadowOffset Shadow offset in pixels
         */
        void SubmitGUIText(const std::string& fontPath, const std::string& text,
                          const Vector2D& position, const Color& color,
                          float fontSize, const Color& shadowColor = Color(0U, 0U, 0U, 0U),
                          const Vector2D& shadowOffset = Vector2D(0, 0));

        /**
         * @brief Submit a GUI slider to be rendered
         * @param position Top-left corner in screen space
         * @param size Width and height of the slider
         * @param value Current value (0.0-1.0)
         * @param backgroundColor Track background color
         * @param handleColor Handle color
         * @param borderColor Border color
         * @param borderRadius Corner radius for handles
         */
        void SubmitGUISlider(const Vector2D& position, const Vector2D& size,
                            float value, const Color& backgroundColor,
                            const Color& handleColor, const Color& borderColor = Color(200U, 200U, 200U, 255U),
                            float borderRadius = 4.0f);

        /**
         * @brief Submit a GUI checkbox to be rendered
         * @param position Top-left corner in screen space
         * @param size Size of the checkbox box
         * @param checked Whether checkbox is checked
         * @param boxColor Checkbox box color
         * @param checkColor Checkmark color
         * @param borderColor Border color
         */
        void SubmitGUICheckbox(const Vector2D& position, const Vector2D& size,
                              bool checked, const Color& boxColor,
                              const Color& checkColor, const Color& borderColor = Color(200U, 200U, 200U, 255U));

        /**
         * @brief Submit a GUI line (separator) to be rendered
         * @param startPos Start position in screen space
         * @param endPos End position in screen space
         * @param color Line color
         * @param thickness Line thickness in pixels
         */
        void SubmitGUILine(const Vector2D& startPos, const Vector2D& endPos,
                          const Color& color, float thickness = 1.0f);

        // Call from editor when a tilemap should be rendered
        void SetDebugTileMap(const TileMap& map, const Tileset& tileset);
        void ClearDebugTileMap();

    private:
        // ====================================================================
        // Conversion Helpers
        // ====================================================================

        /**
         * @brief Convert engine Vector2D to GLM vec2. 
         * @return glm::vec2 representation of the Vector2D.
         */
        glm::vec2 ToGlm(const Vector2D& v) {
            return glm::vec2{ v.X, v.Y };
        }

        /**
         * @brief Convert engine Color (0-255) to GLM vec4 (0.0-1.0).
         * @param c The Color to convert.
         * @return Normalized glm::vec4 suitable for shaders.
         * @note GLSL expects floats in [0.0-1.0]. Forgetting normalization
              will cause washed-out or grayscale rendering.
         */
        glm::vec4 ToGlm(const Color& c) {
            return glm::vec4{ c.R, c.G, c.B, c.A };
        }

        // ====================================================================
        // Member Variables - State
        // ====================================================================

        bool m_initialized = false;                                 ///< Has Initialize() been called?
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
        Engine::Camera* m_activeCamera = nullptr;               ///< Active camera (editor or game)

        std::optional<std::reference_wrapper<const TileMap>> m_debugTileMap;
        std::optional<std::reference_wrapper<const Tileset>> m_debugTileset;

        TileMapRenderer m_tileMapRenderer; // value member, no pointer

        // ====================================================================
        // Member Variables - Wireframe Submissions
        // ====================================================================
        // Wireframe submissions are queued and rendered in the scene pass
        struct WireframeSubmission {
            enum class Type { Quad, Circle, Polygon, Line, Mesh };
            Type type;
            std::vector<glm::vec2> vertices;
            std::vector<uint32_t> indices;
            glm::vec2 center; // for circles
            float radius;     // for circles
            glm::vec4 color;
            float thickness;
            bool closed; // for polygons
        };
        std::vector<WireframeSubmission> m_wireframeQueue;

        // ====================================================================
        // Member Variables - GUI Submissions
        // ====================================================================
        // GUI submissions are queued and rendered in screen-space during GUI pass
        struct GUISubmission {
            enum class Type { Panel, Text, Slider, Checkbox, Line };
            Type type;
            Vector2D position;
            Vector2D size;
            Vector2D startPos; // for lines
            Vector2D endPos; // for lines
            Color color;
            Color secondaryColor; // for sliders (handle color), checkboxes (check color)
            Color borderColor; // for sliders, checkboxes
            float cornerRadius; // for panels, sliders
            float value; // for sliders (0.0-1.0)
            float thickness; // for lines
            bool checked; // for checkboxes
            std::string fontPath; // for text
            std::string text; // for text
            float fontSize; // for text
            Color shadowColor; // for text
            Vector2D shadowOffset; // for text
        };
        std::vector<GUISubmission> m_guiSubmissionQueue;

        Graphics::LightManager m_lightManager;

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

        // Async pick request state
        struct PendingPickRequest {
            uint32_t RequestId = 0;
            float ScreenX = 0.0f;
            float ScreenY = 0.0f;
            glm::vec2 ViewportPos{0.0f, 0.0f};
            glm::vec2 ViewportSize{0.0f, 0.0f};
        };

        std::queue<PendingPickRequest> m_pendingPickRequests; // queue of requests to process
        std::optional<PendingPickRequest> m_currentPickRequest; // currently processing request
        std::unordered_map<uint32_t, uint32_t> m_completedPickResults; // requestId -> picked entity id
        uint32_t m_nextPickRequestId = 1;
        struct InFlightPick {
            uint32_t RequestId = 0;
            int PBOIndex = -1;
        };

        std::optional<InFlightPick> m_inFlightPick;


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
        // Helper Methods - Camera
        // ====================================================================

        /**
         * @brief Extract camera matrices from active source
         * @param world The ECS world (used if m_activeCamera is null)
         * @param[out] outView View matrix
         * @param[out] outProjection Projection matrix
         * @param[out] outOrthoSize Orthographic size (for bloom scaling)
         * @return true if a valid camera was found, false otherwise
         */
        bool GetCameraMatrices(World& world, glm::mat4& outView, glm::mat4& outProjection, float& outOrthoSize);

        // ====================================================================
        // GUI Rendering - Internal Processing
        // ====================================================================

        /**
         * @brief Process all queued GUI submissions
         * Called during render pass to render GUI elements
         */
        void ProcessGUISubmissions();

        /**
         * @brief Process a GUI panel submission
         */
        void ProcessGUIPanel(const GUISubmission& submission);

        /**
         * @brief Process a GUI text submission
         */
        void ProcessGUIText(const GUISubmission& submission);

        /**
         * @brief Process a GUI slider submission
         */
        void ProcessGUISlider(const GUISubmission& submission);

        /**
         * @brief Process a GUI checkbox submission
         */
        void ProcessGUICheckbox(const GUISubmission& submission);

        /**
         * @brief Process a GUI line submission
         */
        void ProcessGUILine(const GUISubmission& submission);

    };

    // Explicit out-of-class static declaration to handle DLL linkage
    extern RendererSystem* g_rendererSystemInstance;

} // namespace ECS

#endif // RENDERER2D_H
