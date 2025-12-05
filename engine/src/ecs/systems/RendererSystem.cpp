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

// ============================================================================
// ECS Components
// ============================================================================
#include "ecs/Components.h"

// ============================================================================
// Services
// ============================================================================
#include "services/WindowManager.h"
#include "services/Time.h"

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

// ============================================================================
// Third-Party Libraries
// ============================================================================
#include <glm/gtc/matrix_transform.hpp>

namespace ECS {
    static constexpr uint32_t INVALID_ENTITY_ID = Entity::NPOS32;

    RendererSystem* RendererSystem::s_instance = nullptr;

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
        SystemMetadata metadata;
        metadata.Name = "Renderer";
        metadata.ReadComponents = {}; // Reads many components
        metadata.WriteComponents = {};
        metadata.ExecutionOrder = 0;
        return metadata;
    }

    void RendererSystem::OnCreate(World& world) {
        if (m_initialized)
            return;

        m_initialized = true;

        //set static instance pointer
        s_instance = this;

        const auto& mainWindow = WindowManager::GetMainWindow();
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

    glm::vec2 RendererSystem::CalculateAnchoredPosition(
        const Components::LocalTransform& transform,
        Components::TextAnchor anchor,
        float screenWidth,
        float screenHeight,
        float scaleFactor) const
    {
        // Scale the offset based on UI scale
        float scaledOffsetX = transform.Position.X * scaleFactor;
        float scaledOffsetY = transform.Position.Y * scaleFactor;

        switch (anchor) {
        case Components::TextAnchor::TopLeft:
            return glm::vec2(
                scaledOffsetX,
                screenHeight - scaledOffsetY
            );

        case Components::TextAnchor::TopRight:
            return glm::vec2(
                screenWidth - scaledOffsetX,
                screenHeight - scaledOffsetY
            );

        case Components::TextAnchor::BottomLeft:
            return glm::vec2(
                scaledOffsetX,
                scaledOffsetY
            );

        case Components::TextAnchor::BottomRight:
            return glm::vec2(
                screenWidth - scaledOffsetX,
                scaledOffsetY
            );

        case Components::TextAnchor::Center:
            return glm::vec2(
                screenWidth * 0.5f + scaledOffsetX,
                screenHeight * 0.5f + scaledOffsetY
            );

        case Components::TextAnchor::Absolute:
        default:
            // No scaling or anchoring for absolute positioning
            return glm::vec2(transform.Position.X, transform.Position.Y);
        }
    }

    void RendererSystem::OnUpdate(World& world, const float deltaTime) {
        (void)deltaTime;
        if (!m_renderer)
            return;

        // Batching pipeline and render graph
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
        bool foundActive = false;

        // Track current camera zoom for bloom scaling
        m_cameraOrthoSize = kReferenceOrthoSize; // default fallback (world-space 1080p)

        // ============================================================
        // 1. Use provided camera, or find active ECS camera
        // ============================================================
        // If external camera is set (e.g., editor camera), use it
        if (m_activeCamera && !m_forceSceneCamera) {
            view = m_activeCamera->GetViewMatrix();
            projection = m_activeCamera->GetProjectionMatrix();
            foundActive = true;
            m_cameraOrthoSize = m_activeCamera->OrthoSize;
        }
        else {
            // Use ECS camera
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
                    view = glm::lookAt(eye, target, glm::vec3(0.f, 1.f, 0.f));

                    // --- Projection
                    if (camera.UsePerspective) {
                        // camera.FOV assumed radians
                        projection = glm::perspective(
                            camera.FOV,
                            camera.AspectRatio,
                            camera.NearPlane,
                            camera.FarPlane
                        );
                    }
                    else {
                        const float halfH = camera.OrthoSize * 0.5f;
                        const float halfW = halfH * camera.AspectRatio;
                        projection = glm::ortho(
                            -halfW, +halfW,
                            -halfH, +halfH,
                            camera.NearPlane, camera.FarPlane
                        );
                    }

                    foundActive = true; // take the first active camera
                    m_cameraOrthoSize = camera.OrthoSize;
                }
            );
        }

        // fallback (if no active camera found)
        if (!foundActive) {
            const auto& mainWindow = WindowManager::GetMainWindow();
            projection = glm::ortho(0.f, static_cast<float>(mainWindow->GetWidth()),
                0.f, static_cast<float>(mainWindow->GetHeight()),
                -1.f, 1.f);
        }

        // ============================================================
        // BLOOM RADIUS CALCULATION (world-space consistent)
        // ============================================================
        const auto& win = WindowManager::GetMainWindow();
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
                    const auto& list = buckets[layer];

                    // ========== CHECK IF THIS IS THE UI LAYER ==========
                    // UI should use fixed screen-space projection, not camera projection
                    bool isUILayer = false;
                    glm::mat4 layerViewProj = viewProj;  // Default: use camera projection

                    // Check if any entity in this layer is marked as UI
                    // (You can optimize this by caching the UI layer ID)
                    for (const auto& entity : list) {
                        if (world.Has<Components::Layer>(entity)) {
                            // const auto& layerComp = world.Get<Components::Layer>(entity);
                            // Check if this layer is the "ui" layer (we may need to adjust this check)
                            // For now, we'll assume layer IDs > maxLayerId-1 are UI, or check by name
                            // A better way: check if entity has UIButton component
                            if (world.Has<Components::UIButton>(entity)) {
                                isUILayer = true;
                                break;
                            }
                        }
                    }

                    // If this is UI layer, use fixed screen-space projection
                    if (isUILayer) {
                        glm::mat4 uiProjection = glm::ortho(
                            0.0f, static_cast<float>(win->GetWidth()),
                            0.0f, static_cast<float>(win->GetHeight()),
                            -1.0f, 1.0f
                        );
                        layerViewProj = uiProjection;  // No view matrix, just screen-space projection
                    }

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

                        // Polygons
                        if (world.Has<Components::ShapePolygon2D<32>>(entity)) {
                            const auto& pl = world.Get<Components::ShapePolygon2D<32>>(entity);
                            if (pl.Count >= 2) {
                                const auto m = TransformUtils::MakeTRS(position, rotation, scale);
                                polyPoints.clear(); polyPoints.reserve(pl.Count);
                                for (uint32_t i = 0; i < pl.Count; ++i) {
                                    const Vector3D p3{ pl.Points[i].X, pl.Points[i].Y, 0.0f };
                                    const Vector4D hp = m * Vector4D{ p3.X, p3.Y, p3.Z, 1.0f };
                                    polyPoints.push_back(ToGlm(Vector2D{ hp.X, hp.Y }));
                                }
                                DebugDraw2D::Polygon(*m_renderer, polyPoints, ToGlm(pl.FillColor), 0);
                            }
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

                    // ============================================================
                    // --- Sub-pass 3: TEXT RENDERING ---
                    // ============================================================
                    if (m_textShader) {
                        m_textShader->use();

                        const float screenWidth = static_cast<float>(win->GetWidth());
                        const float screenHeight = static_cast<float>(win->GetHeight());

                        // Calculate UI scale factor (simple calculation each frame)
                        const float uiScaleFactor = screenHeight / kReferenceHeight;

                        // Screen-space orthographic projection
                        glm::mat4 screenOrtho = glm::ortho(
                            0.0f, screenWidth,
                            0.0f, screenHeight,
                            -1.0f, 1.0f
                        );
                        m_textShader->setMat4("uProjection", screenOrtho);
                        m_renderer->beginFrame();

                        // Font cache (static to persist across frames)
                        static std::unordered_map<std::string, std::shared_ptr<Font>> fontCache;

                        for (ECS::Entity entity : list) {
                            // Skip inactive entities
                            if (world.Has<Components::Active>(entity) &&
                                !world.Get<Components::Active>(entity).Enabled) continue;

                            // Only process entities with Text component
                            if (!world.Has<Components::Text>(entity)) continue;

                            // Get transform and text data
                            const auto& lt = world.Get<Components::LocalTransform>(entity);
                            const auto& text = world.Get<Components::Text>(entity); // const

                            // Load/cache font
                            std::string fontPath(text.FontPath);
                            auto it = fontCache.find(fontPath);
                            if (it == fontCache.end()) {
                                try {
                                    auto font = std::make_shared<Font>(fontPath, 96);
                                    fontCache[fontPath] = font;
                                    it = fontCache.find(fontPath);
                                    LOG_DEBUG("Loaded font: " << fontPath);
                                }
                                catch (const std::exception& e) {
                                    LOG_ERROR("Failed to load font " << fontPath << ": " << e.what());
                                    continue;
                                }
                            }

                            // Calculate position fresh every frame (it's just a few multiplications!)
                            glm::vec2 screenPos = CalculateAnchoredPosition(
                                lt, text.Anchor, screenWidth, screenHeight, uiScaleFactor
                            );

                            // Scale font size proportionally (renderer handles the rest)
                            float scaledFontSize = text.PixelSize * uiScaleFactor;

                            // Submit text to batcher since the quad scaling happens automatically
                            m_renderer->submitText(
                                *it->second,
                                text.getContent(),
                                screenPos,
                                ToGlm(text.Color),
                                scaledFontSize
                            );
                        }

                        m_renderer->endFrame(); // Flush text batch
                    } //if m_textShader

                }
                Framebuffer::Unbind();
            });

        // Object Picking Pass
        m_renderGraph->AddPass("Picking", {}, {},
            [this, &world, &viewProj, &buckets, &win](ResourceAccessor& res)
            {
                // Skip picking entirely when force scene camera is enabled
                if (m_forceSceneCamera) {
                    LOG_DEBUG("[PICKING] Skipping picking - force scene camera enabled");
                    return;
                }

                static bool prevMouseDown = false;
                bool currMouseDown = Input::IsMouseDown(MOUSE_LEFT);
                bool mouseJustReleased = (!currMouseDown && prevMouseDown);
                prevMouseDown = currMouseDown;

                (void)res;
                if (!currMouseDown && !mouseJustReleased) return;

                // ============================================================
                // GET VIEWPORT BOUNDS
                // ============================================================
                glm::vec2 viewportMin(0, 0);
                glm::vec2 viewportSize = glm::vec2(win->GetWidth(), win->GetHeight());

                glm::dvec2 mousePos;
                Input::GetMousePosition(mousePos.x, mousePos.y);

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
                glm::vec2 localMouse = glm::vec2(mousePos) - viewportMin;

                int x = static_cast<int>(localMouse.x);
                int y = static_cast<int>(viewportSize.y - localMouse.y);

                x = glm::clamp(x, 0, vpWidth - 1);
                y = glm::clamp(y, 0, vpHeight - 1);

                LOG_DEBUG("[PICKING] FBO size: " << vpWidth << "x" << vpHeight);
                LOG_DEBUG("[PICKING] Reading pixel: (" << x << ", " << y << ")");


                // Frame N: Write to PBO 0
                // Write to current PBO (async transfer starts)
                m_pbos[m_currentPBO].Bind(GL_PIXEL_PACK_BUFFER);
                glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
                m_pbos[m_currentPBO].Unbind(GL_PIXEL_PACK_BUFFER);

                // Frame N: Swap for next frame
                // Swap PBOs FIRST
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
        if (Time::FrameCount() % 120 == 0)
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

            LOG_DEBUG("Flushes this frame: " << flushes << ss.str() << " | " << "FPS: " <<  static_cast<int>(1.0f / Time::DeltaTime()));
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
        s_instance = nullptr;
    }
}
