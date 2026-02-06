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
#include <string>

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

        struct GUIViewport {
            Vector2D Origin{ 0.0f, 0.0f };
            Vector2D Size{ 0.0f, 0.0f };
            Vector2D DisplayScale{ 1.0f, 1.0f };
            bool Active = false;
        };

        // Static accessor for global access
        static RendererSystem* GetInstance();

        Vector2D GetRenderTargetSize() const { return m_renderTargetSize; }
        GUIViewport GetGUIViewport() const { return m_guiViewport; }
        void SetGUIViewport(const Vector2D& origin, const Vector2D& size, const Vector2D& displayScale);
        void ResetGUIViewport();

        // ====================================================================
        // Viewport Management
        // ====================================================================

        struct Viewport {
            std::string Name;
            Engine::Camera* Camera = nullptr;
            glm::ivec2 Size{ 1, 1 };
            bool Active = true;

            // Per-viewport render targets
            std::unique_ptr<Framebuffer> HDR;
            std::unique_ptr<Framebuffer> LDR;
            std::unique_ptr<Framebuffer> BloomExtract;
            std::unique_ptr<Framebuffer> BloomBlur;
            std::unique_ptr<Framebuffer> PickingFBO;
        };

        void AddViewport(const std::string& name, Engine::Camera* camera, int w, int h);
        void RemoveViewport(const std::string& name);
        void ResizeViewport(const std::string& name, int w, int h);
        void SetViewportCamera(const std::string& name, Engine::Camera* camera);
        Viewport* GetViewport(const std::string& name);
        GLuint GetViewportTexture(const std::string& name) const;

        // ====================================================================

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
        // Submit a filled quad (editor overlays like tile previews).
        void SubmitFilledQuad(const glm::vec2& min, const glm::vec2& max,
                              const glm::vec4& color);

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

        // Submit a textured overlay quad in world space (used by editor previews).
        void SubmitOverlayQuad(const glm::vec2& center,
                               const glm::vec2& size,
                               GLuint textureId,
                               const glm::vec4& uvRect,
                               const glm::vec4& color,
                               float rotation);

        /**
         * @brief Submit collider debug visualizations for an entity
         * @param world ECS world containing the entity
         * @param entityID ID of entity with colliders to render
         * @param color RGBA color for collider wireframes
         * @param thickness Line thickness in world units
         */
        void SubmitColliderDebugDraw(ECS::World& world, uint32_t entityID,
                                     const glm::vec4& color);

        void SubmitGUIPanel(const Vector2D& position, const Vector2D& size,
                            const Color& color, float cornerRadius = 0.0f);
        void SubmitGUIImage(const Vector2D& position, const Vector2D& size,
                            uint32_t textureId, const Vector4D& uvRect, const Color& color);
        void SubmitGUIText(const Vector2D& position, const std::string& text,
                           const std::string& fontPath, float pixelSize, const Color& color);

        // Call from editor when a tilemap should be rendered
        struct DebugTileMapEntry {
            std::reference_wrapper<const TileMap> Map;
            std::vector<const Tileset*> Tilesets;
            glm::vec2 Offset;
        };

        void SetDebugTileMap(const TileMap& map, const Tileset& tileset, const glm::vec2& worldOffset);
        void SetDebugTileMaps(const std::vector<DebugTileMapEntry>& maps);
        void ClearDebugTileMap();
        void ClearDebugTileMaps();

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
        glm::vec2 m_debugTileMapOffset = glm::vec2(0.0f, 0.0f); // World-space offset for debug tilemap rendering.
        std::vector<DebugTileMapEntry> m_debugTileMaps; // Multiple tilemaps to render in the editor.

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
            bool filled = false;
        };
        std::vector<WireframeSubmission> m_wireframeQueue;

        struct OverlayQuadSubmission {
            glm::vec2 center;
            glm::vec2 size;
            GLuint textureId = 0;
            glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
            glm::vec4 color{1.0f};
            float rotation = 0.0f;
        };
        std::vector<OverlayQuadSubmission> m_overlayQuadQueue;

        struct GUIPanelSubmission {
            Vector2D position;
            Vector2D size;
            Color color;
            float cornerRadius = 0.0f;
        };
        std::vector<GUIPanelSubmission> m_guiPanelQueue;

        struct GUITextSubmission {
            Vector2D position;
            std::string text;
            std::string fontPath;
            float pixelSize = 24.0f;
            Color color;
        };
        std::vector<GUITextSubmission> m_guiTextQueue;

        struct GUIImageSubmission {
            Vector2D position;
            Vector2D size;
            uint32_t textureId = 0;
            Vector4D uvRect{ 0.0f, 0.0f, 1.0f, 1.0f };
            Color color;
        };
        std::vector<GUIImageSubmission> m_guiImageQueue;

        Graphics::LightManager m_lightManager;

        // ====================================================================
        // Member Variables - Shaders
        // ====================================================================

        // MAKE IT SHARED SO THAT RM OWNS THE LIBRARY COPY AND THE SAME 
        // TEXTURE/SHADER/FONT CAN BE REUSED WITHOUT DUPLICATING MEMORY

        std::shared_ptr<Shader> m_shader;          ///< Main batched geometry shader
        std::shared_ptr<Shader> m_textShader;      ///< SDF text rendering shader
        std::shared_ptr<Shader> m_sdfCircleShader; ///< SDF circle rendering shader
        std::shared_ptr<Shader> m_blitShader;

        // Post-process shaders
        std::shared_ptr<Shader> m_bloomBlurShader;      ///< Bloom blur pass
        std::shared_ptr<Shader> m_bloomExtractShader;   ///< Bloom extraction pass
        std::shared_ptr<Shader> m_bloomCombineShader;   ///< Bloom composite pass

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

        Vector2D m_renderTargetSize{ 0.0f, 0.0f };
        GUIViewport m_guiViewport{};

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
        // Viewport State
        // ====================================================================
        std::vector<Viewport> m_viewports;

        // ====================================================================
        // Extracted Render Helpers (for multi-viewport)
        // ====================================================================
        void CollectLights(World& world);
        void BucketEntities(World& world, std::vector<std::vector<Entity>>& buckets, int& maxLayerId);
        void RenderSceneToHDR(World& world, Viewport& vp, const glm::mat4& viewProj,
            const std::vector<std::vector<Entity>>& buckets, int maxLayerId);
        void RenderBloom(Viewport& vp, float bloomRadius);
        void ToneMap(Viewport& vp);
        void RenderOverlayQuads(Viewport& vp, const glm::mat4& viewProj);
        void RenderWireframes(Viewport& vp, const glm::mat4& viewProj);
        void RenderGUI(Viewport& vp);
        void RenderPicking(World& world, Viewport& vp, const glm::mat4& viewProj,
            const std::vector<std::vector<Entity>>& buckets);

    };

    // Explicit out-of-class static declaration to handle DLL linkage
    extern RendererSystem* g_rendererSystemInstance;

} // namespace ECS

#endif // RENDERER2D_H
