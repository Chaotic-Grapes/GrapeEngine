/* Start Header *****************************************************************/
/*!
\file   RendererSystem.cpp
\author Choi Meng Yew
\date   31st October 2025
\brief
Implementation of the RendererSystem, the high-level rendering pipeline
for the ECS. Manages shader programs, framebuffers, and the RenderGraph
to orchestrate a multi-pass pipeline including scene rendering, HDR,
bloom extraction, two-pass Gaussian blur, and final tone-mapped composite.

Responsibilities:
- Initialize and manage rendering resources (shaders, render targets, framebuffers)
- Execute a RenderGraph-based pipeline for HDR and post-processing effects
- Render ECS entities by layer with support for SDF primitives, sprites, and text
- Handle object picking and selection highlighting via ID-encoded FBO
- Support external camera injection for flexible rendering contexts

The RendererSystem acts as the bridge between ECS data and GPU rendering,
handling batching, shader bindings, and visual effects in a modular,
pass-based architecture.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

// ============================================================================
// Engine Systems
// ============================================================================
#include "ecs/systems/RendererSystem.h"

// ============================================================================
// Core Engine
// ============================================================================
#include "core/Application.h"
#include "core/messaging/MessageSystem.h"
#include "core/messaging/MessageTypes.h"
#include "services/Input.h"

// ============================================================================
// Graphics
// ============================================================================
#include "graphics/renderer.hpp"
#include "graphics/texture.hpp"
#include "graphics/RenderGraph.hpp"
#include "graphics/PixelBufferObject.hpp"
#include "graphics/font.hpp"
#include "graphics/LightManager.hpp"

// ============================================================================
// ECS Components
// ============================================================================
#include "ecs/Components.h"

// ============================================================================
// Services
// ============================================================================
#include "services/TimeSystem.h"
#include "services/ResourceManager.h"
#include "platform/IPlatformContext.h"

// ============================================================================
// Helpers
// ============================================================================
#include "helpers/TransformUtils.h"

// ============================================================================
// Standard Library
// ============================================================================
#include <algorithm>
#include <iterator>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <cmath>

// ============================================================================
// Third-Party Libraries
// ============================================================================
#include <glm/gtc/matrix_transform.hpp>

namespace ECS {
    static constexpr uint32_t INVALID_ENTITY_ID = Entity::NPOS32;

    // Define the global instance pointer (avoids dllimport issues with static class members)
    RendererSystem* g_rendererSystemInstance = nullptr;

    // Implementation of static GetInstance method
    RendererSystem* RendererSystem::GetInstance() {
        return g_rendererSystemInstance;
    }

    // GUI viewport management
    void RendererSystem::SetGUIViewport(const Vector2D& origin, const Vector2D& size, const Vector2D& displayScale) {
        m_guiViewport.Origin = origin;
        m_guiViewport.Size = size;
        m_guiViewport.DisplayScale = displayScale;
        m_guiViewport.Active = true;
    }

    // Reset GUI viewport to full render target size
    void RendererSystem::ResetGUIViewport() {
        m_guiViewport.Origin = { 0.0f, 0.0f };
        m_guiViewport.Size = m_renderTargetSize;
        m_guiViewport.DisplayScale = { 1.0f, 1.0f };
        m_guiViewport.Active = true;
    }

    // Helper function to get the effective transform for rendering
    // Uses WorldTransform if available, otherwise falls back to LocalTransform
    static void GetRenderTransform(World& world, const Entity entity,
        const Components::LocalTransform& lt,
        Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) {
        if (world.Has<Components::WorldTransform>(entity)) {
            const auto& wt = world.Get<Components::WorldTransform>(entity);
            // Decompose world matrix to get position, rotation, scale
            TransformUtils::DecomposeTRS(wt.Matrix, outPosition, outRotation, outScale);
        }
        else {
            // No WorldTransform, use LocalTransform directly
            outPosition = lt.Position;
            outRotation = lt.Rotation;
            outScale = lt.Scale;
        }
    }

    // Helper function to convert screen coordinates to world coordinates
    static glm::vec2 ScreenToWorld(
        const glm::dvec2& screenPos,
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec2& viewportMin,    // NEW: viewport offset
        const glm::vec2& viewportSize)   // NEW: viewport size
    {
        // Convert to viewport-local coordinates first
        glm::vec2 localPos = glm::vec2(screenPos) - viewportMin;

        // Normalize device coordinates (-1 to 1) using viewport dimensions
        glm::vec4 ndc;
        ndc.x = (2.0f * localPos.x) / viewportSize.x - 1.0f;
        ndc.y = 1.0f - (2.0f * localPos.y) / viewportSize.y;
        ndc.z = 0.0f;
        ndc.w = 1.0f;

        // Inverse projection and view
        glm::mat4 invViewProj = glm::inverse(projection * view);
        glm::vec4 worldPos = invViewProj * ndc;

        return glm::vec2(worldPos.x, worldPos.y);
    }

    SystemMetadata RendererSystem::GetMetadata() const {
        ComponentAccessBuilder builder("Renderer");
        // Note: RendererSystem reads many components (SpriteRenderer2D, WorldTransform, etc.)
        // but uses them through world iteration rather than static declaration.
        // For now, marking as read-only with minimal dependency tracking.
        // Full component access list documented in OnUpdate().
        builder.SetExecutionOrder(0);
        builder.SetGroup(SystemGroup::Render);
        builder.SetRunMode(SystemRunMode::Always);
        return builder.Build();
    }

    void RendererSystem::OnCreate(World& world) {
        if (m_initialized)
            return;

        m_initialized = true;

        // Set global instance pointer
        g_rendererSystemInstance = this;

        auto* context = Engine::CORE->GetPlatformContext();
        auto* mainWindow = context ? context->GetMainWindow() : nullptr;
        if (!mainWindow) {
            LOG_ERROR("RendererSystem::OnCreate: No main window available");
            return;
        }
        const int width = mainWindow->GetWidth();
        const int height = mainWindow->GetHeight();
        m_renderTargetSize = { static_cast<float>(width), static_cast<float>(height) };
        ResetGUIViewport();

        // Use RM instead!
        m_shader = RM.Get<Shader>("assets/shaders/batch");
        m_textShader = RM.Get<Shader>("assets/shaders/sdf_text");
        m_sdfCircleShader = RM.Get<Shader>("assets/shaders/sdf_circle");
        m_bloomExtractShader = RM.Get<Shader>("assets/shaders/bloom_extract");

        m_bloomBlurShader = RM.GetShader(
            "assets/shaders/bloom_extract.vert",
            "assets/shaders/bloom_blur.frag");

        m_bloomCombineShader = RM.GetShader(
            "assets/shaders/bloom_extract.vert",
            "assets/shaders/bloom_combine.frag");

        m_blitShader = RM.Get<Shader>("assets/shaders/blit");

        // Object Picking
        m_pickingFBO.Create(width, height, false, false, 1);
        m_pbos[0].Create(4, GL_STREAM_READ);
        m_pbos[1].Create(4, GL_STREAM_READ);

        // Renderer
        m_renderer = std::make_unique<Renderer>(15000);

        // RenderGraph now owns all framebuffers (no more m_fbos!)
        m_renderGraph = std::make_unique<RenderGraph>();

        m_renderGraph->CreateTexture("HDR",
            { width, height, GL_RGBA16F, false });

        m_renderGraph->CreateTexture("Backbuffer",
            { width, height, GL_RGBA8, true });

        m_renderGraph->CreateTexture("BloomExtract"
            , { width / 2, height / 2, GL_RGBA16F, false });

        m_renderGraph->CreateTexture("BloomBlur"
            , { width / 2, height / 2, GL_RGBA16F, false });

        m_renderGraph->CreateTexture("LDR",
            { width, height, GL_RGBA8, false });

        // Resize HDR when window resizes
        Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
            [this](const Messaging::WindowResized& msg)
            {
                // TODO: Add RenderGraph::ResizeTexture() method to handle this?
                // For now, recreate the graph on resize
                m_renderGraph = std::make_unique<RenderGraph>();

                m_renderGraph->CreateTexture("HDR",          { msg.Width,      msg.Height,      GL_RGBA16F, false });
                m_renderGraph->CreateTexture("Backbuffer",   { msg.Width,      msg.Height,      GL_RGBA8,   true  });
                m_renderGraph->CreateTexture("BloomExtract", { msg.Width / 2,  msg.Height / 2,  GL_RGBA16F, false });
                m_renderGraph->CreateTexture("BloomBlur",    { msg.Width / 2,  msg.Height / 2,  GL_RGBA16F, false });
                m_renderGraph->CreateTexture("LDR",          { msg.Width,      msg.Height,      GL_RGBA8, false });

                // Update fallback projection
                m_projection = glm::ortho(
                    0.f, static_cast<float>(msg.Width),
                    0.f, static_cast<float>(msg.Height),
                    -1.f, 1.f
                );

                // Resize picking FBO
                m_pickingFBO.Resize(msg.Width, msg.Height, false, false);

                m_renderTargetSize = { static_cast<float>(msg.Width), static_cast<float>(msg.Height) };
                ResetGUIViewport();
            });

        // Projection matrix
        m_projection = glm::ortho(
            0.f, static_cast<float>(width),
            0.f, static_cast<float>(height),
            -1.f, 1.f
        );

        // OpenGL state
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Light manager (SSBO creation etc.)
        m_lightManager.Initialize();
    }

    void RendererSystem::BindWorld(World& world) {
        (void)world; // Currently unused
    }

    bool RendererSystem::GetCameraMatrices(World& world, glm::mat4& outView, glm::mat4& outProjection, float& outOrthoSize) {
        // Default fallback
        outView = glm::mat4(1.0f);
        outProjection = glm::mat4(1.0f);
        outOrthoSize = kReferenceOrthoSize;

        // Use external camera if provided
        if (m_activeCamera) {
            outView = m_activeCamera->GetViewMatrix();
            outProjection = m_activeCamera->GetProjectionMatrix();
            outOrthoSize = m_activeCamera->OrthoSize;
            return true;
        }

        // Fall back to ECS camera
        bool foundActive = false;
        world.Each<ECS::Components::LocalTransform, ECS::Components::Camera3D>(
            [&](ECS::Entity /*e*/,
                const ECS::Components::LocalTransform& transform,
                const ECS::Components::Camera3D& camera)
            {
                if (foundActive || !camera.Active) return;

                // --- View: eye looks forward along -Z
                const glm::vec3 eye(
                    transform.Position.X,
                    transform.Position.Y,
                    transform.Position.Z
                );
                const glm::vec3 target = eye + glm::vec3(0.f, 0.f, -1.f);
                outView = glm::lookAt(eye, target, glm::vec3(0.f, 1.f, 0.f));

                // --- Projection
                if (camera.UsePerspective) {
                    // camera.FOV assumed radians
                    outProjection = glm::perspective(
                        camera.FOV,
                        camera.AspectRatio,
                        camera.NearPlane,
                        camera.FarPlane
                    );
                }
                else {
                    const float halfH = camera.OrthoSize * 0.5f;
                    const float halfW = halfH * camera.AspectRatio;
                    outProjection = glm::ortho(
                        -halfW, +halfW,
                        -halfH, +halfH,
                        camera.NearPlane, camera.FarPlane
                    );
                }

                foundActive = true;
                outOrthoSize = camera.OrthoSize;
            }
        );

        if (foundActive) {
            return true;
        }

        // Fallback: screen-aligned ortho
        auto* context = Engine::CORE->GetPlatformContext();
        auto* mainWindow = context ? context->GetMainWindow() : nullptr;
        if (mainWindow) {
            outProjection = glm::ortho(0.f, static_cast<float>(mainWindow->GetWidth()),
                0.f, static_cast<float>(mainWindow->GetHeight()),
                -1.f, 1.f);
            return true;
        }

        return false;
    }

    void RendererSystem::OnUpdate(World& world) {
        if (!m_renderer)
            return;

        auto* context = Engine::CORE->GetPlatformContext();
        auto* win = context ? context->GetMainWindow() : nullptr;
        if (!win) return;

        // ============================================================
        // SHARED WORK (once per frame)
        // ============================================================
        CollectLights(world);

        std::vector<std::vector<Entity>> buckets;
        int maxLayerId = -1;
        BucketEntities(world, buckets, maxLayerId);

        // ============================================================
        // MULTI-VIEWPORT RENDERING (if viewports registered)
        // ============================================================
        if (!m_viewports.empty()) {
            for (auto& vp : m_viewports) {
                if (!vp.Active || !vp.Camera) continue;
                if (vp.Size.x <= 0 || vp.Size.y <= 0) continue;

                glm::mat4 view = vp.Camera->GetViewMatrix();
                glm::mat4 proj = vp.Camera->GetProjectionMatrix();
                glm::mat4 viewProj = proj * view;

                float orthoSize = vp.Camera->OrthoSize;
                float zoomScale = kReferenceOrthoSize / orthoSize;
                float bloomRadius = (kDesiredBloomWorldSpread / kReferenceOrthoSize)
                    * (vp.Size.y / 2.0f) * zoomScale;

                RenderSceneToHDR(world, vp, viewProj, buckets, maxLayerId);
                RenderBloom(vp, bloomRadius);
                ToneMap(vp);
                RenderWireframes(vp, viewProj);
                RenderGUI(vp);
                RenderPicking(world, vp, viewProj, buckets);
            }

            m_wireframeQueue.clear();
            m_guiPanelQueue.clear();
            m_guiTextQueue.clear();

            Framebuffer::Unbind();
            return;  // EXIT HERE - skip RenderGraph path
        }

        // ============================================================
        // FALLBACK: Original RenderGraph path (unchanged for backwards compatibility)
        // ============================================================

        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
        m_cameraOrthoSize = kReferenceOrthoSize;

        GetCameraMatrices(world, view, projection, m_cameraOrthoSize);

        const float bloomBufferHeight = static_cast<float>(win->GetHeight()) / 2.0f;
        const float zoomScale = kReferenceOrthoSize / m_cameraOrthoSize;
        const float bloomRadiusTexels =
            (kDesiredBloomWorldSpread / kReferenceOrthoSize) *
            bloomBufferHeight * zoomScale;

        const glm::mat4 viewProj = projection * view;

        // NOTE: we remove the duplicate light collection and entity bucketing
        // since we now do it above. The RenderGraph passes can use the
        // already-populated `buckets` and `maxLayerId` variables.

        // Reusable temporary buffers to avoid per-entity allocations
        std::vector<glm::vec2> transformedCorners;
        std::vector<glm::vec2> polyPoints;

        // ============================================================
        // RENDER GRAPH SETUP
        // ============================================================
        m_renderGraph->Reset();  // Clear passes from last frame

        // Pass 1: Render scene to HDR framebuffer
        m_renderGraph->AddPass("Scene2D", {}, { "HDR" },
            [this, &world, &viewProj, &maxLayerId, &buckets, &transformedCorners, &polyPoints, &win](ResourceAccessor& res)
            {
                (void)res;
                // Get HDR framebuffer from render graph
                auto* hdrFbo = res.GetFramebuffer("HDR");
                if (!hdrFbo) {
                    std::cerr << "ERROR: HDR framebuffer not found!\n";
                    return;
                }

                // Because of tone-mapping, the background will appear slightly lighter.
                // I chose a slightly brighter neutral gray for a nicer look
                hdrFbo->BindAndClear(0.025f, 0.028f, 0.032f, 1.0f);

                // Get LayerManager for layer visibility and render order
                auto* layerManager = world.GetLayerManager();

                // Determine render order using LayerManager if available, otherwise fall back to manual iteration
                std::vector<uint16_t> renderOrder;
                if (layerManager) {
                    renderOrder = layerManager->DrawOrder();
                } else {
                    // Fallback: manually iterate from 0 to maxLayerId
                    for (int layer = 0; layer <= maxLayerId; ++layer) {
                        renderOrder.push_back(static_cast<uint16_t>(layer));
                    }
                }

                // ---------------------------------------
                // Layered rendering: SDF first, then batch
                // Use LayerManager's render order
                // ---------------------------------------
                for (uint16_t layerId : renderOrder) {
                    // === Check layer visibility and render enabled ===
                    if (layerManager) {
                        const auto& layerData = layerManager->Get(layerId);
                        // Skip layers that are disabled or hidden in editor
                        if (!layerData.renderEnabled || !layerData.editorVisible)
                            continue;
                    }

                    int layer = static_cast<int>(layerId);
                    if (layer >= static_cast<int>(buckets.size())) continue;
                    // Make a local copy of the bucket so we can sort by ZIndex2D
                    auto list = buckets[layer];
                    // Sort by ZIndex2D.ZOrder ascending (smaller drawn first). Entities
                    // without ZIndex2D are treated as ZOrder = 0.
                    std::sort(list.begin(), list.end(), [&](const ECS::Entity& A, const ECS::Entity& B) {
                        int za = 0, zb = 0;
                        if (world.Has<Components::ZIndex2D>(A)) za = world.Get<Components::ZIndex2D>(A).ZOrder;
                        if (world.Has<Components::ZIndex2D>(B)) zb = world.Get<Components::ZIndex2D>(B).ZOrder;
                        if (za != zb) return za < zb;
                        // Stable tie-breaker: entity index
                        return A.Index < B.Index;
                    });

                    glm::mat4 layerViewProj = viewProj;

                    // --- Sub-pass 1: SDF circles on this layer ---
                    m_sdfCircleShader->use();
                    m_sdfCircleShader->setMat4("uViewProj", viewProj);
                    m_sdfCircleShader->setUniform("uStrokePx", 0.0f);
                    m_sdfCircleShader->setUniform("uUseOverrideColor", 0);   // normal circles use their own color
                    m_renderer->beginFrame();

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (world.Has<Components::Active>(entity) &&
                            !world.Get<Components::Active>(entity).Enabled) continue;

                        if (!world.Has<Components::ShapeCircle2D>(entity)) continue;

                        // Transform
                        const auto& lt = world.Get<Components::LocalTransform>(entity);
                        Vector3D position, scale; Quaternion rotation;
                        GetRenderTransform(world, entity, lt, position, rotation, scale);

                        // Draw SDF circle
                        const auto& sc = world.Get<Components::ShapeCircle2D>(entity);
                        DebugDraw2D::Circle(
                            *m_renderer,
                            ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sc.Offset),
                            sc.Radius * ((scale.X + scale.Y) * 0.5f),
                            ToGlm(sc.Color),
                            sc.Filled ? 0.0f : sc.Thickness,
                            /*textureId*/ 0
                        );
                    }

                    m_renderer->endFrame(); // flush SDF for this layer

                    // --- Sub-pass 2: everything else on this layer ---
                    m_shader->use();
                    m_shader->setMat4("uViewProj", viewProj);
                    m_shader->setUniform("uPicking", 0);

                    // enable lighting in batch.frag
                    m_shader->setUniform("uLightingEnabled", 1);

                    // bind SSBO + light uniforms (uPointLightCount/uHasDirectional/uDirLight)
                    m_lightManager.Bind(*m_shader);

                    m_renderer->beginFrame();

                    // ===============================
                    // TILEMAP DRAW (WORLD BACKGROUND)
                    // ===============================
                    if (m_debugTileMap && m_debugTileset)
                    {
                        // Backward-compatible single debug tilemap path.
                        TileMapRenderer tileRenderer;
                        const std::vector<const Tileset*> tilesets = { &m_debugTileset->get() };
                        tileRenderer.Submit(
                            *m_debugTileMap,
                            tilesets,
                            *m_renderer,
                            m_debugTileMapOffset
                        );
                    }

                    if (!m_debugTileMaps.empty())
                    {
                        // Render all tilemaps requested by the editor.
                        TileMapRenderer tileRenderer;
                        for (const auto& entry : m_debugTileMaps) {
                            tileRenderer.Submit(
                                entry.Map.get(),
                                entry.Tilesets,
                                *m_renderer,
                                entry.Offset
                            );
                        }
                    }

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (world.Has<Components::Active>(entity) &&
                            !world.Get<Components::Active>(entity).Enabled) continue;

                        // Skip circles here (already drawn by SDF pass)
                        if (world.Has<Components::ShapeCircle2D>(entity)) continue;

                        // Fetch transform
                        auto& lt = world.Get<Components::LocalTransform>(entity);
                        Vector3D position, scale; Quaternion rotation;
                        GetRenderTransform(world, entity, lt, position, rotation, scale);

                        // Boxes
                        if (world.Has<Components::ShapeBox2D>(entity)) {
                            const auto& sb = world.Get<Components::ShapeBox2D>(entity);
                            const float rotationAngle = 2.0f * std::acos(rotation.W);
                            const bool hasRotation = std::abs(rotationAngle) > 0.01f;

                            if (!hasRotation) {
                                const glm::vec2 halfExtents = ToGlm(Vector2D{ sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y });
                                const glm::vec2 center = ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sb.Offset);
                                const glm::vec2 min = center - halfExtents;
                                const glm::vec2 max = center + halfExtents;

                                if (sb.Filled) DebugDraw2D::RectFill(*m_renderer, min, max, ToGlm(sb.Color), 0);
                                else           DebugDraw2D::RectStroke(*m_renderer, min, max, sb.Thickness, ToGlm(sb.Color), 0);
                            }
                            else {
                                const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                                transformedCorners.clear(); transformedCorners.reserve(4);
                                const Vector2D he = sb.HalfExtents;
                                const Vector3D corners[4] = {
                                    {-he.X, -he.Y, 0.0f}, { he.X, -he.Y, 0.0f},
                                    { he.X,  he.Y, 0.0f}, {-he.X,  he.Y, 0.0f}
                                };
                                for (auto c : corners) {
                                    const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                                    transformedCorners.push_back(ToGlm(Vector2D{ hc.X, hc.Y }) + ToGlm(sb.Offset));
                                }
                                if (sb.Filled) {
                                    DebugDraw2D::Polygon(*m_renderer, transformedCorners, ToGlm(sb.Color), 0);
                                }
                                else {
                                    for (int i = 0; i < 4; ++i)
                                        DebugDraw2D::Line(*m_renderer, transformedCorners[i], transformedCorners[(i + 1) % 4], sb.Thickness, ToGlm(sb.Color), 0);
                                }
                            }
                        }

                        // Lines
                        if (world.Has<Components::ShapeLine2D>(entity)) {
                            const auto& sl = world.Get<Components::ShapeLine2D>(entity);
                            
                            // Build transformation matrix
                            const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                            
                            // Transform endpoints from local to world space
                            const Vector4D worldA = m * Vector4D{sl.A.X, sl.A.Y, 0.0f, 1.0f};
                            const Vector4D worldB = m * Vector4D{sl.B.X, sl.B.Y, 0.0f, 1.0f};
                            
                            DebugDraw2D::Line(*m_renderer,
                                ToGlm(Vector2D{worldA.X, worldA.Y}),
                                ToGlm(Vector2D{worldB.X, worldB.Y}),
                                sl.Thickness, ToGlm(sl.Color), 0);
                        }
                        // Sprites
                        if (world.Has<Components::SpriteRenderer2D>(entity)) {
                            const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                            const float angleZ = std::atan2(
                                2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                                1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
                            );

                            // Calculate UV coordinates from Tiling and Offset
                            const float u0 = sr.Offset.X;
                            const float v0 = sr.Offset.Y;
                            const float u1 = sr.Offset.X + sr.Tiling.X;
                            const float v1 = sr.Offset.Y + sr.Tiling.Y;

                            // Check if entity has Material2D component for PBR rendering
                            GLuint normalTexId = 0;
                            GLuint mraTexId = 0;
                            float metallic = 0.0f;
                            float smoothness = 0.5f;
                            float aoStrength = 1.0f;
                            float normalStrength = 1.0f;
							float flags = 0.0f;

                            if (world.Has<Components::Material2D>(entity)) {
                                const auto& mat = world.Get<Components::Material2D>(entity);
                                normalTexId = mat.NormalTextureId;
                                mraTexId = mat.MRA_TextureId;
                                metallic = mat.Metallic;
                                smoothness = mat.Smoothness;
                                aoStrength = mat.AOStrength;
                                normalStrength = mat.NormalStrength;
                                flags = mat.Flags;
                                if (normalTexId == 0) {
                                    normalTexId = sr.NormalTextureId;
                                }
                            }

                            m_renderer->submitSprite({
                                ToGlm(Vector2D{position.X, position.Y}),
                                ToGlm(Vector2D{scale.X, scale.Y}),
                                {u0, v0, u1, v1},
                                ToGlm(sr.Color),
                                sr.TextureId,
                                angleZ,
                                1.0f,
                                sr.EmissiveTextureId,
                                sr.EmissiveStrength,
                                sr.Width,           // texture width
                                sr.Height,          // texture height
                                normalTexId,        // Material2D: normal map
                                mraTexId,           // Material2D: MRA map
                                metallic,           // Material2D: metallic value
                                smoothness,         // Material2D: smoothness value
                                aoStrength,         // Material2D: AO strength
                                normalStrength,     // Material2D: normal strength
                                static_cast<uint32_t>(flags)
                                });
                        }
                    }

                    m_renderer->endFrame(); // flush non-SDF for this layer
                }
                Framebuffer::Unbind();
            });

        // Object Picking Pass
        m_renderGraph->AddPass("Picking", {}, {},
            [this, &world, &viewProj, &buckets, &win](ResourceAccessor& res)
            {
                // Note: Do NOT skip picking simply because an external camera
                // is set. Editor viewports set an external camera to preview
                // their view; picking should still run if a mouse click or
                // a pending async pick request exists. Earlier logic that
                // unconditionally skipped when m_activeCamera was present
                // prevented the editor from picking. The pass below will
                // early-return when there's no interactive click and no
                // pending request.

                (void)res;
                // Allow the picking pass to run if there is a pending async request or mouse click
                if (!Input::IsMousePressed(MOUSE_LEFT) && m_pendingPickRequests.empty() && !m_currentPickRequest.has_value() && !m_inFlightPick.has_value()) return;

                // Dequeue the next pending request if current one is done
                if (!m_currentPickRequest.has_value() && !m_pendingPickRequests.empty()) {
                    m_currentPickRequest = m_pendingPickRequests.front();
                    m_pendingPickRequests.pop();
                }

                // ============================================================
                // GET VIEWPORT BOUNDS
                // ============================================================
                // By default the picking FBO covers the full window. If an
                // async pick request was submitted by the editor for a
                // sub-region viewport, use that viewport's rect for mapping
                // screen coordinates into FBO texels.
                glm::vec2 viewportMin(0, 0);
                glm::vec2 viewportSize = glm::vec2(win->GetWidth(), win->GetHeight());

                glm::dvec2 mousePos;
                Input::GetMousePosition(mousePos.x, mousePos.y);

                // If there is a current async pick request being processed, 
                // use its viewport rectangle for coordinate mapping
                bool usingCurrentRequest = false;
                if (m_currentPickRequest.has_value()) {
                    viewportMin = m_currentPickRequest->ViewportPos;
                    viewportSize = m_currentPickRequest->ViewportSize;
                    usingCurrentRequest = true;
                }

                // ============================================================
                // RESIZE PICKING FBO IF NEEDED
                // ============================================================
                int vpWidth = static_cast<int>(viewportSize.x);
                int vpHeight = static_cast<int>(viewportSize.y);

                // Check if picking FBO needs resize
                if (m_pickingFBO.Width() != vpWidth || m_pickingFBO.Height() != vpHeight) {
                    LOG_DEBUG("[PICKING] Resizing picking FBO: " << vpWidth << "x" << vpHeight);
                    m_pickingFBO.Resize(vpWidth, vpHeight, false, false);
                }

                // ----------------------------------------------------------------
                // If a previous pick request was submitted last frame, its PBO
                // now contains the pixel result. Map that PBO and move the
                // decoded entity id into m_completedPickResults so clients can
                // poll via TryGetPickResult().
                // ----------------------------------------------------------------
                if (m_inFlightPick.has_value()) {
                    const int idx = m_inFlightPick->PBOIndex;
                    if (idx >= 0 && idx < 2) {
                        m_pbos[idx].Bind(GL_PIXEL_PACK_BUFFER);
                        void* mapped = m_pbos[idx].Map(GL_READ_ONLY);
                        if (mapped) {
                            uint8_t* bytes = static_cast<uint8_t*>(mapped);
                            uint32_t encoded = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16);
                            uint32_t pickedEntity = (encoded == 0) ? INVALID_ENTITY_ID : (encoded - 1);
                            m_completedPickResults[m_inFlightPick->RequestId] = pickedEntity;
                            LOG_DEBUG("[PICKING] In-flight PBO " << idx << " decoded request " << m_inFlightPick->RequestId << " -> entity " << pickedEntity);
                            m_pbos[idx].Unmap();
                        }
                        else {
                            LOG_DEBUG("[PICKING] Warning: failed to map PBO " << idx << " for readback");
                        }
                        m_pbos[idx].Unbind(GL_PIXEL_PACK_BUFFER);
                    }
                    m_inFlightPick.reset();
                }

                m_pickingFBO.BindAndClear(0, 0, 0, 1);

                // Set viewport to match FBO size
                glViewport(0, 0, vpWidth, vpHeight);

                GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
                if (blendWasEnabled) glDisable(GL_BLEND);

                // ============================================================
                // Pass 1: Render circles with SDF shader
                // ============================================================
                m_sdfCircleShader->use();
                m_sdfCircleShader->setMat4("uViewProj", viewProj);
                m_sdfCircleShader->setUniform("uPicking", 1);
                m_shader->setUniform("uLightingEnabled", 0);
                m_renderer->beginFrame();

                for (int layer = 0; layer <= static_cast<int>(buckets.size()) - 1; ++layer) {
                    const auto& list = buckets[layer];

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (world.Has<Components::Active>(entity) &&
                            !world.Get<Components::Active>(entity).Enabled) continue;

                        // Only render circles in this pass
                        if (!world.Has<Components::ShapeCircle2D>(entity)) continue;

                        // Encode entity ID as RGB
                        uint32_t id = entity.Index + 1;
                        glm::vec4 idColor(
                            ((id >> 0) & 0xFF) / 255.0f,
                            ((id >> 8) & 0xFF) / 255.0f,
                            ((id >> 16) & 0xFF) / 255.0f,
                            1.0f
                        );

                        // Get transform
                        const auto& lt = world.Get<Components::LocalTransform>(entity);
                        Vector3D position, scale;
                        Quaternion rotation;
                        GetRenderTransform(world, entity, lt, position, rotation, scale);

                        const auto& sc = world.Get<Components::ShapeCircle2D>(entity);
                        DebugDraw2D::Circle(
                            *m_renderer,
                            ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sc.Offset),
                            sc.Radius * ((scale.X + scale.Y) * 0.5f),
                            idColor,
                            0.0f,
                            0
                        );
                    }
                }

                m_renderer->endFrame();

                // ============================================================
                // Pass 2: Render boxes and sprites with batch shader
                // ============================================================
                m_shader->use();
                m_shader->setMat4("uViewProj", viewProj);
                m_shader->setUniform("uPicking", 1);
                m_shader->setUniform("uLightingEnabled", 0);

                m_renderer->beginFrame();

                for (int layer = 0; layer <= static_cast<int>(buckets.size()) - 1; ++layer) {
                    const auto& list = buckets[layer];

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (world.Has<Components::Active>(entity) &&
                            !world.Get<Components::Active>(entity).Enabled) continue;

                        // Skip circles (already rendered above)
                        if (world.Has<Components::ShapeCircle2D>(entity)) continue;

                        // Encode entity ID as RGB
                        uint32_t id = entity.Index + 1;
                        glm::vec4 idColor(
                            ((id >> 0) & 0xFF) / 255.0f,
                            ((id >> 8) & 0xFF) / 255.0f,
                            ((id >> 16) & 0xFF) / 255.0f,
                            1.0f
                        );

                        // Get transform
                        const auto& lt = world.Get<Components::LocalTransform>(entity);
                        Vector3D position, scale;
                        Quaternion rotation;
                        GetRenderTransform(world, entity, lt, position, rotation, scale);

                        // Render BOXES with ID color
                        if (world.Has<Components::ShapeBox2D>(entity)) {
                            const auto& sb = world.Get<Components::ShapeBox2D>(entity);
                            const glm::vec2 halfExtents = ToGlm(Vector2D{ sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y });
                            const glm::vec2 center = ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sb.Offset);
                            const glm::vec2 min = center - halfExtents;
                            const glm::vec2 max = center + halfExtents;

                            // Use -1 so the shader treats this as a solid-color shape (no texture sampling).
                            // In the picking shader, texIndex >= 0 samples a texture and may alpha-discard.
                            // Negative indices skip sampling and always write the ID color.
                            DebugDraw2D::RectFill(*m_renderer, min, max, idColor, static_cast<GLuint>(-1));
                        }

                        // Render SPRITES with ID color
                        if (world.Has<Components::SpriteRenderer2D>(entity)) {
                            const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                            const float angleZ = std::atan2(
                                2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                                1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
                            );
                            m_renderer->submitSprite({
                                ToGlm(Vector2D{position.X, position.Y}),
                                ToGlm(Vector2D{scale.X, scale.Y}),
                                {0.f, 0.f, 1.f, 1.f},
                                idColor,
                                sr.TextureId,
                                angleZ,
                                1.0f,
                                0,      // emissiveTextureId (no emissive in picking pass)
                                0.0f    // emissiveStrength (no emissive in picking pass)
                                });
                        }
                    }
                }

                m_renderer->endFrame();

                // ============================================================
                // READ PIXEL (now in FBO-local coordinates)
                // ============================================================
                // Determine which screen coordinates to sample. If a current
                // pick request is being processed, use its coordinates.
                // Otherwise use the current mouse position (interactive click).
                glm::vec2 sampleScreenPos;
                bool usingCurrentPickRequest = false;
                if (m_currentPickRequest.has_value()) {
                    sampleScreenPos = glm::vec2(m_currentPickRequest->ScreenX, m_currentPickRequest->ScreenY);
                    usingCurrentPickRequest = true;
                }
                else {
                    sampleScreenPos = glm::vec2(mousePos.x, mousePos.y);
                }

                // Convert sampleScreenPos into viewport-local coordinates
                glm::vec2 localPos = sampleScreenPos - viewportMin;

                // Map viewport-local coordinates to FBO pixel coordinates
                const int fboWidth = m_pickingFBO.Width();
                const int fboHeight = m_pickingFBO.Height();

                int readX = 0;
                int readY = 0;
                vpWidth = static_cast<int>(viewportSize.x);
                vpHeight = static_cast<int>(viewportSize.y);

                if (fboWidth != vpWidth || fboHeight != vpHeight) {
                    const float sx = static_cast<float>(fboWidth) / static_cast<float>(vpWidth);
                    const float sy = static_cast<float>(fboHeight) / static_cast<float>(vpHeight);
                    readX = glm::clamp(static_cast<int>(localPos.x * sx), 0, fboWidth - 1);
                    // Flip Y: viewport local origin is top-left for screen coords
                    readY = glm::clamp(static_cast<int>((vpHeight - localPos.y - 1.0f) * sy), 0, fboHeight - 1);
                }
                else {
                    // 1:1 mapping
                    readX = glm::clamp(static_cast<int>(localPos.x), 0, fboWidth - 1);
                    readY = glm::clamp(static_cast<int>(vpHeight - localPos.y - 1.0f), 0, fboHeight - 1);
                }

                LOG_DEBUG("[PICKING] FBO size: " << fboWidth << "x" << fboHeight);
                LOG_DEBUG("[PICKING] Reading pixel: (" << readX << ", " << readY << ")");
                if (usingCurrentPickRequest && m_currentPickRequest.has_value()) {
                    LOG_DEBUG("[PICKING] Servicing async request " << m_currentPickRequest->RequestId);
                }

                // Frame N: Write to current PBO (async transfer starts)
                m_pbos[m_currentPBO].Bind(GL_PIXEL_PACK_BUFFER);
                glReadPixels(readX, readY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
                m_pbos[m_currentPBO].Unbind(GL_PIXEL_PACK_BUFFER);

                // If this read corresponds to a current async pick request,
                // mark it as in-flight and associate it with the current PBO
                // so the result can be consumed on the next frame.
                if (usingCurrentPickRequest) {
                    m_inFlightPick = InFlightPick{ m_currentPickRequest->RequestId, m_currentPBO };
                    m_currentPickRequest.reset();
                }

                // Swap PBOs for the next frame
                m_currentPBO = 1 - m_currentPBO;


                // Restore blending state
                if (blendWasEnabled) glEnable(GL_BLEND);

                Framebuffer::Unbind();

                // Restore viewport to full window
                glViewport(0, 0, win->GetWidth(), win->GetHeight());
            });


        m_renderGraph->AddPass("BloomExtract", { "HDR" }, { "BloomExtract" },
                [this](ResourceAccessor& res)
                {
                    auto* hdrFbo = res.GetFramebuffer("HDR");
                    auto* extractFbo = res.GetFramebuffer("BloomExtract");
                    if (!hdrFbo || !extractFbo) return;

                    extractFbo->BindAndClear(0, 0, 0, 1);
                    m_bloomExtractShader->use();
                    m_bloomExtractShader->setUniform("uThreshold", 1.1f);   // brightness threshold
                    m_bloomExtractShader->setUniform("uScene", 0);
                    hdrFbo->BindColorTexture(0);                            // texture unit 0 in shader
                    m_renderer->drawFullscreenQuad();
                    Framebuffer::Unbind();
                });

        m_renderGraph->AddPass("BloomBlurH", { "BloomExtract" }, { "BloomBlur" },
            [this, &bloomRadiusTexels](ResourceAccessor& res)
            {
                auto* src = res.GetFramebuffer("BloomExtract");
                auto* dst = res.GetFramebuffer("BloomBlur");
                if (!src || !dst) return;

                dst->BindAndClear(0, 0, 0, 1);
                m_bloomBlurShader->use();
                m_bloomBlurShader->setUniform("uHorizontal", 1);
                m_bloomBlurShader->setUniform("uImage", 0);
                m_bloomBlurShader->setUniform("uRadius", bloomRadiusTexels);
                m_bloomBlurShader->setUniform("uSamples", std::max(12, static_cast<int>(bloomRadiusTexels * 0.6f)));     // Increase uSamples proportionally to uRadius
                m_bloomBlurShader->setUniform("uFalloff", 0.15f);  // LESS FALLOFF
                src->BindColorTexture(0);
                m_renderer->drawFullscreenQuad();
                Framebuffer::Unbind();
            });

        m_renderGraph->AddPass("BloomBlurV", { "BloomBlur" }, { "BloomExtract" },
            [this, &bloomRadiusTexels](ResourceAccessor& res)
            {
                auto* src = res.GetFramebuffer("BloomBlur");
                auto* dst = res.GetFramebuffer("BloomExtract");
                if (!src || !dst) return;

                dst->BindAndClear(0, 0, 0, 1);
                m_bloomBlurShader->use();
                m_bloomBlurShader->setUniform("uHorizontal", 0);
                m_bloomBlurShader->setUniform("uImage", 0);
                m_bloomBlurShader->setUniform("uRadius", bloomRadiusTexels);
                m_bloomBlurShader->setUniform("uSamples", std::max(12, static_cast<int>(bloomRadiusTexels * 0.6f)));     // Increase uSamples proportionally to uRadius
                m_bloomBlurShader->setUniform("uFalloff", 0.15f);  // LESS FALLOFF

                src->BindColorTexture(0);
                m_renderer->drawFullscreenQuad();
                Framebuffer::Unbind();
            });

        // ToneMap pass -> writes final color to LDR texture
        m_renderGraph->AddPass("ToneMap", { "HDR", "BloomExtract" }, { "LDR" },
            [this](ResourceAccessor& res)
            {
                auto* hdr = res.GetFramebuffer("HDR");
                auto* bloom = res.GetFramebuffer("BloomExtract");
                auto* ldr = res.GetFramebuffer("LDR");
                if (!hdr || !bloom || !ldr) return;

                ldr->BindAndClear(0, 0, 0, 1);

                m_bloomCombineShader->use();
                m_bloomCombineShader->setUniform("uScene", 0);
                m_bloomCombineShader->setUniform("uBloomBlur", 1);
                m_bloomCombineShader->setUniform("uExposure", 1.3f);
                m_bloomCombineShader->setUniform("uBloomStrength", 5.2f);
                m_bloomCombineShader->setUniform("uGamma", 1.5f);

                hdr->BindColorTexture(0, 0);
                bloom->BindColorTexture(0, 1);

                m_renderer->drawFullscreenQuad();
                Framebuffer::Unbind();
            });

        // Wireframe Pass - Render debug/editor wireframes on top of tone-mapped scene
        m_renderGraph->AddPass("Wireframe", { "LDR" }, { "LDR" },
            [this, &viewProj](ResourceAccessor& res)
            {
                // Skip if nothing is queued for overlays or wireframes
                if (m_wireframeQueue.empty() && m_overlayQuadQueue.empty()) return;

                auto* ldr = res.GetFramebuffer("LDR");
                if (!ldr) return;

                // Bind LDR framebuffer for rendering on top of tone-mapped content
                ldr->Bind();

                // Enable blending for wireframes
                GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                // Process all queued overlay quads (textured previews, etc.)
                if (!m_overlayQuadQueue.empty()) {
                    if (m_shader) {
                        m_shader->use();
                        m_shader->setMat4("uViewProj", viewProj);
                        m_shader->setUniform("uPicking", 0);
                        m_shader->setUniform("uLightingEnabled", 0);
                    }
                    m_renderer->beginFrame();
                    for (const auto& quad : m_overlayQuadQueue) {
                        m_renderer->submitQuad(
                            quad.center,
                            quad.size,
                            quad.textureId,
                            quad.uvRect,
                            quad.color,
                            quad.rotation,
                            1.0f,
                            0
                        );
                    }
                    m_renderer->endFrame();
                }

                // Process all queued wireframe submissions
                for (const auto& sub : m_wireframeQueue) {
                    switch (sub.type) {
                    case WireframeSubmission::Type::Circle: {
                        // Circles need SDF shader
                        m_sdfCircleShader->use();
                        m_sdfCircleShader->setMat4("uViewProj", viewProj);
                        m_sdfCircleShader->setUniform("uPicking", 0);
                        m_renderer->beginFrame();

                        DebugDraw2D::Circle(*m_renderer,
                            sub.center,
                            sub.radius,
                            sub.color,
                            sub.filled ? 0.0f : sub.thickness,
                            0
                        );

                        m_renderer->endFrame();
                        break;
                    }
                    case WireframeSubmission::Type::Polygon: {
                        // Use batch shader for line-based shapes
                        if (m_shader) {
                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 0);
                        }
                        m_renderer->beginFrame();

                        if (sub.filled && sub.vertices.size() >= 3) {
                            DebugDraw2D::Polygon(*m_renderer, sub.vertices, sub.color, 0);
                        }
                        else if (sub.vertices.size() >= 2) {
                            for (size_t i = 0; i < sub.vertices.size(); ++i) {
                                size_t next = sub.closed ? (i + 1) % sub.vertices.size() : i + 1;
                                if (next < sub.vertices.size()) {
                                    DebugDraw2D::Line(*m_renderer, sub.vertices[i], sub.vertices[next],
                                        sub.thickness, sub.color, 0);
                                }
                            }
                        }

                        m_renderer->endFrame();
                        break;
                    }
                    case WireframeSubmission::Type::Line: {
                        if (m_shader) {
                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 0);
                        }
                        m_renderer->beginFrame();

                        if (sub.vertices.size() == 2) {
                            DebugDraw2D::Line(*m_renderer, sub.vertices[0], sub.vertices[1],
                                sub.thickness, sub.color, 0);
                        }

                        m_renderer->endFrame();
                        break;
                    }
                    case WireframeSubmission::Type::Mesh: {
                        if (m_shader) {
                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 0);
                        }
                        m_renderer->beginFrame();

                        if (!sub.indices.empty()) {
                            // Draw using indices
                            for (size_t i = 0; i < sub.indices.size(); i += 2) {
                                if (i + 1 < sub.indices.size()) {
                                    uint32_t idx0 = sub.indices[i];
                                    uint32_t idx1 = sub.indices[i + 1];
                                    if (idx0 < sub.vertices.size() && idx1 < sub.vertices.size()) {
                                        DebugDraw2D::Line(*m_renderer, sub.vertices[idx0], sub.vertices[idx1],
                                            sub.thickness, sub.color, 0);
                                    }
                                }
                            }
                        }
                        else {
                            // Draw as sequence of lines
                            for (size_t i = 0; i + 1 < sub.vertices.size(); i += 2) {
                                DebugDraw2D::Line(*m_renderer, sub.vertices[i], sub.vertices[i + 1],
                                    sub.thickness, sub.color, 0);
                            }
                        }

                        m_renderer->endFrame();
                        break;
                    }
                    case WireframeSubmission::Type::Quad: {
                        // Switch to batch shader for non-circle shapes
                        if (m_shader) {
                            m_shader->use();
                            m_shader->setMat4("uViewProj", viewProj);
                            m_shader->setUniform("uPicking", 0);
                            m_shader->setUniform("uLightingEnabled", 0);
                        }
                        m_renderer->beginFrame();

                        if (sub.vertices.size() == 4) {
                            const auto& min = sub.vertices[0];
                            const auto& max = sub.vertices[2];
                            if (sub.filled) {
                                DebugDraw2D::RectFill(*m_renderer, min, max, sub.color, 0);
                            }
                            else {
                                DebugDraw2D::RectStroke(*m_renderer, min, max, sub.thickness, sub.color, 0);
                            }
                        }
                        m_renderer->endFrame();
                        break;
                    }
                    }
                }

                m_renderer->endFrame();

                // Clear overlay/wireframe queues for next frame
                m_overlayQuadQueue.clear();
                m_wireframeQueue.clear();

                // Restore blend state
                if (!blendWasEnabled) glDisable(GL_BLEND);

                Framebuffer::Unbind();
            });

        // GUI Pass - Render GUI panels on top of scene
        m_renderGraph->AddPass("GUI", { "LDR" }, { "LDR" },
            [this](ResourceAccessor& res)
            {
                // Skip if no GUI elements queued
                if (m_guiPanelQueue.empty() && m_guiTextQueue.empty()) return;

                auto* ldr = res.GetFramebuffer("LDR");
                if (!ldr) return;

                // Setup orthographic projection for screen-space rendering
                const float width = static_cast<float>(ldr->Width());
                const float height = static_cast<float>(ldr->Height());

                ldr->Bind();
                glViewport(0, 0, ldr->Width(), ldr->Height());

                // Enable blending for GUI rendering
                GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                // Orthographic projection matrix
                glm::mat4 screenOrtho = glm::ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);

                // Setup scissor test based on GUI viewport
                bool scissorEnabled = false;
                GUIViewport viewport = m_guiViewport;
                if (!viewport.Active || viewport.Size.X <= 0.0f || viewport.Size.Y <= 0.0f) {
                    viewport.Origin = { 0.0f, 0.0f };
                    viewport.Size = { width, height };
                }

                // Enable scissor test if viewport size is valid
                if (viewport.Size.X > 0.0f && viewport.Size.Y > 0.0f) {
                    glEnable(GL_SCISSOR_TEST);
                    scissorEnabled = true;
                    const int scissorX = static_cast<int>(std::round(viewport.Origin.X));
                    const int scissorY = static_cast<int>(std::round(height - (viewport.Origin.Y + viewport.Size.Y)));
                    const int scissorW = static_cast<int>(std::round(viewport.Size.X));
                    const int scissorH = static_cast<int>(std::round(viewport.Size.Y));
                    glScissor(scissorX, scissorY, scissorW, scissorH);
                }

                // Render GUI panels
                if (!m_guiPanelQueue.empty()) {
                    if (m_shader) {
                        m_shader->use();
                        m_shader->setMat4("uViewProj", screenOrtho);
                        m_shader->setUniform("uLightingEnabled", 0);
                    }
                    m_renderer->beginFrame();
                    for (const auto& panel : m_guiPanelQueue) { // Render each panel
                        const glm::vec2 center(panel.position.X + panel.size.X * 0.5f,
                                               panel.position.Y + panel.size.Y * 0.5f);
                        const glm::vec2 size(panel.size.X, panel.size.Y);
                        const glm::vec4 color(panel.color.R, panel.color.G, panel.color.B, panel.color.A);
                        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
                        GLuint textureId = 0;
                        m_renderer->submitQuad(center, size, textureId, uvRect, color, 0.0f, 1.0f, 0, 0u, 0.0f);
                    }
                    m_renderer->endFrame();
                }

                // Render GUI text
                if (!m_guiTextQueue.empty()) {
                    const glm::mat4 textOrtho = glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
                    if (m_textShader) {
                        m_textShader->use();
                        m_textShader->setMat4("uProjection", textOrtho);
                    }
                    m_renderer->beginFrame();
                    for (const auto& text : m_guiTextQueue) { // Render each text element
                        if (text.text.empty()) {
                            continue;
                        }

                        // Load font
                        const std::string fontPath = text.fontPath.empty()
                            ? std::string("assets/fonts/Roboto/Roboto-Regular.ttf")
                            : text.fontPath;
                        const int pixelSize = std::max(1, static_cast<int>(std::round(text.pixelSize)));
                        auto font = RM.GetFont(fontPath, pixelSize);
                        if (!font) {
                            continue;
                        }

                        // Calculate text position (flip Y for GUI space)
                        const glm::vec2 textPos(text.position.X, height - text.position.Y);
                        const glm::vec4 color(text.color.R, text.color.G, text.color.B, text.color.A);
                        m_renderer->submitText(*font, text.text, textPos, color, text.pixelSize);
                    }
                    m_renderer->endFrame();
                }

                // Disable scissor test if it was enabled
                if (scissorEnabled) {
                    glDisable(GL_SCISSOR_TEST);
                }

                // Clear GUI queues for next frame
                m_guiPanelQueue.clear();
                m_guiTextQueue.clear();

                if (!blendWasEnabled) glDisable(GL_BLEND);
                Framebuffer::Unbind();
            });

        // Blit LDR to backbuffer
        m_renderGraph->AddPass("Composite", { "LDR" }, { "Backbuffer" },
            [this, &win](ResourceAccessor& res)
            {
                auto* ldr = res.GetFramebuffer("LDR");
                if (!ldr) return;

                Framebuffer::BindDefault();
                glViewport(0, 0, win->GetWidth(), win->GetHeight());

                // Use a simple blit shader, NOT bloomCombine
                m_blitShader->use();
                m_blitShader->setUniform("uTex", 0);
                ldr->BindColorTexture(0, 0);

                m_renderer->drawFullscreenQuad();
            });

        // ============================================================
        // EXECUTE RENDER GRAPH
        // ============================================================
        m_renderGraph->Execute();

        // Performance logging
        if (TimeSystem::Instance().GetFrameCount() % 120 == 0)
        {
            static int previousFlushTotal = 0;
            int currentTotal = GetFlushCount();
            int flushes = currentTotal - previousFlushTotal;
            previousFlushTotal = currentTotal;

            std::stringstream ss;
            if (flushes > 10)
                ss << " Too many flushes! Likely texture switches or buffer overflows...";
            else if (flushes == 1)
                ss << " Single batch, bottleneck is CPU-side or GPU fillrate";

            LOG_DEBUG("Flushes this frame: " << flushes << ss.str() << " | " << "FPS: " <<  static_cast<int>(1.0f / TimeSystem::Instance().GetDeltaTime()));
        }
    }

    void RendererSystem::OnDestroy(World& world) {
        (void)world;
        // Cleanup rendering resources
        m_renderer.reset();
        m_renderGraph.reset();
        m_shader.reset();
        m_textShader.reset();
        m_sdfCircleShader.reset();
        m_blitShader.reset();
        m_bloomBlurShader.reset();
        m_bloomExtractShader.reset();
        m_bloomCombineShader.reset();
        m_pickingFBO.Destroy();
        g_rendererSystemInstance = nullptr;
        m_lightManager.Shutdown();
    }

    // ====================================================================
    // Viewport Management Implementation
    // ====================================================================

    void RendererSystem::AddViewport(const std::string& name, Engine::Camera* camera, int w, int h) {
        // Check if viewport already exists
        for (auto& vp : m_viewports) {
            if (vp.Name == name) {
                vp.Camera = camera;
                ResizeViewport(name, w, h);
                return;
            }
        }

        Viewport vp;
        vp.Name = name;
        vp.Camera = camera;
        vp.Size = { std::max(1, w), std::max(1, h) };
        vp.Active = true;

        // Create per-viewport FBOs
        vp.HDR = std::make_unique<Framebuffer>();
        vp.HDR->Create(vp.Size.x, vp.Size.y, true, true, 1);

        vp.LDR = std::make_unique<Framebuffer>();
        vp.LDR->Create(vp.Size.x, vp.Size.y, false, false, 1);

        vp.BloomExtract = std::make_unique<Framebuffer>();
        vp.BloomExtract->Create(vp.Size.x / 2, vp.Size.y / 2, true, false, 1);

        vp.BloomBlur = std::make_unique<Framebuffer>();
        vp.BloomBlur->Create(vp.Size.x / 2, vp.Size.y / 2, true, false, 1);

        vp.PickingFBO = std::make_unique<Framebuffer>();
        vp.PickingFBO->Create(vp.Size.x, vp.Size.y, false, false, 1);

        m_viewports.push_back(std::move(vp));

        LOG_DEBUG("[Viewport] Created '" << name << "' (" << w << "x" << h << ")");
    }

    void RendererSystem::RemoveViewport(const std::string& name) {
        m_viewports.erase(
            std::remove_if(m_viewports.begin(), m_viewports.end(),
                [&](const Viewport& vp) { return vp.Name == name; }),
            m_viewports.end());

        LOG_DEBUG("[Viewport] Removed '" << name << "'");
    }

    void RendererSystem::ResizeViewport(const std::string& name, int w, int h) {
        Viewport* vp = GetViewport(name);
        if (!vp) return;

        w = std::max(1, w);
        h = std::max(1, h);

        if (vp->Size.x == w && vp->Size.y == h) return;

        vp->Size = { w, h };

        vp->HDR->Resize(w, h, true, true);
        vp->LDR->Resize(w, h, false, false);
        vp->BloomExtract->Resize(w / 2, h / 2, true, false);
        vp->BloomBlur->Resize(w / 2, h / 2, true, false);
        vp->PickingFBO->Resize(w, h, false, false);

        LOG_DEBUG("[Viewport] Resized '" << name << "' to " << w << "x" << h);
    }

    void RendererSystem::SetViewportCamera(const std::string& name, Engine::Camera* camera) {
        if (Viewport* vp = GetViewport(name))
            vp->Camera = camera;
    }

    RendererSystem::Viewport* RendererSystem::GetViewport(const std::string& name) {
        for (auto& vp : m_viewports)
            if (vp.Name == name) return &vp;
        return nullptr;
    }

    GLuint RendererSystem::GetViewportTexture(const std::string& name) const {
        for (const auto& vp : m_viewports)
            if (vp.Name == name && vp.LDR)
                return vp.LDR->GetColorTexture(0);
        return 0;
    }

    // ====================================================================
// Extracted Render Helpers
// ====================================================================

    void RendererSystem::CollectLights(World& world) {
        m_lightManager.BeginFrame();

        world.Each<Components::LocalTransform, Components::Light2D>(
            [&](ECS::Entity e, const Components::LocalTransform& lt, const Components::Light2D& l) {
                if (world.Has<Components::Active>(e) && !world.Get<Components::Active>(e).Enabled)
                    return;

                Vector3D position, scale;
                Quaternion rotation;
                GetRenderTransform(world, e, lt, position, rotation, scale);

                glm::vec3 color = glm::vec3(ToGlm(l.Color));

                if (l.LightType == Components::Light2D::Type::Directional) {
                    glm::vec3 dir(l.Direction.X, l.Direction.Y, l.Direction.Z);
                    if (glm::dot(dir, dir) < 1e-8f) dir = glm::vec3(0.0f, -1.0f, 0.0f);
                    dir = glm::normalize(dir);
                    m_lightManager.SetDirectionalLight(dir, color, l.Intensity);
                }
                else {
                    glm::vec3 worldPos(position.X, position.Y, position.Z);
                    worldPos += glm::vec3(l.Position.X, l.Position.Y, l.Position.Z);
                    m_lightManager.AddPointLight(worldPos, l.Range, color, l.Intensity);
                }
            });

        m_lightManager.Upload();
    }

    void RendererSystem::BucketEntities(World& world,
        std::vector<std::vector<Entity>>& buckets,
        int& maxLayerId) {
        maxLayerId = -1;
        world.Each<Components::Layer>([&](Entity, const Components::Layer& ly) {
            maxLayerId = std::max(static_cast<int>(ly.Id), maxLayerId);
            });

        buckets.clear();
        buckets.resize(std::max(1, maxLayerId + 1));

        world.Each<Components::LocalTransform, Components::Layer>(
            [&](Entity entity, Components::LocalTransform&, const Components::Layer& ly) {
                if (ly.Id < buckets.size())
                    buckets[ly.Id].push_back(entity);
            });
    }

    void RendererSystem::RenderBloom(Viewport& vp, float bloomRadius) {
        // Extract
        vp.BloomExtract->BindAndClear(0, 0, 0, 1);
        glViewport(0, 0, vp.BloomExtract->Width(), vp.BloomExtract->Height());
        m_bloomExtractShader->use();
        m_bloomExtractShader->setUniform("uThreshold", 1.1f);
        m_bloomExtractShader->setUniform("uScene", 0);
        vp.HDR->BindColorTexture(0);
        m_renderer->drawFullscreenQuad();

        // Blur Horizontal
        vp.BloomBlur->BindAndClear(0, 0, 0, 1);
        glViewport(0, 0, vp.BloomBlur->Width(), vp.BloomBlur->Height());
        m_bloomBlurShader->use();
        m_bloomBlurShader->setUniform("uHorizontal", 1);
        m_bloomBlurShader->setUniform("uImage", 0);
        m_bloomBlurShader->setUniform("uRadius", bloomRadius);
        m_bloomBlurShader->setUniform("uSamples", std::max(12, static_cast<int>(bloomRadius * 0.6f)));
        m_bloomBlurShader->setUniform("uFalloff", 0.15f);
        vp.BloomExtract->BindColorTexture(0);
        m_renderer->drawFullscreenQuad();

        // Blur Vertical
        vp.BloomExtract->BindAndClear(0, 0, 0, 1);
        m_bloomBlurShader->setUniform("uHorizontal", 0);
        vp.BloomBlur->BindColorTexture(0);
        m_renderer->drawFullscreenQuad();

        Framebuffer::Unbind();
    }

    void RendererSystem::ToneMap(Viewport& vp) {
        vp.LDR->BindAndClear(0, 0, 0, 1);
        glViewport(0, 0, vp.Size.x, vp.Size.y);

        m_bloomCombineShader->use();
        m_bloomCombineShader->setUniform("uScene", 0);
        m_bloomCombineShader->setUniform("uBloomBlur", 1);
        m_bloomCombineShader->setUniform("uExposure", 1.3f);
        m_bloomCombineShader->setUniform("uBloomStrength", 5.2f);
        m_bloomCombineShader->setUniform("uGamma", 1.5f);

        vp.HDR->BindColorTexture(0, 0);
        vp.BloomExtract->BindColorTexture(0, 1);
        m_renderer->drawFullscreenQuad();

        Framebuffer::Unbind();
    }

    void RendererSystem::RenderSceneToHDR(World& world, Viewport& vp, const glm::mat4& viewProj,
        const std::vector<std::vector<Entity>>& buckets,
        int maxLayerId) {
        vp.HDR->BindAndClear(0.025f, 0.028f, 0.032f, 1.0f);
        glViewport(0, 0, vp.Size.x, vp.Size.y);

        auto* layerManager = world.GetLayerManager();
        std::vector<uint16_t> renderOrder;
        if (layerManager) {
            renderOrder = layerManager->DrawOrder();
        }
        else {
            for (int layer = 0; layer <= maxLayerId; ++layer)
                renderOrder.push_back(static_cast<uint16_t>(layer));
        }

        std::vector<glm::vec2> transformedCorners;

        for (uint16_t layerId : renderOrder) {
            if (layerManager) {
                const auto& layerData = layerManager->Get(layerId);
                if (!layerData.renderEnabled || !layerData.editorVisible) continue;
            }

            int layer = static_cast<int>(layerId);
            if (layer >= static_cast<int>(buckets.size())) continue;

            auto list = buckets[layer];
            std::sort(list.begin(), list.end(), [&](const Entity& A, const Entity& B) {
                int za = 0, zb = 0;
                if (world.Has<Components::ZIndex2D>(A)) za = world.Get<Components::ZIndex2D>(A).ZOrder;
                if (world.Has<Components::ZIndex2D>(B)) zb = world.Get<Components::ZIndex2D>(B).ZOrder;
                if (za != zb) return za < zb;
                return A.Index < B.Index;
                });

            // SDF circles
            m_sdfCircleShader->use();
            m_sdfCircleShader->setMat4("uViewProj", viewProj);
            m_sdfCircleShader->setUniform("uStrokePx", 0.0f);
            m_sdfCircleShader->setUniform("uUseOverrideColor", 0);
            m_renderer->beginFrame();

            for (Entity entity : list) {
                if (world.Has<Components::Active>(entity) && !world.Get<Components::Active>(entity).Enabled) continue;
                if (!world.Has<Components::ShapeCircle2D>(entity)) continue;

                const auto& lt = world.Get<Components::LocalTransform>(entity);
                Vector3D position, scale; Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                const auto& sc = world.Get<Components::ShapeCircle2D>(entity);
                DebugDraw2D::Circle(*m_renderer,
                    ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sc.Offset),
                    sc.Radius * ((scale.X + scale.Y) * 0.5f),
                    ToGlm(sc.Color),
                    sc.Filled ? 0.0f : sc.Thickness, 0);
            }
            m_renderer->endFrame();

            // Everything else
            m_shader->use();
            m_shader->setMat4("uViewProj", viewProj);
            m_shader->setUniform("uPicking", 0);
            m_shader->setUniform("uLightingEnabled", 1);
            m_lightManager.Bind(*m_shader);
            m_renderer->beginFrame();

            // Tilemap
            if (m_debugTileMap && m_debugTileset) {
                TileMapRenderer tileRenderer;
                tileRenderer.Submit(*m_debugTileMap, *m_debugTileset, *m_renderer);
            }

            for (Entity entity : list) {
                if (world.Has<Components::Active>(entity) && !world.Get<Components::Active>(entity).Enabled) continue;
                if (world.Has<Components::ShapeCircle2D>(entity)) continue;

                auto& lt = world.Get<Components::LocalTransform>(entity);
                Vector3D position, scale; Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                // Boxes
                if (world.Has<Components::ShapeBox2D>(entity)) {
                    const auto& sb = world.Get<Components::ShapeBox2D>(entity);
                    const float rotationAngle = 2.0f * std::acos(rotation.W);
                    const bool hasRotation = std::abs(rotationAngle) > 0.01f;

                    if (!hasRotation) {
                        const glm::vec2 halfExtents = ToGlm(Vector2D{ sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y });
                        const glm::vec2 center = ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sb.Offset);
                        const glm::vec2 min = center - halfExtents;
                        const glm::vec2 max = center + halfExtents;
                        if (sb.Filled) DebugDraw2D::RectFill(*m_renderer, min, max, ToGlm(sb.Color), 0);
                        else DebugDraw2D::RectStroke(*m_renderer, min, max, sb.Thickness, ToGlm(sb.Color), 0);
                    }
                    else {
                        const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                        transformedCorners.clear();
                        const Vector2D he = sb.HalfExtents;
                        const Vector3D corners[4] = { {-he.X,-he.Y,0},{he.X,-he.Y,0},{he.X,he.Y,0},{-he.X,he.Y,0} };
                        for (auto c : corners) {
                            const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                            transformedCorners.push_back(ToGlm(Vector2D{ hc.X, hc.Y }) + ToGlm(sb.Offset));
                        }
                        if (sb.Filled) DebugDraw2D::Polygon(*m_renderer, transformedCorners, ToGlm(sb.Color), 0);
                        else for (int i = 0; i < 4; ++i)
                            DebugDraw2D::Line(*m_renderer, transformedCorners[i], transformedCorners[(i + 1) % 4], sb.Thickness, ToGlm(sb.Color), 0);
                    }
                }

                // Lines
                if (world.Has<Components::ShapeLine2D>(entity)) {
                    const auto& sl = world.Get<Components::ShapeLine2D>(entity);
                    const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                    const Vector4D worldA = m * Vector4D{ sl.A.X, sl.A.Y, 0.0f, 1.0f };
                    const Vector4D worldB = m * Vector4D{ sl.B.X, sl.B.Y, 0.0f, 1.0f };
                    DebugDraw2D::Line(*m_renderer, ToGlm(Vector2D{ worldA.X, worldA.Y }), ToGlm(Vector2D{ worldB.X, worldB.Y }), sl.Thickness, ToGlm(sl.Color), 0);
                }

                // Sprites
                if (world.Has<Components::SpriteRenderer2D>(entity)) {
                    const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                    const float angleZ = std::atan2(
                        2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                        1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z));

                    GLuint normalTexId = 0, mraTexId = 0;
                    float metallic = 0.0f, smoothness = 0.5f, aoStrength = 1.0f, normalStrength = 1.0f, flags = 0.0f;

                    if (world.Has<Components::Material2D>(entity)) {
                        const auto& mat = world.Get<Components::Material2D>(entity);
                        normalTexId = mat.NormalTextureId;
                        mraTexId = mat.MRA_TextureId;
                        metallic = mat.Metallic;
                        smoothness = mat.Smoothness;
                        aoStrength = mat.AOStrength;
                        normalStrength = mat.NormalStrength;
                        flags = mat.Flags;
                        if (normalTexId == 0) normalTexId = sr.NormalTextureId;
                    }

                    m_renderer->submitSprite({
                        ToGlm(Vector2D{position.X, position.Y}),
                        ToGlm(Vector2D{scale.X, scale.Y}),
                        {sr.Offset.X, sr.Offset.Y, sr.Offset.X + sr.Tiling.X, sr.Offset.Y + sr.Tiling.Y},
                        ToGlm(sr.Color), sr.TextureId, angleZ, 1.0f,
                        sr.EmissiveTextureId, sr.EmissiveStrength, sr.Width, sr.Height,
                        normalTexId, mraTexId, metallic, smoothness, aoStrength, normalStrength,
                        static_cast<uint32_t>(flags)
                        });
                }
            }
            m_renderer->endFrame();
        }

        Framebuffer::Unbind();
    }

    void RendererSystem::RenderWireframes(Viewport& vp, const glm::mat4& viewProj) {
        if (m_wireframeQueue.empty()) return;

        vp.LDR->Bind();
        glViewport(0, 0, vp.Size.x, vp.Size.y);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (const auto& sub : m_wireframeQueue) {
            if (sub.type == WireframeSubmission::Type::Circle) {
                m_sdfCircleShader->use();
                m_sdfCircleShader->setMat4("uViewProj", viewProj);
                m_sdfCircleShader->setUniform("uPicking", 0);
                m_renderer->beginFrame();
                DebugDraw2D::Circle(*m_renderer, sub.center, sub.radius, sub.color, sub.filled ? 0.0f : sub.thickness, 0);
                m_renderer->endFrame();
            }
            else {
                m_shader->use();
                m_shader->setMat4("uViewProj", viewProj);
                m_shader->setUniform("uPicking", 0);
                m_shader->setUniform("uLightingEnabled", 0);
                m_renderer->beginFrame();

                if (sub.type == WireframeSubmission::Type::Quad && sub.vertices.size() == 4) {
                    if (sub.filled) DebugDraw2D::RectFill(*m_renderer, sub.vertices[0], sub.vertices[2], sub.color, 0);
                    else DebugDraw2D::RectStroke(*m_renderer, sub.vertices[0], sub.vertices[2], sub.thickness, sub.color, 0);
                }
                else if (sub.type == WireframeSubmission::Type::Line && sub.vertices.size() == 2) {
                    DebugDraw2D::Line(*m_renderer, sub.vertices[0], sub.vertices[1], sub.thickness, sub.color, 0);
                }
                else if (sub.type == WireframeSubmission::Type::Polygon && sub.vertices.size() >= 2) {
                    if (sub.filled && sub.vertices.size() >= 3) DebugDraw2D::Polygon(*m_renderer, sub.vertices, sub.color, 0);
                    else for (size_t i = 0; i < sub.vertices.size(); ++i) {
                        size_t next = sub.closed ? (i + 1) % sub.vertices.size() : i + 1;
                        if (next < sub.vertices.size())
                            DebugDraw2D::Line(*m_renderer, sub.vertices[i], sub.vertices[next], sub.thickness, sub.color, 0);
                    }
                }
                m_renderer->endFrame();
            }
        }

        Framebuffer::Unbind();
    }

    void RendererSystem::RenderGUI(Viewport& vp) {
        if (m_guiPanelQueue.empty() && m_guiTextQueue.empty()) return;

        vp.LDR->Bind();
        glViewport(0, 0, vp.Size.x, vp.Size.y);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float w = static_cast<float>(vp.Size.x);
        const float h = static_cast<float>(vp.Size.y);
        glm::mat4 screenOrtho = glm::ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

        // Panels
        if (!m_guiPanelQueue.empty()) {
            m_shader->use();
            m_shader->setMat4("uViewProj", screenOrtho);
            m_shader->setUniform("uLightingEnabled", 0);
            m_renderer->beginFrame();
            for (const auto& panel : m_guiPanelQueue) {
                glm::vec2 center(panel.position.X + panel.size.X * 0.5f, panel.position.Y + panel.size.Y * 0.5f);
                glm::vec2 size(panel.size.X, panel.size.Y);
                glm::vec4 color(panel.color.R, panel.color.G, panel.color.B, panel.color.A);
                m_renderer->submitQuad(center, size, 0, { 0,0,1,1 }, color, 0.0f, 1.0f, 0, 0u, 0.0f);
            }
            m_renderer->endFrame();
        }

        // Text
        if (!m_guiTextQueue.empty()) {
            glm::mat4 textOrtho = glm::ortho(0.0f, w, 0.0f, h, -1.0f, 1.0f);
            m_textShader->use();
            m_textShader->setMat4("uProjection", textOrtho);
            m_renderer->beginFrame();
            for (const auto& text : m_guiTextQueue) {
                if (text.text.empty()) continue;
                std::string fontPath = text.fontPath.empty() ? "assets/fonts/Roboto/Roboto-Regular.ttf" : text.fontPath;
                auto font = RM.GetFont(fontPath, std::max(1, static_cast<int>(text.pixelSize)));
                if (!font) continue;
                glm::vec2 pos(text.position.X, h - text.position.Y);
                glm::vec4 color(text.color.R, text.color.G, text.color.B, text.color.A);
                m_renderer->submitText(*font, text.text, pos, color, text.pixelSize);
            }
            m_renderer->endFrame();
        }

        Framebuffer::Unbind();
    }

    void RendererSystem::RenderPicking(World& world, Viewport& vp, const glm::mat4& viewProj,
        const std::vector<std::vector<Entity>>& buckets) {
        // Only run if there are pending pick requests
        if (m_pendingPickRequests.empty() && !m_currentPickRequest.has_value() && !m_inFlightPick.has_value())
            return;

        // Process in-flight pick from last frame
        if (m_inFlightPick.has_value()) {
            const int idx = m_inFlightPick->PBOIndex;
            if (idx >= 0 && idx < 2) {
                m_pbos[idx].Bind(GL_PIXEL_PACK_BUFFER);
                void* mapped = m_pbos[idx].Map(GL_READ_ONLY);
                if (mapped) {
                    uint8_t* bytes = static_cast<uint8_t*>(mapped);
                    uint32_t encoded = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16);
                    uint32_t pickedEntity = (encoded == 0) ? INVALID_ENTITY_ID : (encoded - 1);
                    m_completedPickResults[m_inFlightPick->RequestId] = pickedEntity;
                    m_pbos[idx].Unmap();
                }
                m_pbos[idx].Unbind(GL_PIXEL_PACK_BUFFER);
            }
            m_inFlightPick.reset();
        }

        // Dequeue next request
        if (!m_currentPickRequest.has_value() && !m_pendingPickRequests.empty()) {
            m_currentPickRequest = m_pendingPickRequests.front();
            m_pendingPickRequests.pop();
        }

        if (!m_currentPickRequest.has_value()) return;

        // Resize picking FBO if needed
        if (vp.PickingFBO->Width() != vp.Size.x || vp.PickingFBO->Height() != vp.Size.y)
            vp.PickingFBO->Resize(vp.Size.x, vp.Size.y, false, false);

        vp.PickingFBO->BindAndClear(0, 0, 0, 1);
        glViewport(0, 0, vp.Size.x, vp.Size.y);
        glDisable(GL_BLEND);

        // Render entities with ID colors (simplified - just sprites/boxes)
        m_shader->use();
        m_shader->setMat4("uViewProj", viewProj);
        m_shader->setUniform("uPicking", 1);
        m_shader->setUniform("uLightingEnabled", 0);
        m_renderer->beginFrame();

        for (const auto& bucket : buckets) {
            for (Entity entity : bucket) {
                if (world.Has<Components::Active>(entity) && !world.Get<Components::Active>(entity).Enabled) continue;

                uint32_t id = entity.Index + 1;
                glm::vec4 idColor(((id >> 0) & 0xFF) / 255.0f, ((id >> 8) & 0xFF) / 255.0f, ((id >> 16) & 0xFF) / 255.0f, 1.0f);

                const auto& lt = world.Get<Components::LocalTransform>(entity);
                Vector3D position, scale; Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                if (world.Has<Components::SpriteRenderer2D>(entity)) {
                    const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                    float angleZ = std::atan2(2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                        1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z));
                    m_renderer->submitSprite({ ToGlm(Vector2D{position.X, position.Y}), ToGlm(Vector2D{scale.X, scale.Y}),
                        {0,0,1,1}, idColor, sr.TextureId, angleZ, 1.0f, 0, 0.0f });
                }

                if (world.Has<Components::ShapeBox2D>(entity)) {
                    const auto& sb = world.Get<Components::ShapeBox2D>(entity);
                    glm::vec2 he = ToGlm(Vector2D{ sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y });
                    glm::vec2 center = ToGlm(Vector2D{ position.X, position.Y }) + ToGlm(sb.Offset);
                    DebugDraw2D::RectFill(*m_renderer, center - he, center + he, idColor, static_cast<GLuint>(-1));
                }
            }
        }
        m_renderer->endFrame();

        // Read pixel
        int readX = glm::clamp(static_cast<int>(m_currentPickRequest->ScreenX), 0, vp.Size.x - 1);
        int readY = glm::clamp(static_cast<int>(vp.Size.y - m_currentPickRequest->ScreenY - 1), 0, vp.Size.y - 1);

        m_pbos[m_currentPBO].Bind(GL_PIXEL_PACK_BUFFER);
        glReadPixels(readX, readY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        m_pbos[m_currentPBO].Unbind(GL_PIXEL_PACK_BUFFER);

        m_inFlightPick = InFlightPick{ m_currentPickRequest->RequestId, m_currentPBO };
        m_currentPickRequest.reset();
        m_currentPBO = 1 - m_currentPBO;

        glEnable(GL_BLEND);
        Framebuffer::Unbind();
    }

    uint32_t RendererSystem::RequestPick(float screenX, float screenY, const glm::vec2& viewportPos, const glm::vec2& viewportSize) {
        // Check if within viewport bounds
        if (screenX < viewportPos.x || screenX >= (viewportPos.x + viewportSize.x) ||
            screenY < viewportPos.y || screenY >= (viewportPos.y + viewportSize.y)) {
            LOG_DEBUG("[Renderer] RequestPick ignored, screen coordinates out of viewport bounds.");
            return ECS::Entity::NPOS32;
        }

        // Queue the request instead of rejecting it if one is pending
        uint32_t id = m_nextPickRequestId++;
        PendingPickRequest req;
        req.RequestId = id;
        req.ScreenX = screenX;
        req.ScreenY = screenY;
        req.ViewportPos = viewportPos;
        req.ViewportSize = viewportSize;
        m_pendingPickRequests.push(req);

        LOG_DEBUG("[Renderer] RequestPick id=" << id << " screen=(" << screenX << "," << screenY << ") viewport=(" << viewportPos.x << "," << viewportPos.y << "," << viewportSize.x << "," << viewportSize.y << ") - queued");
        return id;
    }

    bool RendererSystem::TryGetPickResult(uint32_t requestId, uint32_t& outEntityId) {
        auto it = m_completedPickResults.find(requestId);
        if (it == m_completedPickResults.end()) return false;
        outEntityId = it->second;
        m_completedPickResults.erase(it);
        return true;
    }

    // ============================================================
    // Wireframe/Debug Rendering API Implementations
    // ============================================================

    void RendererSystem::SubmitWireframeQuad(const glm::vec2& min, const glm::vec2& max,
                                              const glm::vec4& color, float thickness) {
        if (!m_renderer) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Quad;
        sub.vertices = {
            { min.x, min.y },
            { max.x, min.y },
            { max.x, max.y },
            { min.x, max.y }
        };
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = true;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    void RendererSystem::SubmitFilledQuad(const glm::vec2& min, const glm::vec2& max,
                                          const glm::vec4& color) {
        if (!m_renderer) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Quad;
        sub.vertices = {
            { min.x, min.y },
            { max.x, min.y },
            { max.x, max.y },
            { min.x, max.y }
        };
        sub.color = color;
        sub.thickness = 0.0f;
        sub.closed = true;
        sub.filled = true;
        m_wireframeQueue.push_back(sub);
    }

    void RendererSystem::SubmitWireframeCircle(const glm::vec2& center, float radius,
                                                const glm::vec4& color, float thickness) {
        if (!m_renderer) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Circle;
        sub.center = center;
        sub.radius = radius;
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = false;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    void RendererSystem::SubmitWireframePolygon(const glm::vec2* vertices, size_t vertexCount,
                                                 const glm::vec4& color, float thickness, bool closed) {
        if (!m_renderer || !vertices || vertexCount < 2) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Polygon;
        sub.vertices.assign(vertices, vertices + vertexCount);
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = closed;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    void RendererSystem::SubmitWireframeLine(const glm::vec2& p1, const glm::vec2& p2,
                                              const glm::vec4& color, float thickness) {
        if (!m_renderer) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Line;
        sub.vertices = { p1, p2 };
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = false;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    void RendererSystem::SubmitWireframeMesh(const glm::vec2* vertices, size_t vertexCount,
                                              const uint32_t* indices, size_t indexCount,
                                              const glm::vec4& color, float thickness) {
        if (!m_renderer || !vertices || vertexCount < 2) return;

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Mesh;
        sub.vertices.assign(vertices, vertices + vertexCount);
        if (indices && indexCount > 0) {
            sub.indices.assign(indices, indices + indexCount);
        }
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = false;
        sub.filled = false;
        m_wireframeQueue.push_back(sub);
    }

    void RendererSystem::SubmitOverlayQuad(const glm::vec2& center,
                                           const glm::vec2& size,
                                           GLuint textureId,
                                           const glm::vec4& uvRect,
                                           const glm::vec4& color,
                                           float rotation) {
        if (!m_renderer) return;

        OverlayQuadSubmission sub;
        sub.center = center;
        sub.size = size;
        sub.textureId = textureId;
        sub.uvRect = uvRect;
        sub.color = color;
        sub.rotation = rotation;
        m_overlayQuadQueue.push_back(sub);
    }

    void RendererSystem::SubmitGUIPanel(const Vector2D& position, const Vector2D& size,
                                        const Color& color, float cornerRadius) {
        (void)cornerRadius;
        if (!m_renderer) return;

        GUIPanelSubmission submission;
        submission.position = position;
        submission.size = size;
        submission.color = color;
        submission.cornerRadius = cornerRadius;
        m_guiPanelQueue.push_back(submission);
    }

    void RendererSystem::SubmitGUIText(const Vector2D& position, const std::string& text,
                                       const std::string& fontPath, float pixelSize, const Color& color) {
        if (!m_renderer) return;

        GUITextSubmission submission;
        submission.position = position;
        submission.text = text;
        submission.fontPath = fontPath;
        submission.pixelSize = pixelSize;
        submission.color = color;
        m_guiTextQueue.push_back(std::move(submission));
    }

    void RendererSystem::SubmitColliderDebugDraw(ECS::World& world, uint32_t entityID,
        const glm::vec4& color) {
        if (entityID == ECS::Entity::NPOS32) {
            return;
        }

        ECS::Entity entity{ entityID };

        // Get world position/rotation/scale (respect WorldTransform if present)
        Vector3D worldPos{ 0.0f, 0.0f, 0.0f };
        Vector3D scale{ 1.0f, 1.0f, 1.0f };
        Quaternion rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        if (world.Has<ECS::Components::LocalTransform>(entity)) {
            const auto& lt = world.Get<ECS::Components::LocalTransform>(entity);
            GetRenderTransform(world, entity, lt, worldPos, rotation, scale);
        }

        // Precompute 2D position and angle
        const glm::vec2 worldPos2D{ worldPos.X, worldPos.Y };
        const float entityAngleZ = std::atan2(
            2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
            1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
        );

        // Helper: Rotate a 2D vector by radians
        auto rotate2D = [](const glm::vec2& v, float radians) {
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            return glm::vec2(v.x * c - v.y * s, v.x * s + v.y * c);
            };

        // Render 2D Box Collider
        if (world.Has<ECS::Components::BoxCollider2D>(entity)) {
            auto& collider = world.Get<ECS::Components::BoxCollider2D>(entity);

            // Compute box corners
            const glm::vec2 offset{ collider.Offset.X, collider.Offset.Y };
            const glm::vec2 rotatedOffset = rotate2D(offset, entityAngleZ);
            const glm::vec2 center = worldPos2D + rotatedOffset;
            const glm::vec2 halfExtents{ collider.HalfExtents.X * scale.X, collider.HalfExtents.Y * scale.Y };

            // Account for collider rotation
            const float boxAngle = entityAngleZ + collider.Rotation;
            const glm::vec2 right = rotate2D(glm::vec2(1.0f, 0.0f), boxAngle);
            const glm::vec2 up = rotate2D(glm::vec2(0.0f, 1.0f), boxAngle);

            // Define corners
            glm::vec2 corners[4];
            corners[0] = center + right * halfExtents.x + up * halfExtents.y;
            corners[1] = center - right * halfExtents.x + up * halfExtents.y;
            corners[2] = center - right * halfExtents.x - up * halfExtents.y;
            corners[3] = center + right * halfExtents.x - up * halfExtents.y;

            // Submit as polygon
            WireframeSubmission sub;
            sub.type = WireframeSubmission::Type::Polygon;
            sub.vertices.assign(corners, corners + 4);
            sub.color = color;
            sub.thickness = 1.0f;
            sub.closed = true;
            sub.filled = true;
            m_wireframeQueue.push_back(sub);
        }

        // Render 2D Circle Collider - render as polygon for accuracy
        if (world.Has<ECS::Components::CircleCollider2D>(entity)) {
            auto& collider = world.Get<ECS::Components::CircleCollider2D>(entity);

            // Compute circle center and radius
            const glm::vec2 offset{ collider.Offset.X, collider.Offset.Y };
            const glm::vec2 rotatedOffset = rotate2D(offset, entityAngleZ);
            const glm::vec2 center = worldPos2D + rotatedOffset;
            const float radius = collider.Radius * ((scale.X + scale.Y) * 0.5f);

            // Submit as filled circle
            WireframeSubmission sub;
            sub.type = WireframeSubmission::Type::Circle;
            sub.center = center;
            sub.radius = radius;
            sub.color = color;
            sub.thickness = 0.0f;
            sub.closed = false;
            sub.filled = true;
            m_wireframeQueue.push_back(sub);
        }
    }

    void RendererSystem::SetDebugTileMap(const TileMap& map, const Tileset& tileset, const glm::vec2& worldOffset)
    {
        m_debugTileMap = map;
        m_debugTileset = tileset;
        m_debugTileMapOffset = worldOffset;
    }

    void RendererSystem::SetDebugTileMaps(const std::vector<DebugTileMapEntry>& maps)
    {
        m_debugTileMaps = maps; // Store references to editor-owned tilemaps for this frame.
    }

    void RendererSystem::ClearDebugTileMap()
    {
        m_debugTileMap.reset();
        m_debugTileset.reset();
        m_debugTileMapOffset = glm::vec2(0.0f, 0.0f);
    }

    void RendererSystem::ClearDebugTileMaps()
    {
        m_debugTileMaps.clear(); // Clear multi-tilemap debug rendering state.
    }
}
