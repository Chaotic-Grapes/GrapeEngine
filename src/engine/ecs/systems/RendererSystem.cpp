#include "ecs/systems/RendererSystem.h"
#include "core/Application.h"
#include "graphics/renderer.hpp"
#include <iterator>
#include "services/WindowManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include "ecs/Components.h"
#include "graphics/texture.hpp"
#include "services/Time.h"

namespace Engine {
    RendererSystem::RendererSystem(World* world) : m_world(world) {}

    void RendererSystem::OnCreate() {
        // Window and dimensions
        const auto& mainWindow = WindowManager::GetMainWindow();
        const int width = mainWindow->Width();
        const int height = mainWindow->Height();

        // Shaders
        m_shader = std::make_unique<Shader>("assets/shaders/batch.vert", "assets/shaders/batch.frag");
        m_textShader = std::make_unique<Shader>("assets/shaders/sdf_text.vert", "assets/shaders/sdf_text.frag");

        // Renderer
        m_renderer = std::make_unique<Renderer>(3000);

        // --- Editor Camera ---
        m_editorCamera = std::make_unique<EditorCamera>(*m_world);

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

    void RendererSystem::OnUpdate() {
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
            for (auto& [transform, camera] :
                m_world->GetEntityManager().Query<Component::Transform, Component::Camera3D>())
            {
                if (!camera.Active) continue; // skip inactive cameras

                // Build View
                view = glm::lookAt(
                    glm::vec3(transform.Position.X, transform.Position.Y, camera.Z),
                    glm::vec3(transform.Position.X, transform.Position.Y, camera.Z - 1.0f),
                    glm::vec3(0, 1, 0)
                );

                // Build Projection
                if (camera.UsePerspective)
                    projection = glm::perspective(camera.FOV, camera.AspectRatio, camera.NearPlane, camera.FarPlane);
                else
                    projection = glm::ortho(
                        -camera.OrthoSize * camera.AspectRatio * 0.5f,
                        camera.OrthoSize * camera.AspectRatio * 0.5f,
                        -camera.OrthoSize * 0.5f,
                        camera.OrthoSize * 0.5f,
                        camera.NearPlane, camera.FarPlane
                    );

                foundActive = true;
                break;
            }
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
        m_shader->use();
        m_shader->setMat4("uViewProj", projection * view);

        m_renderer->beginFrame();

        // Using DebugDraw first
		// TODO: Replace with actual RendererSystem calls
        for (auto& [transform, shape] : m_world->GetEntityManager().Query<Component::Transform, Component::ShapeRenderer2D>()) {
            switch (shape.Type) {
            case Component::ShapeRenderer2D::ShapeType::Rectangle: {
                // Compute rectangle bounds from Transform
                glm::vec2 halfExtents = ToGlm(transform.Scale) * 0.5f;
                glm::vec2 center = ToGlm(transform.Position);

                glm::vec2 min = center - halfExtents;
                glm::vec2 max = center + halfExtents;

                if (shape.OutlineThickness > 0.0f) {
                    // Stroke (outline rectangle)
                    DebugDraw2D::RectStroke(*m_renderer,
                        min,
                        max,
                        shape.OutlineThickness,
                        ToGlm(shape.OutlineColor),
                        0); // texture ID (0 = solid color)
                }
                else {
                    // Solid filled rectangle
                    DebugDraw2D::RectFill(*m_renderer,
                        min,
                        max,
                        ToGlm(shape.FillColor),
                        0); // texture ID (0 = solid color)
                }
                break;
            }

            case Component::ShapeRenderer2D::ShapeType::Circle: {
                // Apply transform scale to radius
                // Use average of X and Y scale for uniform circle scaling
                float scaledRadius = shape.Radius * ((transform.Scale.X + transform.Scale.Y) * 0.5f);
                
                DebugDraw2D::Circle(*m_renderer,
                    ToGlm(transform.Position),
                    scaledRadius,
                    ToGlm(shape.FillColor),
                    48, 0);
                break;
            }
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
			//case Component::ShapeRenderer2D::ShapeType::Line:
			//    DebugDraw2D::Line(*m_renderer,
			//        ToGlm(transform.Position + shape.Points[0]),
			//        ToGlm(transform.Position + shape.Points[1]),
			//        shape.OutlineThickness,
			//        ToGlm(shape.FillColor), 0);
			//    break;
            }
        }

        for (auto& [transform, line] : m_world->GetEntityManager().Query<Component::Transform, Component::LineRenderer>()) {
            DebugDraw2D::Line(*m_renderer,
                ToGlm(line.Start),
                ToGlm(line.End),
                2.0f,
                ToGlm(line.Color), 0);
		}

        for (auto& [transform, sprite] : m_world->GetEntityManager().Query<Component::Transform, Component::SpriteRenderer>()) {
            m_renderer->submitSprite({
                ToGlm(transform.Position),          // pos
                ToGlm(transform.Scale),             // size
                {0.f, 0.f, 1.f, 1.f},               // uv
                ToGlm(sprite.Color),                // color
                sprite.TextureId,                   // textureId (GLuint)
                glm::radians(transform.Rotation),   // rotation (radians)
                1.0f                                // uniformScale
            });
        }

        m_renderer->endFrame();
    }
}
