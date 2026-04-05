/* Start Header *****************************************************************/
/*!
\file   RendererSystem.h
\author Choi Meng Yew (95%)
        Foo Rui Qin (5%)
\date   12th March 2026
\par    choi.m@digipen.edu
        ruiqin.foo@digipen.edu
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
#include "ecs/systems/ParticleSystem.h"
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
#include "graphics/WaterDistortionPass.hpp"
#include "graphics/LightManager.hpp"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <string>

#include "graphics/TileMapRenderer.hpp"

// Forward declarations
class TileMap;
class Tileset;
class TileMapRenderer;

namespace ECS {
    class BoidSystem;
}

namespace ECS {
    /**
     * @brief System for managing 2D rendering
     * Executes in Render phase with executionOrder=0
     */
    class GRAPEENGINE_API RendererSystem : public ISystem {
    public:
        // Build an uninitialized renderer system with default state
        RendererSystem() = default;

        // Clean up renderer-owned resources
        ~RendererSystem() override = default;

        // ====================================================================
        // Public Interface
        // ====================================================================

        // ISystem interface

        // Create renderer resources and bind to the active world
        void OnCreate(World& world) override;

        // Render one frame for the current world state
        void OnUpdate(World& world) override;

        // Release renderer resources before system shutdown
        void OnDestroy(World& world) override;
        
        // Describe scheduling metadata used by ECS system orchestration
        SystemMetadata GetMetadata() const override;

        // Report this system's execution group
        SystemGroup GetSystemGroup() const override { return SystemGroup::Render; }

        // Keep renderer updates enabled in both edit and play contexts
        SystemRunMode GetRunMode() const override { return SystemRunMode::Always; }

        /**
         * @brief Get the number of batch flushes this frame (for profiling).
         * @return Flush count, or -1 if renderer not initialized.
        */
        int GetFlushCount() const {
            return m_renderer ? m_renderer->flushCountThisFrame : -1;
        }

        // Compatibility accessors for editor integration

        // Expose the active orthographic size for editor tools
        float GetCameraOrthoSize() const { return m_cameraOrthoSize; }

        // Expose the render graph so editor systems can inspect pass outputs
        RenderGraph* GetRenderGraph() { return m_renderGraph.get(); }

        struct GUIViewport {
            Vector2D Origin{ 0.0f, 0.0f };
            Vector2D Size{ 0.0f, 0.0f };
            Vector2D DisplayScale{ 1.0f, 1.0f };
            bool Active = false;
        };

        // Static accessor for global access
        static RendererSystem* GetInstance();

        // Return the current render target size in pixels
        Vector2D GetRenderTargetSize() const { return m_renderTargetSize; }

        // Return the active GUI viewport definition
        GUIViewport GetGUIViewport() const { return m_guiViewport; }

        // Configure GUI viewport origin, size, and DPI scale for UI rendering
        void SetGUIViewport(const Vector2D& origin, const Vector2D& size, const Vector2D& displayScale);

        // Clear GUI viewport override and fall back to full render target
        void ResetGUIViewport();

        // Project a world position into screen coordinates for the provided viewport
        bool WorldToScreen(World& world, const Vector3D& worldPos, const Vector2D& viewportOrigin,
            const Vector2D& viewportSize, Vector2D& outScreen);

        // Compute camera basis vectors used by editor gizmo alignment
        bool GetCameraBasis(World& world, glm::vec3& outRight, glm::vec3& outUp);

        // ====================================================================
        // Viewport Management
        // ====================================================================

        struct Viewport {
            std::string Name;
            Engine::Camera* Camera = nullptr;
            glm::ivec2 Size{ 1, 1 };
            bool Active = true;
            bool BloomEnabled = true;

            // Per-viewport render targets
            std::unique_ptr<Framebuffer> HDR;
            std::unique_ptr<Framebuffer> LDR;
            std::unique_ptr<Framebuffer> BloomExtract;
            std::unique_ptr<Framebuffer> BloomBlur;
            std::unique_ptr<Framebuffer> PickingFBO;
        };

        // Register a named viewport and allocate its render targets
        void AddViewport(const std::string& name, Engine::Camera* camera, int w, int h);

        // Remove a named viewport and free its associated resources
        void RemoveViewport(const std::string& name);

        // Resize all render targets for a named viewport
        void ResizeViewport(const std::string& name, int w, int h);

        // Swap the camera source used by a named viewport
        void SetViewportCamera(const std::string& name, Engine::Camera* camera);

        /**
         * @brief Enable or disable bloom passes for a named viewport.
         * @param name Unique viewport name.
         * @param enabled True to run bloom extraction and blur passes.
         * @return void
         */
        void SetViewportBloomEnabled(const std::string& name, bool enabled);

        // Look up a viewport by name for mutation
        Viewport* GetViewport(const std::string& name);

        // Fetch the final LDR texture id for viewport presentation
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

        // Rebuild world-dependent renderer state after world replacement
        void BindWorld(World& world);

        // -----------------------
        // Async Picking API
        // -----------------------
        /**
         * @brief Request an async GPU pick at the given screen position (absolute coordinates).
         * @param screenX Absolute screen X coordinate of the pick position.
         * @param screenY Absolute screen Y coordinate of the pick position.
         * @param viewportPos Top-left corner of the viewport in screen space.
         * @param viewportSize Width and height of the viewport in pixels.
         * @return Request id which can be polled via TryGetPickResult().
         * @note The result will become available one frame later when the renderer's PBO readback completes.
         */
        uint32_t RequestPick(float screenX, float screenY, const glm::vec2& viewportPos, const glm::vec2& viewportSize);

        /**
         * @brief Try to retrieve a completed pick result for a previously requested pick.
         * @param requestId Request id returned by RequestPick().
         * @param outEntityId Set to the picked entity id when the result is ready.
         * @return True and sets outEntityId when ready; otherwise returns false.
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
         * @brief Submit a filled quad (editor overlays like tile previews).
         * @param min Bottom-left corner in world space.
         * @param max Top-right corner in world space.
         * @param color RGBA fill color.
         */
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
         * @brief Submit collider debug visualizations for an entity.
         * @param world ECS world containing the entity.
         * @param entityID ID of entity with colliders to render.
         * @param color RGBA color for collider wireframes.
         */
        void SubmitColliderDebugDraw(ECS::World& world, uint32_t entityID,
                                     const glm::vec4& color);

        // Queue a screen-space rounded panel quad for GUI rendering
        void SubmitGUIPanel(const Vector2D& position, const Vector2D& size,
                            const Color& color, float cornerRadius = 0.0f, float rotationRadians = 0.0f);

        // Queue a screen-space textured GUI image quad
        void SubmitGUIImage(const Vector2D& position, const Vector2D& size,
                            uint32_t textureId, const Vector4D& uvRect, const Color& color,
                            Graphics::TextureFilter textureFilter = Graphics::TextureFilter::Nearest,
                            float rotationRadians = 0.0f);

        // Queue a screen-space text draw request
        void SubmitGUIText(const Vector2D& position, const std::string& text,
                           const std::string& fontPath, float pixelSize, const Color& color);

        // Queue a world-space panel that is rendered with scene camera transforms
        void SubmitWorldGUIPanel(const Vector2D& position, const Vector2D& size,
                                 const Color& color, float cornerRadius = 0.0f, float rotationRadians = 0.0f);

        // Queue a world-space textured GUI image
        void SubmitWorldGUIImage(const Vector2D& position, const Vector2D& size,
                                 uint32_t textureId, const Vector4D& uvRect, const Color& color,
                                 Graphics::TextureFilter textureFilter = Graphics::TextureFilter::Nearest,
                                 float rotationRadians = 0.0f);

        // Queue world-space text to be composited in the scene pass
        void SubmitWorldGUIText(const Vector2D& position, const std::string& text,
                                const std::string& fontPath, float pixelSize, const Color& color);

        // Call from editor when a tilemap should be rendered
        struct DebugTileMapEntry {
            std::reference_wrapper<const TileMap> Map;
            
            // Shared pointer is required here to extend the lifetime of the Tileset
            // In the editor, a user might hot-swap a tileset (e.g. drag-and-drop) which could destroy the old Tileset 
            // while the RendererSystem is still processing the current frame
            // Holding a shared_ptr prevents use-after-free
            std::vector<std::shared_ptr<const Tileset>> Tilesets;
            
            glm::vec2 Offset;
            Entity SourceEntity = NULL_ENTITY; // Editor entity that owns this debug tilemap
        };

        // Install one debug tilemap entry from direct map and tileset references
        void SetDebugTileMap(const TileMap& map, const Tileset& tileset, const glm::vec2& worldOffset);

        // Replace all debug tilemap entries for the next render
        void SetDebugTileMaps(const std::vector<DebugTileMapEntry>& maps);

        // Clear single-debug-tilemap state
        void ClearDebugTileMap();

        // Clear multi-debug-tilemap state
        void ClearDebugTileMaps();

    private:
        // ====================================================================
        // Conversion Helpers
        // ====================================================================

        /**
         * @brief Convert engine Vector2D to GLM vec2.
         * @param v Source Vector2D.
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
        std::unique_ptr<Renderer> m_guiRenderer;                ///< Dedicated GUI batch renderer
        std::unique_ptr<RenderGraph> m_renderGraph;             ///< Render graph (owns framebuffers)
        Engine::Camera* m_activeCamera = nullptr;               ///< Active camera (editor or game)

        std::optional<std::reference_wrapper<const TileMap>> m_debugTileMap;
        std::optional<std::reference_wrapper<const Tileset>> m_debugTileset;
        glm::vec2 m_debugTileMapOffset = glm::vec2(0.0f, 0.0f); // World-space offset for debug tilemap rendering.
        std::vector<DebugTileMapEntry> m_debugTileMaps; // Multiple tilemaps to render in the editor.

        TileMapRenderer m_tileMapRenderer; // value member, no pointer

        // Compute systems
        BoidSystem* m_boidSystem = nullptr;

		// This cache holds runtime tilemap data for entities with TileMapComponent, keyed by EntityId
        struct RuntimeTileMapEntry {
			uint32_t Generation = 0;       // Used to track changes and invalidate cache when tilemap is modified
			std::shared_ptr<TileMap> Map;  // Pointer to tilemap data 
			std::vector<std::shared_ptr<Tileset>> Tilesets;  // Pointers to tilesets used by the tilemap
			std::vector<std::string> TilesetPaths;           // Original tileset paths
			std::string MapPath;                             // Original map path
			float TileWorldSize = 1.0f;    
			uint32_t TilePixelSize = 32;   
			uint32_t DefaultWidth = 64;    
			uint32_t DefaultHeight = 64;  
            uint16_t RenderLayerId = 0;
            bool Visible = true;
            glm::vec2 Origin{ 0.0f, 0.0f };

            ECS::Components::Material2D Material{};
            bool HasMaterial = false;
        };

        std::unordered_map<EntityId, RuntimeTileMapEntry> m_runtimeTileMaps;

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
            float rotation = 0.0f;
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
            Graphics::TextureFilter textureFilter = Graphics::TextureFilter::Nearest;
            float rotation = 0.0f;
        };
        std::vector<GUIImageSubmission> m_guiImageQueue;

        struct WorldGUIPanelSubmission {
            Vector2D position;
            Vector2D size;
            Color color;
            float cornerRadius = 0.0f;
            float rotation = 0.0f;
        };
        std::vector<WorldGUIPanelSubmission> m_worldGuiPanelQueue;

        struct WorldGUITextSubmission {
            Vector2D position;
            std::string text;
            std::string fontPath;
            float pixelSize = 24.0f;
            Color color;
        };
        std::vector<WorldGUITextSubmission> m_worldGuiTextQueue;

        struct WorldGUIImageSubmission {
            Vector2D position;
            Vector2D size;
            uint32_t textureId = 0;
            Vector4D uvRect{ 0.0f, 0.0f, 1.0f, 1.0f };
            Color color;
            Graphics::TextureFilter textureFilter = Graphics::TextureFilter::Nearest;
            float rotation = 0.0f;
        };
        std::vector<WorldGUIImageSubmission> m_worldGuiImageQueue;

        Graphics::LightManager m_lightManager;

        // ====================================================================
        // Member Variables - Shaders
        // ====================================================================

        // MAKE IT SHARED SO THAT RM OWNS THE LIBRARY COPY AND THE SAME 
        // TEXTURE/SHADER/FONT CAN BE REUSED WITHOUT DUPLICATING MEMORY

        std::shared_ptr<Shader> m_shader;          ///< Main batched geometry shader
        std::shared_ptr<Shader> m_guiShader;       ///< GUI-only shader with gamma correction
        std::shared_ptr<Shader> m_textShader;      ///< SDF text rendering shader
        std::shared_ptr<Shader> m_sdfCircleShader; ///< SDF circle rendering shader
        std::shared_ptr<Shader> m_blitShader;

        // Post-process shaders
        std::shared_ptr<Shader> m_bloomBlurShader;      ///< Bloom blur pass
        std::shared_ptr<Shader> m_bloomExtractShader;   ///< Bloom extraction pass
        std::shared_ptr<Shader> m_bloomCombineShader;   ///< Bloom composite pass

        std::unique_ptr<WaterDistortionPass> m_waterPass; ///< Water distortion effect pass

        // Compute shaders
        std::shared_ptr<Shader> m_boidShader;

        // Particle
        std::shared_ptr<Shader> m_particleShader;
        ECS::ParticleSystem* m_particleSystem = nullptr;

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
        float m_windowAspectRatio = 0.0f;
        bool m_windowAspectDirty = false;

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

        // Gather active lights into the renderer light manager
        void CollectLights(World& world);

        // Group renderable entities into layer buckets for stable draw ordering
        void BucketEntities(World& world, std::vector<std::vector<Entity>>& buckets, int& maxLayerId);

        // Render scene content into the viewport HDR target
        void RenderSceneToHDR(World& world, Viewport& vp, const glm::mat4& viewProj,
            const std::vector<std::vector<Entity>>& buckets, int maxLayerId);

        // Apply bloom extraction and blur passes for the viewport
        void RenderBloom(Viewport& vp, float bloomRadius);

        /**
         * @brief Run tone mapping from HDR to final LDR output.
         * @param vp Target viewport to write final LDR output.
         * @param bloomEnabled Controls whether bloom contribution is blended in.
         * @return void
         */
        void ToneMap(Viewport& vp, bool bloomEnabled);

        // Sync runtime tilemap cache with current world state
        // Prepare runtime data
		void RefreshRuntimeTileMaps(World& world); 

        // Submit one runtime tilemap when its owning entity is reached in Z-sorted draw order
        // Draw runtime tilemap per entity
        void SubmitRuntimeTileMapEntity(Entity entity); 

        // Submit one editor debug tilemap when its owning entity is reached in Z-sorted draw order
        // Draw editor/debug tilemap per entity
        void SubmitDebugTileMapEntity(World& world, Entity entity); 

        // Draw queued overlay quads after main scene geometry
        void RenderOverlayQuads(Viewport& vp, const glm::mat4& viewProj);

        // Draw queued wireframe primitives for editor visualization
        void RenderWireframes(Viewport& vp, const glm::mat4& viewProj);

        // Render queued screen-space GUI primitives
        void RenderGUI(Viewport& vp);

        // Render queued world-space GUI primitives
        void RenderWorldGUI(const glm::mat4& viewProj);

        // Render picking ids into the picking target and process async readback
        void RenderPicking(World& world, Viewport& vp, const glm::mat4& viewProj,
            const std::vector<std::vector<Entity>>& buckets);

    };

    // Explicit out-of-class static declaration to handle DLL linkage
    extern RendererSystem* g_rendererSystemInstance;

} // namespace ECS

#endif // RENDERER2D_H
