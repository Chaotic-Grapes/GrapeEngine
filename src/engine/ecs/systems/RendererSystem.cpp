#include "ecs/systems/RendererSystem.h"
#include "core/Application.h"
#include "graphics/renderer.hpp"
#include <algorithm>
#include <iterator>
#include "services/WindowManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include "ecs/Components.h"
#include "graphics/texture.hpp"
#include "services/Time.h"
#include "helpers/TransformUtils.h"

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
        } else {
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

        // Renderer
        m_renderer = std::make_unique<Renderer>(15000);

        // --- Editor Camera ---
        m_editorCamera = std::make_unique<Engine::EditorCamera>(world);

        // Framebuffers
        m_fbos["hdr"] = std::make_unique<Framebuffer>();
        m_fbos["hdr"]->Create(width, height, true);

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

    void RendererSystem::Update(World& world, const float dt) {
        (void)dt;
        if (!m_renderer)
            return;

        // Batching pipeline
        glClearColor(0.1f, 0.1f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
        bool foundActive = false;

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

        // We only send (Projection * View) here instead of the full (Projection * View * Model)
        // because each sprite/shape's model transform (position, rotation, scale) is already
        // baked into its vertex positions on the CPU during batching. By the time vertices
        // reach the GPU, they are in world space, so the shader only needs to transform them
        // into camera (view) space and then into clip space.
        // I will remind myself to change this in the future
        const glm::mat4 viewProj = projection * view;

        // Determine max layer id present this frame
        int maxLayerId = -1;
        world.Each<Components::Layer>([&](ECS::Entity, const Components::Layer &ly) {
            maxLayerId = std::max(static_cast<int>(ly.Id), maxLayerId);
        });

        // Render per-layer from back (0) to front (max)
        // Single-pass collection: bucket entities by layer then process each
        std::vector<std::vector<ECS::Entity>> buckets;
        buckets.resize(std::max(1, maxLayerId + 1));

        // Collect entities that have LocalTransform + Layer
        world.Each<Components::LocalTransform, Components::Layer>([&](ECS::Entity entity, Components::LocalTransform& lt, Components::Layer& ly){
            uint16_t lid = static_cast<uint16_t>(ly.Id);
            if (lid < buckets.size())
                buckets[lid].push_back(entity);
        });

        // Reusable temporary buffers to avoid per-entity allocations
        thread_local std::vector<glm::vec2> transformedCorners;
        thread_local std::vector<glm::vec2> polyPoints;

        // ---------------------------------------
        // Layered rendering: SDF first, then batch
        // ---------------------------------------
        for (int layer = 0; layer <= maxLayerId; ++layer) {
            if (layer >= (int)buckets.size()) continue;
            const auto& list = buckets[layer];

            // --- Sub-pass 1: SDF circles on this layer ---
            m_sdfCircleShader->use();
            m_sdfCircleShader->setMat4("uViewProj", viewProj);
            m_sdfCircleShader->setUniform("uStrokePx", 0.0f); // filled SDF circles; set >0 for outlines (see note)
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
                // If we want per-circle outlines later, we'll need to either:
                //  - encode stroke in a vertex attribute, or
                //  - flush per thickness (costly). For now we keep filled circles.
            }

            m_renderer->endFrame(); // flush SDF for this layer

            // --- Sub-pass 2: everything else on this layer ---
            m_shader->use();
            m_shader->setMat4("uViewProj", viewProj);
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
        }

        if (Time::FrameCount() % 60 == 0)
        {
            // Renderer exposes a cumulative flush counter. Compute per-frame
            // flushes by tracking the previous total and taking the delta.
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
