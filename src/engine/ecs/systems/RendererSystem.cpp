#include "ecs/systems/RendererSystem.h"
#include "core/Application.h"
#include "graphics/renderer.hpp"
#include <iterator>
#include "services/WindowManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include "ecs/Components.h"
#include "graphics/texture.hpp"

namespace ECS {
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

        // Using DebugDraw first
		// TODO: Replace with actual RendererSystem calls
        
        // Circles
        world.Each<Components::LocalTransform, Components::ShapeCircle2D, Components::Layer>([&](ECS::Entity, Components::LocalTransform& lt, Components::ShapeCircle2D& sc, Components::Layer& ly){
            // Optional: filter by a render layer
            (void)ly;
            // const auto M = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);
            // TODO: make use of M, and other params like Thickness, Filled

            DebugDraw2D::Circle(*m_renderer,
                ToGlm(Vector2D{lt.Position.X, lt.Position.Y}) + ToGlm(sc.Offset),
                sc.Radius * ((lt.Scale.X + lt.Scale.Y) * 0.5f),
                ToGlm(sc.Color),
                48, 0);
        });

        // Boxes
        world.Each<Components::LocalTransform, Components::ShapeBox2D, Components::Layer>([&](ECS::Entity, Components::LocalTransform& lt, Components::ShapeBox2D& sb, Components::Layer& ly){
            (void)ly;
            // const auto M = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);

            glm::vec2 halfExtents = ToGlm(Vector2D{sb.HalfExtents.X * lt.Scale.X, sb.HalfExtents.Y * lt.Scale.Y});
            glm::vec2 center = ToGlm(Vector2D{lt.Position.X, lt.Position.Y}) + ToGlm(sb.Offset);

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
        });

        // Lines
        world.Each<Components::LocalTransform, Components::ShapeLine2D, Components::Layer>([&](ECS::Entity, Components::LocalTransform& lt, Components::ShapeLine2D& sl, Components::Layer& ly){
            (void)ly;
            // const auto M = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);

            DebugDraw2D::Line(*m_renderer,
                ToGlm(Vector2D{lt.Position.X, lt.Position.Y} + sl.A),
                ToGlm(Vector2D{lt.Position.X, lt.Position.Y} + sl.B),
                sl.Thickness,
                ToGlm(sl.Color), 0);
        });

        // Polygons?
        world.Each<Components::LocalTransform, Components::ShapePolygon2D<32>>([&](ECS::Entity, Components::LocalTransform& lt, Components::ShapePolygon2D<32>& pl){
            if (pl.Count < 2)
                return;
            const auto M = TransformUtils::MakeTRS(lt.Position, lt.Rotation, lt.Scale);

            // Transform to world in a small stack buffer (avoid heap)
            std::vector<glm::vec2> points;
            points.reserve(pl.Count);
            for (uint32_t i = 0; i < pl.Count; ++i) {
                // Promote to 3D -> multiply -> project to XY
                Vector3D p3{ pl.Points[i].X, pl.Points[i].Y, 0.0f };
                Vector4D hp = M * Vector4D{ p3.X, p3.Y, p3.Z, 1.0f };
                points[i] = ToGlm(Vector2D{ hp.X, hp.Y });
            }
            
            DebugDraw2D::Polygon(*m_renderer,
                points,
                ToGlm(pl.FillColor),
                0);
        });


        // SpriteRenderers
        world.Each<Components::LocalTransform, Components::SpriteRenderer2D, Components::Layer>([&](ECS::Entity, Components::LocalTransform& lt, Components::SpriteRenderer2D& sr, Components::Layer& ly) {
            (void)ly;
            m_renderer->submitSprite({
                ToGlm(Vector2D{lt.Position.X, lt.Position.Y}),  // pos
                ToGlm(Vector2D{lt.Scale.X, lt.Scale.Y}),        // size
                {0.f, 0.f, 1.f, 1.f},                           // uv
                ToGlm(sr.Color),                                // color
                sr.TextureId,                                   // textureId (GLuint)
                glm::radians(2 * acos(lt.Rotation.W)),          // rotation (radians)
                1.0f                                            // uniformScale
            });
        });

        m_renderer->endFrame();
    }
}
