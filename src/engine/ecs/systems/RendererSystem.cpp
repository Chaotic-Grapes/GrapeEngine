#include "ecs/systems/RendererSystem.h"
#include "core/Application.h"
#include "graphics/renderer.hpp"
#include <iterator>
#include "services/WindowManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include "ecs/Components.h"
#include "graphics/texture.hpp"
#include "helpers/TransformUtils.h"

namespace ECS {
    // Helper function to get the effective transform for rendering
    // Uses WorldTransform if available, otherwise falls back to LocalTransform
    static void GetRenderTransform(World& world, Entity entity, 
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

        m_renderer->beginFrame();

        // Determine max layer id present this frame
        int maxLayerId = -1;
        world.Each<Components::Layer>([&](ECS::Entity, const Components::Layer& ly) {
            if (static_cast<int>(ly.Id) > maxLayerId) maxLayerId = static_cast<int>(ly.Id);
        });

        // Render per-layer from back (0) to front (max)
        for (int layer = 0; layer <= maxLayerId; ++layer) {
            // Circles
            world.Each<Components::LocalTransform, Components::ShapeCircle2D, Components::Layer>([&](ECS::Entity entity, Components::LocalTransform& lt, Components::ShapeCircle2D& sc, Components::Layer& ly){
                if (ly.Id != static_cast<uint16_t>(layer)) return;
                // Skip if entity has Active component and is disabled
                if (world.Has<Components::Active>(entity)) {
                    const auto& active = world.Get<Components::Active>(entity);
                    if (!active.Enabled) return;
                }

                // Get the effective transform (world if available, local otherwise)
                Vector3D position, scale;
                Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                DebugDraw2D::Circle(*m_renderer,
                    ToGlm(Vector2D{position.X, position.Y}) + ToGlm(sc.Offset),
                    sc.Radius * ((scale.X + scale.Y) * 0.5f),
                    ToGlm(sc.Color),
                    48, 0);
            });

            // Boxes
            world.Each<Components::LocalTransform, Components::ShapeBox2D, Components::Layer>([&](ECS::Entity entity, Components::LocalTransform& lt, Components::ShapeBox2D& sb, Components::Layer& ly){
                if (ly.Id != static_cast<uint16_t>(layer)) return;
                // Skip if entity has Active component and is disabled
                if (world.Has<Components::Active>(entity)) {
                    const auto& active = world.Get<Components::Active>(entity);
                    if (!active.Enabled) return;
                }

                // Get the effective transform (world if available, local otherwise)
                Vector3D position, scale;
                Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                // Check if rotation is significant (not identity)
                const float rotationAngle = 2.0f * std::acos(rotation.W); // angle in radians
                const bool hasRotation = std::abs(rotationAngle) > 0.01f; // threshold for "no rotation"

                if (!hasRotation) {
                    // No rotation, use simple axis-aligned rect
                    glm::vec2 halfExtents = ToGlm(Vector2D{sb.HalfExtents.X * scale.X, sb.HalfExtents.Y * scale.Y});
                    glm::vec2 center = ToGlm(Vector2D{position.X, position.Y}) + ToGlm(sb.Offset);

                    glm::vec2 min = center - halfExtents;
                    glm::vec2 max = center + halfExtents;

                    if (sb.Filled) {
                        DebugDraw2D::RectFill(*m_renderer,
                            min,
                            max,
                            ToGlm(sb.Color),
                            0);
                    }
                    else {
                        DebugDraw2D::RectStroke(*m_renderer,
                            min,
                            max,
                            sb.Thickness,
                            ToGlm(sb.Color),
                            0);
                    }
                }
                else {
                    // Has rotation, render as a transformed polygon (4 corners)
                    const Matrix4x4 M = TransformUtils::MakeTRS(position, rotation, scale);

                    // Define box corners in local space (before offset)
                    Vector2D halfExt = sb.HalfExtents;
                    Vector3D corners[4] = {
                        Vector3D{-halfExt.X, -halfExt.Y, 0.0f},
                        Vector3D{ halfExt.X, -halfExt.Y, 0.0f},
                        Vector3D{ halfExt.X,  halfExt.Y, 0.0f},
                        Vector3D{-halfExt.X,  halfExt.Y, 0.0f}
                    };

                    // Transform corners to world space
                    std::vector<glm::vec2> transformedCorners;
                    transformedCorners.reserve(4);
                    for (int i = 0; i < 4; ++i) {
                        Vector4D corner4D = M * Vector4D{corners[i].X, corners[i].Y, corners[i].Z, 1.0f};
                        transformedCorners.push_back(ToGlm(Vector2D{corner4D.X, corner4D.Y}) + ToGlm(sb.Offset));
                    }

                    // Render as polygon
                    if (sb.Filled) {
                        DebugDraw2D::Polygon(*m_renderer, transformedCorners, ToGlm(sb.Color), 0);
                    }
                    else {
                        // Draw outline as 4 lines
                        for (int i = 0; i < 4; ++i) {
                            DebugDraw2D::Line(*m_renderer,
                                transformedCorners[i],
                                transformedCorners[(i + 1) % 4],
                                sb.Thickness,
                                ToGlm(sb.Color),
                                0);
                        }
                    }
                }
            });

            // Lines
            world.Each<Components::LocalTransform, Components::ShapeLine2D, Components::Layer>([&](ECS::Entity entity, Components::LocalTransform& lt, Components::ShapeLine2D& sl, Components::Layer& ly){
                if (ly.Id != static_cast<uint16_t>(layer)) return;
                // Skip if entity has Active component and is disabled
                if (world.Has<Components::Active>(entity)) {
                    const auto& active = world.Get<Components::Active>(entity);
                    if (!active.Enabled) return;
                }

                // Get the effective transform (world if available, local otherwise)
                Vector3D position, scale;
                Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                DebugDraw2D::Line(*m_renderer,
                    ToGlm(Vector2D{position.X, position.Y} + sl.A),
                    ToGlm(Vector2D{position.X, position.Y} + sl.B),
                    sl.Thickness,
                    ToGlm(sl.Color), 0);
            });

            // Polygons (explicit Layer filter and fix point collection)
            world.Each<Components::LocalTransform, Components::ShapePolygon2D<32>, Components::Layer>([&](ECS::Entity entity, Components::LocalTransform& lt, Components::ShapePolygon2D<32>& pl, Components::Layer& ly){
                if (ly.Id != static_cast<uint16_t>(layer)) return;
                // Skip if entity has Active component and is disabled
                if (world.Has<Components::Active>(entity)) {
                    const auto& active = world.Get<Components::Active>(entity);
                    if (!active.Enabled) return;
                }

                if (pl.Count < 2)
                    return;

                // Get the effective transform (world if available, local otherwise)
                Vector3D position, scale;
                Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                const auto M = TransformUtils::MakeTRS(position, rotation, scale);

                // Transform to world in a small stack buffer (avoid heap)
                std::vector<glm::vec2> points;
                points.reserve(pl.Count);
                for (uint32_t i = 0; i < pl.Count; ++i) {
                    // Promote to 3D -> multiply -> project to XY
                    Vector3D p3{ pl.Points[i].X, pl.Points[i].Y, 0.0f };
                    Vector4D hp = M * Vector4D{ p3.X, p3.Y, p3.Z, 1.0f };
                    points.push_back(ToGlm(Vector2D{ hp.X, hp.Y }));
                }

                DebugDraw2D::Polygon(*m_renderer,
                    points,
                    ToGlm(pl.FillColor),
                    0);
            });

            // SpriteRenderers
            world.Each<Components::LocalTransform, Components::SpriteRenderer2D, Components::Layer>([&](ECS::Entity entity, Components::LocalTransform& lt, Components::SpriteRenderer2D& sr, Components::Layer& ly) {
                if (ly.Id != static_cast<uint16_t>(layer)) return;
                // Skip if entity has Active component and is disabled
                if (world.Has<Components::Active>(entity)) {
                    const auto& active = world.Get<Components::Active>(entity);
                    if (!active.Enabled) return;
                }

                // Get the effective transform (world if available, local otherwise)
                Vector3D position, scale;
                Quaternion rotation;
                GetRenderTransform(world, entity, lt, position, rotation, scale);

                m_renderer->submitSprite({
                    ToGlm(Vector2D{position.X, position.Y}),        // pos
                    ToGlm(Vector2D{scale.X, scale.Y}),              // size
                    {0.f, 0.f, 1.f, 1.f},                           // uv
                    ToGlm(sr.Color),                                // color
                    sr.TextureId,                                   // textureId (GLuint)
                    glm::radians(2 * acos(rotation.W)),             // rotation (radians)
                    1.0f                                            // uniformScale
                });
            });
        }

        m_renderer->endFrame();
    }
}
