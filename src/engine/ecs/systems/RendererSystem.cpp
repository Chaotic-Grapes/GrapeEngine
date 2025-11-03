/* Start Header *****************************************************************/
/*!
\file    RendererSystem.cpp
\authors Muhammad Nur Fadzly Bin Zulkifli (75%), Choi Meng Yew (25%)
\par     muhammadnurfadzly.b@digipen.edu, choi.m@digipen.edu
\date    20th October 2025
\brief
Implements the RendererSystem.

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

    void RendererSystem::Initialize(World& world) {
        if (m_initialized)
            return;

        m_initialized = true;

        const auto& mainWindow = WindowManager::GetMainWindow();
        const int width = mainWindow->Width();
        const int height = mainWindow->Height();

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

        // Object Picking
        m_pickingFBO.Create(width, height, false, false, 1);
        m_pbos[0].Create(4, GL_STREAM_READ);
        m_pbos[1].Create(4, GL_STREAM_READ);

        // Renderer
        m_renderer = std::make_unique<Renderer>(15000);

        // --- Editor Camera ---
        m_editorCamera = std::make_unique<Engine::EditorCamera>(world);

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


        // Resize HDR when window resizes
        Messaging::MessageSystem::Subscribe<Messaging::WindowResized>(
            [this](const Messaging::WindowResized& msg)
            {
                // TODO: Add RenderGraph::ResizeTexture() method to handle this
                // For now, recreate the graph on resize
                m_renderGraph = std::make_unique<RenderGraph>();

                m_renderGraph->CreateTexture("HDR",          { msg.Width,      msg.Height,      GL_RGBA16F, false });
                m_renderGraph->CreateTexture("Backbuffer",   { msg.Width,      msg.Height,      GL_RGBA8,   true });
                m_renderGraph->CreateTexture("BloomExtract", { msg.Width / 2,  msg.Height / 2,  GL_RGBA16F, false });
                m_renderGraph->CreateTexture("BloomBlur",    { msg.Width / 2,  msg.Height / 2,  GL_RGBA16F, false });

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

    void RendererSystem::Update(World& world, const float dt) {
        (void)dt;
        if (!m_renderer)
            return;

        // Batching pipeline and render graph
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
        bool foundActive = false;

        // Track current camera zoom for bloom scaling
        m_cameraOrthoSize = kReferenceOrthoSize; // default fallback (world-space 1080p)

        // ============================================================
        // 1. Toggle or cycle camera
        // ============================================================
        if (Input::IsKeyPressed(KEY_C)) {
            m_useEditorCamera = !m_useEditorCamera;
            std::cout << "[RendererSystem] "
                << (m_useEditorCamera ? "Using Editor Camera" : "Using Scene Camera")
                << std::endl;

            if (m_editorCamera) {
                m_editorCamera->GetCameraComponent()->Active = m_useEditorCamera;
            }
        }

        // ============================================================
        // 2. Use EditorCamera if active, otherwise ECS camera
        // ============================================================
        if (m_useEditorCamera && m_editorCamera) {
            m_editorCamera->Update(Time::DeltaTime());
            view = m_editorCamera->GetViewMatrix();
            projection = m_editorCamera->GetProjectionMatrix();
            foundActive = true;
            m_cameraOrthoSize = m_editorCamera->GetCameraComponent()->OrthoSize;
        }
        else {
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
            projection = glm::ortho(0.f, float(mainWindow->Width()),
                0.f, float(mainWindow->Height()),
                -1.f, 1.f);
        }

        // ============================================================
        // BLOOM RADIUS CALCULATION (world-space consistent)
        // ============================================================
        const auto& win = WindowManager::GetMainWindow();
        const float bloomBufferHeight = static_cast<float>(win->Height()) / 2.0f;

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
            [this, &world, &viewProj, &maxLayerId, &buckets, &transformedCorners, &polyPoints](ResourceAccessor& res)
            {
                // Get HDR framebuffer from render graph
                auto* hdrFbo = res.GetFramebuffer("HDR");
                if (!hdrFbo) {
                    std::cerr << "ERROR: HDR framebuffer not found!\n";
                    return;
                }

                // Bceause of tone-mapping, the background will appear slightly lighter.
                // tone mapping remaps linear HDR gray (0.1) into gamma-corrected space (looks brighter)
                hdrFbo->BindAndClear(0.018f, 0.018f, 0.019f, 1.0f);

                // ---------------------------------------
                // Layered rendering: SDF first, then batch
                // ---------------------------------------
                for (int layer = 0; layer <= maxLayerId; ++layer) {
                    if (layer >= (int)buckets.size()) continue;
                    const auto& list = buckets[layer];

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
                            m_renderer->submitSprite({
                                ToGlm(Vector2D{position.X, position.Y}),
                                ToGlm(Vector2D{scale.X, scale.Y}),
                                {0.f, 0.f, 1.f, 1.f},
                                ToGlm(sr.Color),
                                sr.TextureId,
                                angleZ,
                                1.0f
                                });
                        }
                    }

                    m_renderer->endFrame(); // flush non-SDF for this layer

                    // ============================================================
                    // --- Sub-pass 3: TEXT RENDERING ---
                    // ============================================================
                    if (m_textShader) {
                        m_textShader->use();

                        const auto& win = WindowManager::GetMainWindow();
                        const float screenWidth = static_cast<float>(win->Width());
                        const float screenHeight = static_cast<float>(win->Height());

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
                            const auto& text = world.Get<Components::Text>(entity);  // Now const!

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

                    // ============================================================
                    // --- DEBUG: Draw Non-Editor Camera Frustum ---
                    // ============================================================
                    if (m_useEditorCamera) { // Only show when using editor camera
                        m_shader->use();
                        m_shader->setMat4("uViewProj", viewProj);
                        m_renderer->beginFrame();

                        // Get editor camera entity to exclude it
                        const ECS::Entity editorCameraEntity = m_editorCamera->GetEntity();

                        // Find the active non-editor camera
                        world.Each<ECS::Components::LocalTransform, ECS::Components::Camera3D>(
                            [&](ECS::Entity entity,
                                const ECS::Components::LocalTransform& camTransform,
                                const ECS::Components::Camera3D& camera)
                            {
                                // Skip editor camera itself!!!
                                if (entity == editorCameraEntity) return;

                                if (!camera.Active) return; // Skip inactive cameras

                                // Calculate camera frustum bounds in world space
                                const float halfH = camera.OrthoSize * 0.5f;
                                const float halfW = halfH * camera.AspectRatio;

                                // Camera position in world space
                                const glm::vec2 camPos(camTransform.Position.X, camTransform.Position.Y);

                                // Frustum corners (centered on camera position)
                                const glm::vec2 frustumMin = camPos - glm::vec2(halfW, halfH);
                                const glm::vec2 frustumMax = camPos + glm::vec2(halfW, halfH);

                                // Calculate constant screen-space thickness
                                const auto& win = WindowManager::GetMainWindow();
                                const float desiredPixelThickness = 2.0f; // Always 2 pixels thick
                                const float worldThickness = (m_cameraOrthoSize / win->Height()) * desiredPixelThickness;

                                // Draw frustum rectangle with constant screen-space thickness
                                const glm::vec4 frustumColor(0.0f, 1.0f, 1.0f, 0.6f); // Cyan, semi-transparent

                                DebugDraw2D::RectStroke(*m_renderer, frustumMin, frustumMax,
                                    worldThickness, frustumColor, 0);
                            }
                        );

                        m_renderer->endFrame();
                    } // if m_useEditorCamera

                }
                Framebuffer::Unbind();
            });

        // Object Picking Pass
        m_renderGraph->AddPass("Picking", {}, {},
            [this, &world, &viewProj, &buckets](ResourceAccessor& res)
            {
                // single-click detection
                if (!Input::IsMousePressed(MOUSE_LEFT)) return;

                m_pickingFBO.BindAndClear(0, 0, 0, 1);

                // Disable blending for picking to prevent ID color contamination
                GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
                if (blendWasEnabled) glDisable(GL_BLEND);

                // ============================================================
                // Pass 1: Render circles with SDF shader
                // ============================================================
                m_sdfCircleShader->use();
                m_sdfCircleShader->setMat4("uViewProj", viewProj);
                m_sdfCircleShader->setUniform("uPicking", 1);
                m_renderer->beginFrame();

                for (int layer = 0; layer <= (int)buckets.size() - 1; ++layer) {
                    const auto& list = buckets[layer];

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (world.Has<Components::Active>(entity) &&
                            !world.Get<Components::Active>(entity).Enabled) continue;

                        // Only render circles in this pass
                        if (!world.Has<Components::ShapeCircle2D>(entity)) continue;

                        // Encode entity ID as RGB
                        uint32_t id = entity.Index;
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

                for (int layer = 0; layer <= (int)buckets.size() - 1; ++layer) {
                    const auto& list = buckets[layer];

                    for (ECS::Entity entity : list) {
                        // Skip inactive
                        if (world.Has<Components::Active>(entity) &&
                            !world.Get<Components::Active>(entity).Enabled) continue;

                        // Skip circles (already rendered above)
                        if (world.Has<Components::ShapeCircle2D>(entity)) continue;

                        // Encode entity ID as RGB
                        uint32_t id = entity.Index;
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
                            DebugDraw2D::RectFill(*m_renderer, min, max, idColor, -1);
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
                                1.0f
                                });
                        }
                    }
                }

                m_renderer->endFrame();

                // Read pixel at mouse position (async)
                glm::dvec2 mousePos;
                Input::GetMousePosition(mousePos.x, mousePos.y);

                int x = static_cast<int>(mousePos.x);
                int y = m_pickingFBO.height - static_cast<int>(mousePos.y); // Flip Y

                static bool firstFrame = true;
                if (!firstFrame) {
                    // Frame N: Read from PBO 1 (contains data from frame N-1)
                    int readPBO = 1 - m_currentPBO;
                    uint32_t pickedID = m_pbos[readPBO].ReadUInt32() & 0x00FFFFFF; // Mask to 24-bit

                    if (pickedID > 0) {
                        m_selectedEntityID = pickedID; // Store for outline rendering and doing property updates etc.
                        std::cout << "Picked entity index: " << pickedID << std::endl;
                        // ... handle picked ID
                    } else {
                        m_selectedEntityID = 0; // Clear selection if clicking empty space
                    }
                }
                firstFrame = false;

                // Frame N: Swap for next frame
                // Swap PBOs FIRST
                m_currentPBO = 1 - m_currentPBO;

                // Frame N: Write to PBO 0
                // Write to current PBO (async transfer starts)
                m_pbos[m_currentPBO].Bind(GL_PIXEL_PACK_BUFFER);
                glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
                m_pbos[m_currentPBO].Unbind(GL_PIXEL_PACK_BUFFER);


                // Restore blending state
                if (blendWasEnabled) glEnable(GL_BLEND);

                Framebuffer::Unbind();
            });

        // Highlight the currently selected entity (overlay onto HDR)
        // Read from HDR target, then write back to the same HDR target (overlay)
        m_renderGraph->AddPass("SelectionOutline", { "HDR" }, { "HDR" },
            [this, &world, &viewProj, &buckets, &transformedCorners, &polyPoints](ResourceAccessor& res)
            {
                // nothing selected this frame
                if (m_selectedEntityID == 0) return;

                auto* hdrFbo = res.GetFramebuffer("HDR");
                if (!hdrFbo) return;

                // IMPORTANT: do NOT clear since we want to draw on top
                hdrFbo->Bind();

                // make outline look 2px thick in screen space
                const auto& win = WindowManager::GetMainWindow();
                const float desiredPx = 2.0f;
                const float worldThickness = (m_cameraOrthoSize / win->Height()) * desiredPx;

                // selection color (pick whatever you like but it should contrast against viewport)
                const glm::vec4 selColor(1.0f, 0.85f, 0.15f, 1.0f); // yellow-ish

                // start with the normal 2D batch shader
                m_shader->use();
                m_shader->setMat4("uViewProj", viewProj);
                m_renderer->beginFrame();

                // scan all layers we rendered in Scene2D
                for (const auto& list : buckets)
                {
                    for (ECS::Entity entity : list)
                    {
                        // match by index, since picking stores only Index
                        if (entity.Index != m_selectedEntityID)
                            continue;

                        // skip inactive
                        if (world.Has<Components::Active>(entity) &&
                            !world.Get<Components::Active>(entity).Enabled)
                            continue;

                        // get transform (same helper as Scene2D)
                        const auto& lt = world.Get<Components::LocalTransform>(entity);
                        Vector3D position, scale;
                        Quaternion rotation;
                        GetRenderTransform(world, entity, lt, position, rotation, scale);

                        // 1) BOXES ----------------------------------------------------
                        if (world.Has<Components::ShapeBox2D>(entity))
                        {
                            const auto& sb = world.Get<Components::ShapeBox2D>(entity);

                            // check if rotated
                            const float rotAngle = 2.0f * std::acos(rotation.W);
                            const bool rotated = std::abs(rotAngle) > 0.01f;

                            if (!rotated)
                            {
                                const glm::vec2 halfExtents = {
                                    sb.HalfExtents.X * scale.X,
                                    sb.HalfExtents.Y * scale.Y
                                };

                                const glm::vec2 center = {
                                    position.X + sb.Offset.X,
                                    position.Y + sb.Offset.Y
                                };

                                const glm::vec2 min = center - halfExtents;
                                const glm::vec2 max = center + halfExtents;

                                DebugDraw2D::RectStroke(
                                    *m_renderer,
                                    min, max,
                                    worldThickness,
                                    selColor,
                                    /*textureId*/ 0);
                            }
                            else
                            {
                                // rotated box: build 4 corners like Scene2D
                                const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                                transformedCorners.clear(); transformedCorners.reserve(4);

                                const Vector2D he = sb.HalfExtents;
                                const Vector3D corners[4] = {
                                    { -he.X, -he.Y, 0.0f },
                                    {  he.X, -he.Y, 0.0f },
                                    {  he.X,  he.Y, 0.0f },
                                    { -he.X,  he.Y, 0.0f }
                                };

                                for (auto c : corners)
                                {
                                    const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                                    transformedCorners.push_back(
                                        glm::vec2(hc.X + sb.Offset.X, hc.Y + sb.Offset.Y)
                                    );
                                }

                                // draw 4 edges
                                for (int i = 0; i < 4; ++i)
                                {
                                    DebugDraw2D::Line(
                                        *m_renderer,
                                        transformedCorners[i],
                                        transformedCorners[(i + 1) % 4],
                                        worldThickness,
                                        selColor,
                                        0);
                                }
                            }

                            continue;
                        }

                        // 2) SPRITES --------------------------------------------------
                        if (world.Has<Components::SpriteRenderer2D>(entity))
                        {
                            // we outline the *quad*, not opaque pixels
                            const float angleZ = std::atan2(
                                2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                                1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
                            );

                            const bool rotated = std::abs(angleZ) > 0.001f;

                            if (!rotated)
                            {
                                const glm::vec2 half = {
                                    scale.X * 0.5f,
                                    scale.Y * 0.5f
                                };

                                const glm::vec2 min = {
                                    position.X - half.x,
                                    position.Y - half.y
                                };
                                const glm::vec2 max = {
                                    position.X + half.x,
                                    position.Y + half.y
                                };

                                DebugDraw2D::RectStroke(
                                    *m_renderer,
                                    min, max,
                                    worldThickness,
                                    selColor,
                                    0);
                            }
                            else
                            {
                                // build unit quad and TRS it
                                const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                                transformedCorners.clear(); transformedCorners.reserve(4);

                                const Vector3D corners[4] = {
                                    { -0.5f, -0.5f, 0.0f },
                                    {  0.5f, -0.5f, 0.0f },
                                    {  0.5f,  0.5f, 0.0f },
                                    { -0.5f,  0.5f, 0.0f }
                                };

                                for (auto c : corners)
                                {
                                    const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                                    transformedCorners.push_back(glm::vec2(hc.X, hc.Y));
                                }

                                for (int i = 0; i < 4; ++i)
                                {
                                    DebugDraw2D::Line(
                                        *m_renderer,
                                        transformedCorners[i],
                                        transformedCorners[(i + 1) % 4],
                                        worldThickness,
                                        selColor,
                                        0);
                                }
                            }

                            continue;
                        }

                        // 3) CIRCLES --------------------------------------------------
                        if (world.Has<Components::ShapeCircle2D>(entity))
                        {
                            const auto& sc = world.Get<Components::ShapeCircle2D>(entity);

                            // circle center in world
                            const glm::vec2 center = {
                                position.X + sc.Offset.X,
                                position.Y + sc.Offset.Y
                            };

                            // apply non-uniform scale by averaging (same as the circle draw)
                            const float radius =
                                sc.Radius * ((scale.X + scale.Y) * 0.5f);

                            // just outline the circle's quad (AABB)
                            const glm::vec2 half = { radius, radius };
                            const glm::vec2 min = center - half;
                            const glm::vec2 max = center + half;

                            DebugDraw2D::RectStroke(
                                *m_renderer,
                                min, max,
                                worldThickness,
                                selColor,
                                0);

                            continue;
                        }

                        // 4) POLYGONS -------------------------------------------------
                        if (world.Has<Components::ShapePolygon2D<32>>(entity))
                        {
                            const auto& pl = world.Get<Components::ShapePolygon2D<32>>(entity);
                            if (pl.Count >= 2)
                            {
                                const auto m = TransformUtils::MakeTRS(position, rotation, scale);
                                polyPoints.clear(); polyPoints.reserve(pl.Count);

                                for (uint32_t i = 0; i < pl.Count; ++i)
                                {
                                    const Vector3D p3{ pl.Points[i].X, pl.Points[i].Y, 0.0f };
                                    const Vector4D hp = m * Vector4D{ p3.X, p3.Y, p3.Z, 1.0f };
                                    polyPoints.push_back(glm::vec2(hp.X, hp.Y));
                                }

                                for (uint32_t i = 0; i < pl.Count; ++i)
                                {
                                    DebugDraw2D::Line(
                                        *m_renderer,
                                        polyPoints[i],
                                        polyPoints[(i + 1) % pl.Count],
                                        worldThickness,
                                        selColor,
                                        0);
                                }
                            }

                            continue;
                        }

                        // 5) LINES ----------------------------------------------------
                        if (world.Has<Components::ShapeLine2D>(entity))
                        {
                            const auto& sl = world.Get<Components::ShapeLine2D>(entity);

                            const glm::vec2 a = {
                                position.X + sl.A.X,
                                position.Y + sl.A.Y
                            };
                            const glm::vec2 b = {
                                position.X + sl.B.X,
                                position.Y + sl.B.Y
                            };

                            DebugDraw2D::Line(
                                *m_renderer,
                                a, b,
                                worldThickness,
                                selColor,
                                0);

                            continue;
                        }

                        // other component types: ignore
                    }
                }

                m_renderer->endFrame();
                Framebuffer::Unbind();
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
                m_bloomBlurShader->setUniform("uSamples", std::max(12, int(bloomRadiusTexels * 0.6f)));     // Increase uSamples proportionally to uRadius
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
                m_bloomBlurShader->setUniform("uSamples", std::max(12, int(bloomRadiusTexels * 0.6f)));     // Increase uSamples proportionally to uRadius
                m_bloomBlurShader->setUniform("uFalloff", 0.15f);  // LESS FALLOFF

                src->BindColorTexture(0);
                m_renderer->drawFullscreenQuad();
                Framebuffer::Unbind();
            });

        // Pass 2: Blit HDR to backbuffer
        m_renderGraph->AddPass("Composite", { "HDR", "BloomExtract" }, { "Backbuffer" },
            [this](ResourceAccessor& res)
            {
                auto* hdr = res.GetFramebuffer("HDR");
                auto* bloom = res.GetFramebuffer("BloomExtract");
                if (!hdr || !bloom) return;

                const auto& win = WindowManager::GetMainWindow();
                Framebuffer::BindDefault();
                glViewport(0, 0, win->Width(), win->Height());

                m_bloomCombineShader->use();
                m_bloomCombineShader->setUniform("uScene", 0);
                m_bloomCombineShader->setUniform("uBloomBlur", 1);
                m_bloomCombineShader->setUniform("uExposure", 1.3f);      // Or 0.8f if still too bright?
                m_bloomCombineShader->setUniform("uBloomStrength", 5.2f); // Control bloom intensity
                m_bloomCombineShader->setUniform("uGamma", 1.5f);
                hdr->BindColorTexture(0, 0);
                bloom->BindColorTexture(0, 1);
                m_renderer->drawFullscreenQuad();
            });

        // ============================================================
        // EXECUTE RENDER GRAPH
        // ============================================================
        m_renderGraph->Execute();

        // Performance logging
        if (Time::FrameCount() % 60 == 0)
        {
            static int previousFlushTotal = 0;
            int currentTotal = GetFlushCount();
            int flushes = currentTotal - previousFlushTotal;
            previousFlushTotal = currentTotal;
            LOG_DEBUG("=== RENDERER ANALYSIS ===");
            LOG_DEBUG("Flushes this frame: " << flushes);
            if (flushes > 10)
            {
                LOG_DEBUG("Too many flushes! Likely texture switches or buffer overflows...");
            }
            else if (flushes == 1)
            {
                LOG_DEBUG("Single batch, bottleneck is CPU-side or GPU fillrate");
            }
            LOG_DEBUG("FPS: " << (1.0f / Time::DeltaTime()));
            LOG_DEBUG("=========================");
        }
    }
}
