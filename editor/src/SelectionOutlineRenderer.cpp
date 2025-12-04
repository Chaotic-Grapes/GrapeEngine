/* Start Header *****************************************************************/
/*!
\file   SelectionOutlineRenderer.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu

\brief
Implements editor-side selection wireframe rendering.

Draws a yellow wireframe outline around the selected entity for visual feedback.
Supports boxes, sprites, circles, polygons, and lines.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "SelectionOutlineRenderer.h"
#include "ecs/Entity.h"
#include "ecs/Components.h"
#include "graphics/renderer.hpp"
#include "graphics/shader.hpp"
#include "graphics/debugDraw2D.hpp"
#include "helpers/TransformUtils.h"
#include "math/Matrix4x4.h"
#include "math/Vector3D.h"
#include "math/Vector4D.h"
#include "math/Quaternion.h"
#include <vector>

namespace Editor {

    // Helper to get effective transform (WorldTransform if available, otherwise LocalTransform)
    static void GetRenderTransform(ECS::World& world, const ECS::Entity entity,
        const ECS::Components::LocalTransform& lt,
        Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) {
        if (world.Has<ECS::Components::WorldTransform>(entity)) {
            const auto& wt = world.Get<ECS::Components::WorldTransform>(entity);
            TransformUtils::DecomposeTRS(wt.Matrix, outPosition, outRotation, outScale);
        }
        else {
            outPosition = lt.Position;
            outRotation = lt.Rotation;
            outScale = lt.Scale;
        }
    }

    void SelectionOutlineRenderer::RenderOutline(
        ECS::World& world,
        uint32_t selectedEntityID,
        Renderer* renderer,
        Shader* shader,
        const glm::mat4& viewProj,
        float cameraOrthoSize,
        float windowHeight)
    {
        if (selectedEntityID == 0 || !renderer || !shader) return;

        ECS::Entity entity = world.Resolve(selectedEntityID);
        if (entity.IsNull() || !world.IsAlive(entity)) return;
        if (!world.Has<ECS::Components::LocalTransform>(entity)) return;

        // Skip inactive entities
        if (world.Has<ECS::Components::Active>(entity) &&
            !world.Get<ECS::Components::Active>(entity).Enabled)
            return;

        // Calculate 2px thick outline in screen space
        const float desiredPx = 2.0f;
        const float worldThickness = (cameraOrthoSize / windowHeight) * desiredPx;
        const glm::vec4 selColor(1.0f, 0.85f, 0.15f, 1.0f); // Yellow-ish

        // Setup rendering
        shader->use();
        shader->setMat4("uViewProj", viewProj);
        renderer->beginFrame();

        // Get transform
        const auto& lt = world.Get<ECS::Components::LocalTransform>(entity);
        Vector3D position, scale;
        Quaternion rotation;
        GetRenderTransform(world, entity, lt, position, rotation, scale);

        // Reusable buffers
        std::vector<glm::vec2> transformedCorners;
        std::vector<glm::vec2> polyPoints;

        // 1) BOXES
        if (world.Has<ECS::Components::ShapeBox2D>(entity))
        {
            const auto& sb = world.Get<ECS::Components::ShapeBox2D>(entity);
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

                DebugDraw2D::RectStroke(*renderer, min, max, worldThickness, selColor, 0);
            }
            else
            {
                const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                transformedCorners.clear();
                transformedCorners.reserve(4);

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
                    transformedCorners.push_back(glm::vec2(hc.X + sb.Offset.X, hc.Y + sb.Offset.Y));
                }

                for (int i = 0; i < 4; ++i)
                {
                    DebugDraw2D::Line(*renderer,
                        transformedCorners[i],
                        transformedCorners[(i + 1) % 4],
                        worldThickness, selColor, 0);
                }
            }
        }
        // 2) SPRITES
        else if (world.Has<ECS::Components::SpriteRenderer2D>(entity))
        {
            const float angleZ = std::atan2(
                2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
            );
            const bool rotated = std::abs(angleZ) > 0.001f;

            if (!rotated)
            {
                const glm::vec2 half = { scale.X * 0.5f, scale.Y * 0.5f };
                const glm::vec2 min = { position.X - half.x, position.Y - half.y };
                const glm::vec2 max = { position.X + half.x, position.Y + half.y };

                DebugDraw2D::RectStroke(*renderer, min, max, worldThickness, selColor, 0);
            }
            else
            {
                const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                transformedCorners.clear();
                transformedCorners.reserve(4);

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
                    DebugDraw2D::Line(*renderer,
                        transformedCorners[i],
                        transformedCorners[(i + 1) % 4],
                        worldThickness, selColor, 0);
                }
            }
        }
        // 3) CIRCLES
        else if (world.Has<ECS::Components::ShapeCircle2D>(entity))
        {
            const auto& sc = world.Get<ECS::Components::ShapeCircle2D>(entity);
            const glm::vec2 center = {
                position.X + sc.Offset.X,
                position.Y + sc.Offset.Y
            };
            const float radius = sc.Radius * ((scale.X + scale.Y) * 0.5f);
            const glm::vec2 half = { radius, radius };
            const glm::vec2 min = center - half;
            const glm::vec2 max = center + half;

            DebugDraw2D::RectStroke(*renderer, min, max, worldThickness, selColor, 0);
        }
        // 4) POLYGONS
        else if (world.Has<ECS::Components::ShapePolygon2D<32>>(entity))
        {
            const auto& pl = world.Get<ECS::Components::ShapePolygon2D<32>>(entity);
            if (pl.Count >= 2)
            {
                const auto m = TransformUtils::MakeTRS(position, rotation, scale);
                polyPoints.clear();
                polyPoints.reserve(pl.Count);

                for (uint32_t i = 0; i < pl.Count; ++i)
                {
                    const Vector3D p3{ pl.Points[i].X, pl.Points[i].Y, 0.0f };
                    const Vector4D hp = m * Vector4D{ p3.X, p3.Y, p3.Z, 1.0f };
                    polyPoints.push_back(glm::vec2(hp.X, hp.Y));
                }

                for (uint32_t i = 0; i < pl.Count; ++i)
                {
                    DebugDraw2D::Line(*renderer,
                        polyPoints[i],
                        polyPoints[(i + 1) % pl.Count],
                        worldThickness, selColor, 0);
                }
            }
        }
        // 5) LINES
        else if (world.Has<ECS::Components::ShapeLine2D>(entity))
        {
            const auto& sl = world.Get<ECS::Components::ShapeLine2D>(entity);
            const glm::vec2 a = { position.X + sl.A.X, position.Y + sl.A.Y };
            const glm::vec2 b = { position.X + sl.B.X, position.Y + sl.B.Y };

            DebugDraw2D::Line(*renderer, a, b, worldThickness, selColor, 0);
        }

        renderer->endFrame();
    }

}
