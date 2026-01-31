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
#include "core/Logger.h"
#include "ecs/Entity.h"
#include "ecs/Components.h"
#include "EditorECSUtils.h"
#include "graphics/renderer.hpp"
#include "graphics/shader.hpp"
#include "graphics/debugDraw2D.hpp"
#include "helpers/TransformUtils.h"
#include "math/Matrix4x4.h"
#include "math/Vector3D.h"
#include "math/Vector4D.h"
#include "math/Quaternion.h"
#include <vector>
#include <algorithm>

namespace Editor {
    // Forward declare helper defined later
    static void GetRenderTransform(ECS::World& world, const ECS::Entity entity,
        const ECS::Components::LocalTransform& lt,
        Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale);

    // Compute world-space AABB for an entity and all its descendants
    static bool ComputeWorldAABB(ECS::World& world, const ECS::Entity root, glm::vec2& outMin, glm::vec2& outMax) {
        const float kLarge = 1e9f;
        glm::vec2 minPt{ kLarge, kLarge };
        glm::vec2 maxPt{ -kLarge, -kLarge };
        bool any = false;

        // Helper to process a single entity and expand min/max
        auto process = [&](const ECS::Entity e) {
            if (!world.IsAlive(e)) return;
            if (!Editor::ECSUtils::HasComponent(&world, e, "LocalTransform")) return;

            const auto* lt = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(&world, e, "LocalTransform");
            if (!lt) return;
            Vector3D pos; Vector3D scale; Quaternion rot;
            GetRenderTransform(world, e, *lt, pos, rot, scale);

            // BOX
            if (Editor::ECSUtils::HasComponent(&world, e, "ShapeBox2D")) {
                const auto* sb = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeBox2D>(&world, e, "ShapeBox2D");
                if (!sb) return;
                const Matrix4x4 m = TransformUtils::MakeTRS(pos, rot, scale);
                const Vector2D he = sb->HalfExtents;
                const Vector3D corners[4] = {
                    { -he.X + sb->Offset.X, -he.Y + sb->Offset.Y, 0.0f },
                    {  he.X + sb->Offset.X, -he.Y + sb->Offset.Y, 0.0f },
                    {  he.X + sb->Offset.X,  he.Y + sb->Offset.Y, 0.0f },
                    { -he.X + sb->Offset.X,  he.Y + sb->Offset.Y, 0.0f }
                };
                for (auto c : corners) {
                    const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                    minPt.x = std::min(minPt.x, hc.X);
                    minPt.y = std::min(minPt.y, hc.Y);
                    maxPt.x = std::max(maxPt.x, hc.X);
                    maxPt.y = std::max(maxPt.y, hc.Y);
                    any = true;
                }
            }
            // SPRITE
            if (Editor::ECSUtils::HasComponent(&world, e, "SpriteRenderer2D")) {
                const Matrix4x4 m = TransformUtils::MakeTRS(pos, rot, scale);
                const Vector3D corners[4] = {
                    { -0.5f, -0.5f, 0.0f },
                    {  0.5f, -0.5f, 0.0f },
                    {  0.5f,  0.5f, 0.0f },
                    { -0.5f,  0.5f, 0.0f }
                };
                for (auto c : corners) {
                    const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                    minPt.x = std::min(minPt.x, hc.X);
                    minPt.y = std::min(minPt.y, hc.Y);
                    maxPt.x = std::max(maxPt.x, hc.X);
                    maxPt.y = std::max(maxPt.y, hc.Y);
                    any = true;
                }
            }
            // CIRCLE
            if (Editor::ECSUtils::HasComponent(&world, e, "ShapeCircle2D")) {
                const auto* sc = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeCircle2D>(&world, e, "ShapeCircle2D");
                if (!sc) return;
                const glm::vec2 center = { pos.X + sc->Offset.X, pos.Y + sc->Offset.Y };
                const float radius = sc->Radius * ((scale.X + scale.Y) * 0.5f);
                minPt.x = std::min(minPt.x, center.x - radius);
                minPt.y = std::min(minPt.y, center.y - radius);
                maxPt.x = std::max(maxPt.x, center.x + radius);
                maxPt.y = std::max(maxPt.y, center.y + radius);
                any = true;
            }
            // LINE
            if (Editor::ECSUtils::HasComponent(&world, e, "ShapeLine2D")) {
                const auto* sl = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeLine2D>(&world, e, "ShapeLine2D");
                if (!sl) return;
                const glm::vec2 a = { pos.X + sl->A.X, pos.Y + sl->A.Y };
                const glm::vec2 b = { pos.X + sl->B.X, pos.Y + sl->B.Y };
                minPt.x = std::min(minPt.x, std::min(a.x, b.x));
                minPt.y = std::min(minPt.y, std::min(a.y, b.y));
                maxPt.x = std::max(maxPt.x, std::max(a.x, b.x));
                maxPt.y = std::max(maxPt.y, std::max(a.y, b.y));
                any = true;
            }
        };

        // Stack-based traversal to include children
        std::vector<ECS::Entity> stack;
        stack.push_back(root);
        while (!stack.empty()) {
            ECS::Entity e = stack.back(); stack.pop_back();
            process(e);
            world.ForChildren(e, [&](ECS::Entity child) {
                stack.push_back(child);
            });
        }

        if (!any) {
            return false;
        }

        outMin = minPt;
        outMax = maxPt;
        return true;
    }
    // Helper to get effective transform (WorldTransform if available, otherwise LocalTransform)
    static void GetRenderTransform(ECS::World& world, const ECS::Entity entity,
        const ECS::Components::LocalTransform& lt,
        Vector3D& outPosition, Quaternion& outRotation, Vector3D& outScale) {
        if (Editor::ECSUtils::HasComponent(&world, entity, "WorldTransform")) {
            const auto* wt = Editor::ECSUtils::GetComponentPtr<ECS::Components::WorldTransform>(&world, entity, "WorldTransform");
            if (wt) {
                TransformUtils::DecomposeTRS(wt->Matrix, outPosition, outRotation, outScale);
            }
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
        float windowWidth,
        float windowHeight)
    {
        (void)windowWidth;
        if (selectedEntityID == ECS::Entity::NPOS32 || !renderer || !shader) return;

        ECS::Entity entity = world.Resolve(selectedEntityID);
        if (entity.IsNull() || !world.IsAlive(entity)) return;
        if (!Editor::ECSUtils::HasComponent(&world, entity, "LocalTransform")) return;

        // Skip inactive entities
        if (Editor::ECSUtils::HasComponent(&world, entity, "Active")) {
            const auto* active = Editor::ECSUtils::GetComponentPtr<ECS::Components::Active>(&world, entity, "Active");
            if (active && !active->Enabled) {
                return;
            }
        }

        // Calculate 2px thick outline in screen space
        const float desiredPx = 2.0f;
        const float worldThickness = (cameraOrthoSize / windowHeight) * desiredPx;
        const glm::vec4 selColor(1.0f, 0.85f, 0.15f, 1.0f); // Yellow-ish

        // Setup rendering
        shader->use();
        shader->setMat4("uViewProj", viewProj);
        renderer->beginFrame();

        // Get transform
        const auto* lt = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(&world, entity, "LocalTransform");
        if (!lt) return;
        Vector3D position, scale;
        Quaternion rotation;
        GetRenderTransform(world, entity, *lt, position, rotation, scale);

        // Reusable buffers
        std::vector<glm::vec2> transformedCorners;
        std::vector<glm::vec2> polyPoints;

        // 1) BOXES
        if (Editor::ECSUtils::HasComponent(&world, entity, "ShapeBox2D"))
        {
            const auto* sb = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeBox2D>(&world, entity, "ShapeBox2D");
            if (!sb) return;
            const float rotAngle = 2.0f * std::acos(rotation.W);
            const bool rotated = std::abs(rotAngle) > 0.01f;

            if (!rotated)
            {
                const glm::vec2 halfExtents = {
                    sb->HalfExtents.X * scale.X,
                    sb->HalfExtents.Y * scale.Y
                };
                const glm::vec2 center = {
                    position.X + sb->Offset.X,
                    position.Y + sb->Offset.Y
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

                const Vector2D he = sb->HalfExtents;
                const Vector3D corners[4] = {
                    { -he.X, -he.Y, 0.0f },
                    {  he.X, -he.Y, 0.0f },
                    {  he.X,  he.Y, 0.0f },
                    { -he.X,  he.Y, 0.0f }
                };

                for (auto c : corners)
                {
                    const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                    transformedCorners.push_back(glm::vec2(hc.X + sb->Offset.X, hc.Y + sb->Offset.Y));
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
        else if (Editor::ECSUtils::HasComponent(&world, entity, "SpriteRenderer2D"))
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
        else if (Editor::ECSUtils::HasComponent(&world, entity, "ShapeCircle2D"))
        {
            const auto* sc = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeCircle2D>(&world, entity, "ShapeCircle2D");
            if (!sc) return;
            const glm::vec2 center = {
                position.X + sc->Offset.X,
                position.Y + sc->Offset.Y
            };
            const float radius = sc->Radius * ((scale.X + scale.Y) * 0.5f);
            const glm::vec2 half = { radius, radius };
            const glm::vec2 min = center - half;
            const glm::vec2 max = center + half;

            DebugDraw2D::RectStroke(*renderer, min, max, worldThickness, selColor, 0);
        }
        // 4) LINES
        else if (Editor::ECSUtils::HasComponent(&world, entity, "ShapeLine2D"))
        {
            const auto* sl = Editor::ECSUtils::GetComponentPtr<ECS::Components::ShapeLine2D>(&world, entity, "ShapeLine2D");
            if (!sl) return;
            const glm::vec2 a = { position.X + sl->A.X, position.Y + sl->A.Y };
            const glm::vec2 b = { position.X + sl->B.X, position.Y + sl->B.Y };

            DebugDraw2D::Line(*renderer, a, b, worldThickness, selColor, 0);
        }

        // Additionally draw an axis-aligned bounding box (AABB) covering the selected entity and its children.
        // This gives a clear wireframe rectangle even if the object is rotated or composed of multiple parts.
        glm::vec2 aabbMin, aabbMax;
        if (ComputeWorldAABB(world, entity, aabbMin, aabbMax)) {
            DebugDraw2D::RectStroke(*renderer, aabbMin, aabbMax, worldThickness, selColor, 0);

            // Draw corner markers and log their projected positions for debugging
            const glm::vec4 cornersWorld[4] = {
                { aabbMin.x, aabbMin.y, 0.0f, 1.0f },
                { aabbMax.x, aabbMin.y, 0.0f, 1.0f },
                { aabbMax.x, aabbMax.y, 0.0f, 1.0f },
                { aabbMin.x, aabbMax.y, 0.0f, 1.0f }
            };

            for (int i = 0; i < 4; ++i) {
                const glm::vec4 wc = cornersWorld[i];
                const glm::vec4 clip = viewProj * wc;
                if (clip.w != 0.0f) {
                    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
                }

                // Draw a visible marker at corner (use pixel->unit conversion for size)
                const float markerPx = 8.0f;
                const float markerUnits = PixelsToUnits(markerPx);
                DebugDraw2D::Point(*renderer, glm::vec2(wc.x, wc.y), markerUnits, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f), 0);
            }
        }

        renderer->endFrame();
    }

}
