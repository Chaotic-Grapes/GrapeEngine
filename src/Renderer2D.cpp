#include "Renderer2D.h"
#include <iterator>
#include "Application.h"
#include "systems/WindowManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include "ecs/Components.h"
#include "graphics/texture.hpp"

namespace Engine {
    Renderer2D::Renderer2D(World* world) : m_world(world) {}

    void Renderer2D::OnCreate() {
        m_shader = std::make_unique<Shader>("assets/shaders/batch.vert",
            "assets/shaders/batch.frag");
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

    void Renderer2D::OnUpdate() {
        if (!m_renderer)
            return;

        // Batching pipeline
        glClearColor(0.1f, 0.1f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        m_shader->use();
        m_shader->setMat4("uProjection", m_projection);

        m_renderer->beginFrame();

        // Using DebugDraw first
		// TODO: Replace with actual Renderer2D calls
        for (auto& [transform, shape] : m_world->GetEntityManager().Query<Component::Transform, Component::ShapeRenderer2D>()) {
            switch (shape.Type) {
            case Component::ShapeRenderer2D::ShapeType::Rectangle:
                break;
            case Component::ShapeRenderer2D::ShapeType::Circle:
                DebugDraw2D::Circle(*m_renderer,
                    ToGlm(transform.Position),
                    shape.Radius,
                    ToGlm(shape.FillColor),
                    48, 0);
                break;
            case Component::ShapeRenderer2D::ShapeType::Polygon: {
                // Convert std::vector<Vector2> to std::vector<glm::vec2>
                std::vector<glm::vec2> points;
                points.reserve(shape.Points.size());
                std::transform(
                    shape.Points.begin(), shape.Points.end(), std::back_inserter(points),
                    [](const auto& in) {
                        return glm::vec2(in.X, in.Y);
                    }
                );

                DebugDraw2D::Polygon(*m_renderer,
                    points,
                    ToGlm(shape.FillColor),
                    0);
                break;
            }
            case Component::ShapeRenderer2D::ShapeType::Line:
                DebugDraw2D::Line(*m_renderer,
                    ToGlm(transform.Position + shape.Points[0]),
                    ToGlm(transform.Position + shape.Points[1]),
                    shape.OutlineThickness,
                    ToGlm(shape.FillColor), 0);
                break;
            }
        }

        for (auto& [transform, sprite] : m_world->GetEntityManager().Query<Component::Transform, Component::SpriteRenderer>()) {
            m_renderer->drawSprite({
                ToGlm(transform.Position),   // Position
                ToGlm(transform.Scale),      // Scale
                {0,0,0,0},
                ToGlm(sprite.Color),         // Tint
                sprite.Source.ID()           // Texture ID
            });
        }

        m_renderer->endFrame();
    }
}
