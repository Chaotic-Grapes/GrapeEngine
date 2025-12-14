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

// ============================================================================
// ECS Components
// ============================================================================
#include "ecs/Components.h"

// ============================================================================
// Services
// ============================================================================
#include "services/TimeSystem.h"
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

        // Shaders
        m_shader = std::make_unique<Shader>(
            "assets/shaders/batch.vert",
            "assets/shaders/batch.frag");

        m_textShader = std::make_unique<Shader>(
            "assets/shaders/sdf_text.vert",
            "assets/shaders/sdf_text.frag");

        m_sdfCircleShader = std::make_unique<Shader>(
            "assets/shaders/sdf_circle.vert",
            "assets/shaders/sdf_circle.frag"
        );

        m_bloomExtractShader = std::make_unique<Shader>(
            "assets/shaders/bloom_extract.vert",
            "assets/shaders/bloom_extract.frag");

        m_bloomBlurShader = std::make_unique<Shader>(
            "assets/shaders/bloom_extract.vert",
            "assets/shaders/bloom_blur.frag");

        m_bloomCombineShader = std::make_unique<Shader>(
            "assets/shaders/bloom_extract.vert",
            "assets/shaders/bloom_combine.frag");

        m_blitShader = std::make_unique<Shader>(
            "assets/shaders/blit.vert",
            "assets/shaders/blit.frag");

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

    void RendererSystem::OnUpdate(World& world, const float deltaTime) {
        (void)deltaTime;
        if (!m_renderer)
            return;

        // Batching pipeline and render graph
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
        m_cameraOrthoSize = kReferenceOrthoSize; // default fallback (world-space 1080p)

        // ============================================================
        // 1. Acquire camera matrices from active source (camera agnostic)
        // ============================================================
        GetCameraMatrices(world, view, projection, m_cameraOrthoSize);

        // ============================================================
        // BLOOM RADIUS CALCULATION (world-space consistent)
        // ============================================================
        auto* context = Engine::CORE->GetPlatformContext();
        auto* win = context ? context->GetMainWindow() : nullptr;
        if (!win) return;
        const float bloomBufferHeight = static_cast<float>(win->GetHeight()) / 2.0f;

        // How zoomed in we are relative to the default ortho size
        const float zoomScale = kReferenceOrthoSize / m_cameraOrthoSize;

        // Convert desired world-space bloom spread to texel space
        const float bloomRadiusTexels =
            (kDesiredBloomWorldSpread / kReferenceOrthoSize) *
            bloomBufferHeight * zoomScale;

        // We only send (Projection * View) here instead of the full (Projection * View * Model)
        // because each sprite/shape's model transform (position, rotation, scale) is already
        // baked into its vertex positions on the CPU during batching. By the time vertices
        // reach the GPU, they are in world space, so the shader only needs to transform them
        // into camera (view) space and then into clip space.
        // I will remind myself to change this in the future
        const glm::mat4 viewProj = projection * view;

        // Determine max layer id present this frame
        int maxLayerId = -1;
        world.Each<Components::Layer>([&](ECS::Entity, const Components::Layer& ly) {
            maxLayerId = std::max(static_cast<int>(ly.Id), maxLayerId);
            });

        // Render per-layer from back (0) to front (max)
        // Single-pass collection: bucket entities by layer then process each
        std::vector<std::vector<ECS::Entity>> buckets;
        buckets.resize(std::max(1, maxLayerId + 1));

        // Collect entities that have LocalTransform + Layer
        world.Each<Components::LocalTransform, Components::Layer>([&](const ECS::Entity entity, Components::LocalTransform& lt, const Components::Layer& ly){
            (void)lt;
        	const uint16_t lid = ly.Id;
            if (lid < buckets.size())
                buckets[lid].push_back(entity);
            });

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

                // ---------------------------------------
                // Layered rendering: SDF first, then batch
                // ---------------------------------------
                for (int layer = 0; layer <= maxLayerId; ++layer) {
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
                    m_renderer->beginFrame();

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
                            DebugDraw2D::Line(*m_renderer,
                                ToGlm(Vector2D{ position.X, position.Y } + sl.A),
                                ToGlm(Vector2D{ position.X, position.Y } + sl.B),
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
                            // Tiling controls how much of the texture is shown (1.0 = full texture)
                            // Offset controls where in the texture to start sampling (0.0 = top-left)
                            const float u0 = sr.Offset.X;
                            const float v0 = sr.Offset.Y;
                            const float u1 = sr.Offset.X + sr.Tiling.X;
                            const float v1 = sr.Offset.Y + sr.Tiling.Y;
                            
                            m_renderer->submitSprite({
                                ToGlm(Vector2D{position.X, position.Y}),
                                ToGlm(Vector2D{scale.X, scale.Y}),
                                {u0, v0, u1, v1},
                                ToGlm(sr.Color),
                                sr.TextureId,
                                angleZ,
                                1.0f,
                                sr.EmissiveTextureId,
                                sr.EmissiveStrength
                                });
                        }
                    }

                    m_renderer->endFrame(); // flush non-SDF for this layer

                    // Render queued wireframe submissions (debug/editor outlines)
                    if (!m_wireframeQueue.empty()) {
                        m_renderer->beginFrame();
                        for (const auto& sub : m_wireframeQueue) {
                            switch (sub.type) {
                            case WireframeSubmission::Type::Quad: {
                                if (sub.vertices.size() == 4) {
                                    const auto& min = sub.vertices[0];
                                    const auto& max = sub.vertices[2];
                                    DebugDraw2D::RectStroke(*m_renderer, min, max, sub.thickness, sub.color, 0);
                                }
                                break;
                            }
                            case WireframeSubmission::Type::Circle: {
                                if (sub.vertices.size() >= 2) {
                                    for (size_t i = 0; i < sub.vertices.size(); ++i) {
                                        size_t next = sub.closed ? (i + 1) % sub.vertices.size() : i + 1;
                                        if (next < sub.vertices.size()) {
                                            DebugDraw2D::Line(*m_renderer, sub.vertices[i], sub.vertices[next],
                                                sub.thickness, sub.color, 0);
                                        }
                                    }
                                }
                                break;
                            }
                            case WireframeSubmission::Type::Polygon: {
                                if (sub.vertices.size() >= 2) {
                                    for (size_t i = 0; i < sub.vertices.size(); ++i) {
                                        size_t next = sub.closed ? (i + 1) % sub.vertices.size() : i + 1;
                                        if (next < sub.vertices.size()) {
                                            DebugDraw2D::Line(*m_renderer, sub.vertices[i], sub.vertices[next],
                                                sub.thickness, sub.color, 0);
                                        }
                                    }
                                }
                                break;
                            }
                            case WireframeSubmission::Type::Line: {
                                if (sub.vertices.size() == 2) {
                                    DebugDraw2D::Line(*m_renderer, sub.vertices[0], sub.vertices[1],
                                        sub.thickness, sub.color, 0);
                                }
                                break;
                            }
                            case WireframeSubmission::Type::Mesh: {
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
                                } else {
                                    // Draw as sequence of lines
                                    for (size_t i = 0; i + 1 < sub.vertices.size(); i += 2) {
                                        DebugDraw2D::Line(*m_renderer, sub.vertices[i], sub.vertices[i + 1],
                                            sub.thickness, sub.color, 0);
                                    }
                                }
                                break;
                            }
                            }
                        }
                        m_renderer->endFrame();
                        m_wireframeQueue.clear(); // Clear queue for next frame
                    }

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

                static bool prevMouseDown = false;
                bool currMouseDown = Input::IsMouseDown(MOUSE_LEFT);
                bool mouseJustReleased = (!currMouseDown && prevMouseDown);
                prevMouseDown = currMouseDown;

                (void)res;
                // Allow the picking pass to run if there is a pending async request
                if (!currMouseDown && !mouseJustReleased && !m_pendingPickRequest.has_value()) return;

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

                // If there is a pending async pick request, prefer its viewport
                // rectangle for coordinate mapping (it contains viewportPos/Size).
                bool usingPendingRequestForViewport = false;
                if (m_pendingPickRequest.has_value()) {
                    viewportMin = m_pendingPickRequest->ViewportPos;
                    viewportSize = m_pendingPickRequest->ViewportSize;
                    usingPendingRequestForViewport = true;
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
                // Determine which screen coordinates to sample. If an async
                // pick request exists, use its provided coordinates. Otherwise
                // use the current mouse position (interactive click).
                glm::vec2 sampleScreenPos;
                bool usingPendingRequest = false;
                if (m_pendingPickRequest.has_value()) {
                    sampleScreenPos = glm::vec2(m_pendingPickRequest->ScreenX, m_pendingPickRequest->ScreenY);
                    usingPendingRequest = true;
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
                if (usingPendingRequest && m_pendingPickRequest.has_value()) {
                    LOG_DEBUG("[PICKING] Servicing async request " << m_pendingPickRequest->RequestId);
                }

                // Frame N: Write to current PBO (async transfer starts)
                m_pbos[m_currentPBO].Bind(GL_PIXEL_PACK_BUFFER);
                glReadPixels(readX, readY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
                m_pbos[m_currentPBO].Unbind(GL_PIXEL_PACK_BUFFER);

                // If this read corresponds to a pending async pick request,
                // mark it as in-flight and associate it with the current PBO
                // so the result can be consumed on the next frame.
                if (usingPendingRequest) {
                    m_inFlightPick = InFlightPick{ m_pendingPickRequest->RequestId, m_currentPBO };
                    m_pendingPickRequest.reset();
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

        // GUI Rendering Pass - Render queued GUI elements on top of scene
        m_renderGraph->AddPass("GUI", { "LDR" }, { "LDR" },
            [this](ResourceAccessor& res)
            {
                auto* ldr = res.GetFramebuffer("LDR");
                if (!ldr) return;

                // Bind LDR for reading/writing
                ldr->Bind();

                // Enable blending for GUI elements
                GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                // Use batch shader for GUI panels
                if (m_shader) {
                    m_shader->use();
                    glm::mat4 screenOrtho = glm::ortho(0.0f, 1920.0f, 0.0f, 1080.0f, -1.0f, 1.0f);
                    m_shader->setMat4("uViewProj", screenOrtho);
                }

                m_renderer->beginFrame();

                // Process all queued GUI submissions
                ProcessGUISubmissions();

                m_renderer->endFrame();

                // Restore blend state
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
    }

    uint32_t RendererSystem::RequestPick(float screenX, float screenY, const glm::vec2& viewportPos, const glm::vec2& viewportSize) {
        uint32_t id = m_nextPickRequestId++;
        PendingPickRequest req;
        req.RequestId = id;
        req.ScreenX = screenX;
        req.ScreenY = screenY;
        req.ViewportPos = viewportPos;
        req.ViewportSize = viewportSize;
        m_pendingPickRequest = req;
        LOG_DEBUG("[Renderer] RequestPick id=" << id << " screen=(" << screenX << "," << screenY << ") viewport=(" << viewportPos.x << "," << viewportPos.y << "," << viewportSize.x << "," << viewportSize.y << ")");
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
        m_wireframeQueue.push_back(sub);
    }

    void RendererSystem::SubmitWireframeCircle(const glm::vec2& center, float radius,
                                                const glm::vec4& color, float thickness) {
        if (!m_renderer) return;

        // Tessellate circle into line segments
        constexpr int segments = 64;
        std::vector<glm::vec2> verts;
        verts.reserve(segments);

        for (int i = 0; i < segments; ++i) {
            float angle = (2.0f * 3.14159265f * i) / segments;
            glm::vec2 pt = center + glm::vec2(cosf(angle), sinf(angle)) * radius;
            verts.push_back(pt);
        }

        WireframeSubmission sub;
        sub.type = WireframeSubmission::Type::Circle;
        sub.vertices = std::move(verts);
        sub.color = color;
        sub.thickness = thickness;
        sub.closed = true;
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
        m_wireframeQueue.push_back(sub);
    }

    // ========================================================================
    // GUI Rendering APIs
    // ========================================================================

    void RendererSystem::SubmitGUIPanel(const Vector2D& position, const Vector2D& size,
                                       const Color& color, float cornerRadius) {
        if (!m_renderer) return;

        GUISubmission submission;
        submission.type = GUISubmission::Type::Panel;
        submission.position = position;
        submission.size = size;
        submission.color = color;
        submission.cornerRadius = cornerRadius;

        m_guiSubmissionQueue.push_back(submission);
    }

    void RendererSystem::SubmitGUIText(const std::string& fontPath, const std::string& text,
                                      const Vector2D& position, const Color& color,
                                      float fontSize, const Color& shadowColor,
                                      const Vector2D& shadowOffset) {
        if (!m_renderer) return;

        GUISubmission submission;
        submission.type = GUISubmission::Type::Text;
        submission.fontPath = fontPath;
        submission.text = text;
        submission.position = position;
        submission.color = color;
        submission.fontSize = fontSize;
        submission.shadowColor = shadowColor;
        submission.shadowOffset = shadowOffset;

        m_guiSubmissionQueue.push_back(submission);
    }

    void RendererSystem::SubmitGUISlider(const Vector2D& position, const Vector2D& size,
                                        float value, const Color& backgroundColor,
                                        const Color& handleColor, const Color& borderColor,
                                        float borderRadius) {
        if (!m_renderer) return;

        GUISubmission submission;
        submission.type = GUISubmission::Type::Slider;
        submission.position = position;
        submission.size = size;
        submission.value = value;
        submission.color = backgroundColor;
        submission.secondaryColor = handleColor;
        submission.borderColor = borderColor;
        submission.cornerRadius = borderRadius;

        m_guiSubmissionQueue.push_back(submission);
    }

    void RendererSystem::SubmitGUICheckbox(const Vector2D& position, const Vector2D& size,
                                          bool checked, const Color& boxColor,
                                          const Color& checkColor, const Color& borderColor) {
        if (!m_renderer) return;

        GUISubmission submission;
        submission.type = GUISubmission::Type::Checkbox;
        submission.position = position;
        submission.size = size;
        submission.checked = checked;
        submission.color = boxColor;
        submission.secondaryColor = checkColor;
        submission.borderColor = borderColor;

        m_guiSubmissionQueue.push_back(submission);
    }

    void RendererSystem::SubmitGUILine(const Vector2D& startPos, const Vector2D& endPos,
                                      const Color& color, float thickness) {
        if (!m_renderer) return;

        GUISubmission submission;
        submission.type = GUISubmission::Type::Line;
        submission.startPos = startPos;
        submission.endPos = endPos;
        submission.color = color;
        submission.thickness = thickness;

        m_guiSubmissionQueue.push_back(submission);
    }

    // ========================================================================
    // Internal: Process GUI Submissions
    // ========================================================================

    void RendererSystem::ProcessGUISubmissions() {
        if (!m_renderer) return;

        for (const auto& submission : m_guiSubmissionQueue) {
            if (submission.type == GUISubmission::Type::Panel) {
                ProcessGUIPanel(submission);
            } else if (submission.type == GUISubmission::Type::Text) {
                ProcessGUIText(submission);
            } else if (submission.type == GUISubmission::Type::Slider) {
                ProcessGUISlider(submission);
            } else if (submission.type == GUISubmission::Type::Checkbox) {
                ProcessGUICheckbox(submission);
            } else if (submission.type == GUISubmission::Type::Line) {
                ProcessGUILine(submission);
            }
        }

        m_guiSubmissionQueue.clear();
    }

    void RendererSystem::ProcessGUIPanel(const GUISubmission& submission) {
        // Convert engine types to GLM
        glm::vec2 glmPos(submission.position.X, submission.position.Y);
        glm::vec2 glmSize(submission.size.X, submission.size.Y);
        glm::vec4 glmColor(submission.color.R / 255.0f, submission.color.G / 255.0f,
                          submission.color.B / 255.0f, submission.color.A / 255.0f);

        // Submit quad representing the panel (no texture)
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        GLuint textureId = 0; // no texture (solid color)
        m_renderer->submitQuad(glmPos, glmSize, textureId, uvRect, glmColor, 0.0f, 1.0f, 0, 0u, 0.0f);
    }

    void RendererSystem::ProcessGUIText(const GUISubmission& submission) {
        if (!m_textShader || !m_renderer) return;

        // Font cache (static to persist across frames)
        static std::unordered_map<std::string, std::shared_ptr<Font>> fontCache;

        // Load/cache font
        std::string fontPath = submission.fontPath;
        auto it = fontCache.find(fontPath);
        if (it == fontCache.end()) {
            try {
                auto font = std::make_shared<Font>(fontPath, 96);
                fontCache[fontPath] = font;
                it = fontCache.find(fontPath);
                LOG_DEBUG("Loaded font for GUI: " << fontPath);
            }
            catch (const std::exception& e) {
                LOG_ERROR("Failed to load GUI font " << fontPath << ": " << e.what());
                return;
            }
        }

        // Use text shader for SDF rendering
        m_textShader->use();

        // Convert color from 0-255 to 0.0-1.0
        glm::vec4 glmColor(
            submission.color.R / 255.0f,
            submission.color.G / 255.0f,
            submission.color.B / 255.0f,
            submission.color.A / 255.0f
        );

        // Screen-space position
        glm::vec2 screenPos(submission.position.X, submission.position.Y);

        // Submit text to renderer for SDF rendering
        // The renderer's submitText handles font geometry generation and batching
        m_renderer->submitText(
            *it->second,
            submission.text,
            screenPos,
            glmColor,
            submission.fontSize
        );
    }

    void RendererSystem::ProcessGUISlider(const GUISubmission& submission) {
        if (!m_renderer) return;

        // Slider consists of: background track + handle
        glm::vec2 glmPos(submission.position.X, submission.position.Y);
        glm::vec2 glmSize(submission.size.X, submission.size.Y);
        glm::vec4 bgColor(submission.color.R / 255.0f, submission.color.G / 255.0f,
                         submission.color.B / 255.0f, submission.color.A / 255.0f);
        glm::vec4 handleColor(submission.secondaryColor.R / 255.0f,
                             submission.secondaryColor.G / 255.0f,
                             submission.secondaryColor.B / 255.0f,
                             submission.secondaryColor.A / 255.0f);

        // Draw background track (full width, small height)
        float trackHeight = glmSize.y * 0.3f; // Track is 30% of slider height
        glm::vec2 trackPos = glmPos + glm::vec2(0, (glmSize.y - trackHeight) * 0.5f);
        glm::vec2 trackSize(glmSize.x, trackHeight);

        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        GLuint textureId = 0;
        m_renderer->submitQuad(trackPos, trackSize, textureId, uvRect, bgColor, 0.0f, 1.0f, 0, 0u, 0.0f);

        // Draw handle (circle/rounded rect at value position)
        float handleWidth = glmSize.y * 0.8f; // Handle width = slider height * 0.8
        float handleX = glmPos.x + (glmSize.x - handleWidth) * submission.value;
        glm::vec2 handlePos(handleX, glmPos.y);
        glm::vec2 handleSize(handleWidth, glmSize.y);

        m_renderer->submitQuad(handlePos, handleSize, textureId, uvRect, handleColor, 0.0f, 1.0f, 0, 0u, 0.0f);
    }

    void RendererSystem::ProcessGUICheckbox(const GUISubmission& submission) {
        if (!m_renderer) return;

        // Checkbox consists of: box outline + checkmark if checked
        glm::vec2 glmPos(submission.position.X, submission.position.Y);
        glm::vec2 glmSize(submission.size.X, submission.size.Y);
        glm::vec4 boxColor(submission.color.R / 255.0f, submission.color.G / 255.0f,
                          submission.color.B / 255.0f, submission.color.A / 255.0f);
        glm::vec4 checkColor(submission.secondaryColor.R / 255.0f,
                            submission.secondaryColor.G / 255.0f,
                            submission.secondaryColor.B / 255.0f,
                            submission.secondaryColor.A / 255.0f);

        // Draw checkbox box
        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        GLuint textureId = 0;
        m_renderer->submitQuad(glmPos, glmSize, textureId, uvRect, boxColor, 0.0f, 1.0f, 0, 0u, 0.0f);

        // Draw checkmark if checked (as a simple X shape with two diagonal lines)
        if (submission.checked) {
            // Simplified: submit small quad in center as checkmark indicator
            float inset = glmSize.x * 0.2f;
            glm::vec2 checkPos = glmPos + glm::vec2(inset, inset);
            glm::vec2 checkSize = glmSize - glm::vec2(inset * 2.0f, inset * 2.0f);
            m_renderer->submitQuad(checkPos, checkSize, textureId, uvRect, checkColor, 0.0f, 1.0f, 0, 0u, 0.0f);
        }
    }

    void RendererSystem::ProcessGUILine(const GUISubmission& submission) {
        if (!m_renderer) return;

        // Convert engine types to GLM
        glm::vec2 start(submission.startPos.X, submission.startPos.Y);
        glm::vec2 end(submission.endPos.X, submission.endPos.Y);
        glm::vec4 color(submission.color.R / 255.0f, submission.color.G / 255.0f,
                       submission.color.B / 255.0f, submission.color.A / 255.0f);

        // Calculate line direction and perpendicular
        glm::vec2 direction = glm::normalize(end - start);
        glm::vec2 perpendicular(-direction.y, direction.x); // Rotate 90 degrees

        // Create a quad that represents the line (like a thick line)
        glm::vec2 halfThickness = perpendicular * (submission.thickness * 0.5f);

        // Calculate the four corners of the line quad
        glm::vec2 p1 = start - halfThickness;
        glm::vec2 p2 = start + halfThickness;
        glm::vec2 p3 = end + halfThickness;
        glm::vec2 p4 = end - halfThickness;

        // Submit as a quad (using average position and approximate size)
        glm::vec2 center = (start + end) * 0.5f;
        float lineLength = glm::distance(start, end);
        glm::vec2 size(lineLength, submission.thickness);

        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        GLuint textureId = 0;
        m_renderer->submitQuad(center - size * 0.5f, size, textureId, uvRect, color, 0.0f, 1.0f, 0, 0u, 0.0f);
    }
}
