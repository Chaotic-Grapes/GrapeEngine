#include "ecs/systems/RendererSystem.h"
#include "core/Application.h"
#include "graphics/renderer.hpp"
#include <algorithm>
#include <iterator>
#include "services/WindowManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include "ecs/Components.h"
#include "graphics/texture.hpp"
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

    void RendererSystem::Initialize() {
        if (m_initialized)
            return;
        
        m_initialized = true;
        m_shader = std::make_unique<Shader>("assets/shaders/batch.vert",
                                            "assets/shaders/batch.frag");
        m_textShader = std::make_unique<Shader>("assets/shaders/sdf_text.vert",
                                                "assets/shaders/sdf_text.frag");
        m_renderer = std::make_unique<Renderer>(3000);

        const auto& mainWindow = WindowManager::GetMainWindow();

        m_projection = glm::ortho(0.f,
            static_cast<float>(mainWindow->Width()),
            0.f,
            static_cast<float>(mainWindow->Height()),
            -1.f,
            1.f
        );

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

        m_shader->use();
        m_shader->setMat4("uProjection", m_projection);

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

        m_renderer->beginFrame();

        for (int layer = 0; layer <= maxLayerId; ++layer) {
            if (layer >= (int)buckets.size())
                continue;
            
            const auto& list = buckets[layer];
            for (ECS::Entity entity : list) {
                // Skip if entity disabled
                if (world.Has<Components::Active>(entity)) {
                    const auto& active = world.Get<Components::Active>(entity);
                    
                    if (!active.Enabled)
                        continue;
                }

                auto& lt = world.Get<Components::LocalTransform>(entity);
                Vector3D position;
                Vector3D scale;
                Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                // Circles
                if (world.Has<Components::ShapeCircle2D>(entity)) {
                    const auto& sc = world.Get<Components::ShapeCircle2D>(entity);
                    DebugDraw2D::Circle(*m_renderer,
                        ToGlm(Vector2D{position.X, position.Y}) + ToGlm(sc.Offset),
                        sc.Radius * ((scale.X + scale.Y) * 0.5f),
                        ToGlm(sc.Color),
                        48, 0);
                }

                // Boxes
                if (world.Has<Components::ShapeBox2D>(entity)) {
                    const auto& sb = world.Get<Components::ShapeBox2D>(entity);
                    const float rotationAngle = 2.0f * std::acos(rotation.W);
                    const bool hasRotation = std::abs(rotationAngle) > 0.01f;

                    if (!hasRotation) {
                        const glm::vec2 halfExtents = ToGlm(Vector2D{sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y});
                        const glm::vec2 center = ToGlm(Vector2D{position.X, position.Y}) + ToGlm(sb.Offset);
                        const glm::vec2 min = center - halfExtents;
                        const glm::vec2 max = center + halfExtents;

                        if (sb.Filled) {
                            DebugDraw2D::RectFill(*m_renderer, min, max, ToGlm(sb.Color), 0);
                        }
                        else {
                            DebugDraw2D::RectStroke(*m_renderer, min, max, sb.Thickness, ToGlm(sb.Color), 0);
                        }
                    } 
                    else {
                        const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                        transformedCorners.clear(); transformedCorners.reserve(4);
                        const Vector2D halfExt = sb.HalfExtents;
                        const Vector3D corners[4] = {
                            Vector3D{-halfExt.X, -halfExt.Y, 0.0f},
                            Vector3D{ halfExt.X, -halfExt.Y, 0.0f},
                            Vector3D{ halfExt.X,  halfExt.Y, 0.0f},
                            Vector3D{-halfExt.X,  halfExt.Y, 0.0f}
                        };
                        
                        for (auto corner : corners) {
                            const Vector4D corner4D = m * Vector4D{corner.X, corner.Y, corner.Z, 1.0f};
                            transformedCorners.push_back(ToGlm(Vector2D{corner4D.X, corner4D.Y}) + ToGlm(sb.Offset));
                        }

                        if (sb.Filled)
                            DebugDraw2D::Polygon(*m_renderer, transformedCorners, ToGlm(sb.Color), 0);
                        else {
                            for (int i = 0; i < 4; ++i) {
                                DebugDraw2D::Line(*m_renderer, transformedCorners[i], transformedCorners[(i+1)%4], sb.Thickness, ToGlm(sb.Color), 0);
                            }
                        }
                    }
                }

                // Lines
                if (world.Has<Components::ShapeLine2D>(entity)) {
                    const auto& sl = world.Get<Components::ShapeLine2D>(entity);
                    DebugDraw2D::Line(*m_renderer,
                        ToGlm(Vector2D{position.X, position.Y} + sl.A),
                        ToGlm(Vector2D{position.X, position.Y} + sl.B),
                        sl.Thickness,
                        ToGlm(sl.Color), 0);
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

                // SpriteRenderer
                if (world.Has<Components::SpriteRenderer2D>(entity)) {
                    const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                    
                    m_renderer->submitSprite({
                        ToGlm(Vector2D{position.X, position.Y}),
                        ToGlm(Vector2D{scale.X, scale.Y}),
                        {0.f, 0.f, 1.f, 1.f},
                        ToGlm(sr.Color),
                        sr.TextureId,
                        glm::radians(2 * acos(rotation.W)),
                        1.0f
                    });
                }
            }
        }

        m_renderer->endFrame();

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
