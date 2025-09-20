#ifndef RENDERER2D_H
#define RENDERER2D_H

#include "../include/graphics/renderer.hpp"
#include "../include/graphics/debugDraw2D.hpp"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include "Color.h"
#include "ecs/ISystem.h"
#include "ecs/World.h"
#include "graphics/shader.hpp"
#include "Math/Vector2D.h"

class Entity;
namespace Engine {
    class Renderer2D : public ISystem {
    public:
        Renderer2D(World* world);

        void OnCreate() override;
        void OnUpdate() override;
        std::string Name() const override { return "Renderer2D"; }

    private:
        // Conversion helpers (keep glm isolated to graphics)
        glm::vec2 ToGlm(const Vector2D& v) { return glm::vec2 {v.X, v.Y}; }
        glm::vec4 ToGlm(const Color& c) { return glm::vec4 {c.R, c.G, c.B, c.A}; }

        World* m_world;
        std::unique_ptr<Renderer> m_renderer;
		std::unique_ptr<Shader> m_shader;
        glm::mat4x4 m_projection = glm::identity<glm::mat4x4>();
    };
}

#endif