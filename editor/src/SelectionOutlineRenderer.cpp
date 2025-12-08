/* Start Header *****************************************************************/
/*!
\file   SelectionOutlineRenderer.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\brief
Implements editor-side selection wireframe rendering using RendererSystem APIs.

Uses the engine's wireframe submission API (SubmitWireframeQuad, SubmitWireframeCircle, etc.)
to decouple editor rendering from low-level graphics details. This follows industry-standard
practice of keeping editor code separate from engine code.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "SelectionOutlineRenderer.h"
#include "core/Logger.h"
#include "ecs/Entity.h"
#include "ecs/Components.h"
#include "ecs/systems/RendererSystem.h"
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
            if (!world.Has<ECS::Components::LocalTransform>(e)) return;

            const auto& lt = world.Get<ECS::Components::LocalTransform>(e);
            Vector3D pos; Vector3D scale; Quaternion rot;
            GetRenderTransform(world, e, lt, pos, rot, scale);

            // BOX
            if (world.Has<ECS::Components::ShapeBox2D>(e)) {
                const auto& sb = world.Get<ECS::Components::ShapeBox2D>(e);
                const Matrix4x4 m = TransformUtils::MakeTRS(pos, rot, scale);
                const Vector2D he = sb.HalfExtents;
                const Vector3D corners[4] = {
                    { -he.X + sb.Offset.X, -he.Y + sb.Offset.Y, 0.0f },
                    {  he.X + sb.Offset.X, -he.Y + sb.Offset.Y, 0.0f },
                    {  he.X + sb.Offset.X,  he.Y + sb.Offset.Y, 0.0f },
                    { -he.X + sb.Offset.X,  he.Y + sb.Offset.Y, 0.0f }
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
            if (world.Has<ECS::Components::SpriteRenderer2D>(e)) {
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
            if (world.Has<ECS::Components::ShapeCircle2D>(e)) {
                const auto& sc = world.Get<ECS::Components::ShapeCircle2D>(e);
                const glm::vec2 center = { pos.X + sc.Offset.X, pos.Y + sc.Offset.Y };
                const float radius = sc.Radius * ((scale.X + scale.Y) * 0.5f);
                minPt.x = std::min(minPt.x, center.x - radius);
                minPt.y = std::min(minPt.y, center.y - radius);
                maxPt.x = std::max(maxPt.x, center.x + radius);
                maxPt.y = std::max(maxPt.y, center.y + radius);
                any = true;
            }
            // POLYGON
            if (world.Has<ECS::Components::ShapePolygon2D<32>>(e)) {
                const auto& pl = world.Get<ECS::Components::ShapePolygon2D<32>>(e);
                if (pl.Count >= 1) {
                    const auto m = TransformUtils::MakeTRS(pos, rot, scale);
                    for (uint32_t i = 0; i < pl.Count; ++i) {
                        const Vector3D p3{ pl.Points[i].X, pl.Points[i].Y, 0.0f };
                        const Vector4D hp = m * Vector4D{ p3.X, p3.Y, p3.Z, 1.0f };
                        minPt.x = std::min(minPt.x, hp.X);
                        minPt.y = std::min(minPt.y, hp.Y);
                        maxPt.x = std::max(maxPt.x, hp.X);
                        maxPt.y = std::max(maxPt.y, hp.Y);
                        any = true;
                    }
                }
            }
            // LINE
            if (world.Has<ECS::Components::ShapeLine2D>(e)) {
                const auto& sl = world.Get<ECS::Components::ShapeLine2D>(e);
                const glm::vec2 a = { pos.X + sl.A.X, pos.Y + sl.A.Y };
                const glm::vec2 b = { pos.X + sl.B.X, pos.Y + sl.B.Y };
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
        ECS::RendererSystem* rendererSystem,
        float cameraOrthoSize,
        float windowHeight)
    {
        if (selectedEntityID == 0 || !rendererSystem) return;

        ECS::Entity entity = world.Resolve(selectedEntityID);
        if (entity.IsNull() || !world.IsAlive(entity)) return;
        if (!world.Has<ECS::Components::LocalTransform>(entity)) return;

        LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Rendering outline for entity " << entity.Index);

        // Skip inactive entities (if Active component exists and is disabled)
        if (world.Has<ECS::Components::Active>(entity) &&
            !world.Get<ECS::Components::Active>(entity).Enabled) {
            LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Entity " << entity.Index << " is inactive, skipping");
            return;
        }

        // Calculate pixel-perfect thickness (industry-standard)
        const float desiredPx = 2.0f;
        const float worldThickness = (cameraOrthoSize / windowHeight) * desiredPx;
        const glm::vec4 selColor(1.0f, 0.85f, 0.15f, 1.0f); // Yellow-ish

        LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] worldThickness=" << worldThickness << 
                  " cameraOrthoSize=" << cameraOrthoSize << " windowHeight=" << windowHeight);

        // Get entity transform
        const auto& lt = world.Get<ECS::Components::LocalTransform>(entity);
        Vector3D position, scale;
        Quaternion rotation;
        GetRenderTransform(world, entity, lt, position, rotation, scale);

        LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Entity transform: pos=(" << position.X << "," 
                  << position.Y << ") scale=(" << scale.X << "," << scale.Y << ")");

        bool hasShape = false;

        // Render individual shape outlines
        if (world.Has<ECS::Components::ShapeBox2D>(entity)) {
            LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Rendering ShapeBox2D");
            hasShape = true;
            const auto& sb = world.Get<ECS::Components::ShapeBox2D>(entity);
            
            // Extract Z-axis rotation angle properly from quaternion
            const float angleZ = std::atan2(
                2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
            );
            const bool rotated = std::abs(angleZ) > 0.001f;

            if (!rotated) {
                // Optimization: Use axis-aligned rect for non-rotated boxes
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

                rendererSystem->SubmitWireframeQuad(min, max, selColor, worldThickness);
            }
            else {
                // For rotated boxes, apply full transform matrix
                const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                std::vector<glm::vec2> transformedCorners;
                transformedCorners.reserve(4);

                const Vector2D he = sb.HalfExtents;
                const Vector3D corners[4] = {
                    { -he.X + sb.Offset.X, -he.Y + sb.Offset.Y, 0.0f },
                    {  he.X + sb.Offset.X, -he.Y + sb.Offset.Y, 0.0f },
                    {  he.X + sb.Offset.X,  he.Y + sb.Offset.Y, 0.0f },
                    { -he.X + sb.Offset.X,  he.Y + sb.Offset.Y, 0.0f }
                };

                for (auto c : corners) {
                    const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                    transformedCorners.push_back(glm::vec2(hc.X, hc.Y));
                }

                rendererSystem->SubmitWireframePolygon(transformedCorners.data(), transformedCorners.size(), 
                                                       selColor, worldThickness, true);
            }
        }
        else if (world.Has<ECS::Components::SpriteRenderer2D>(entity)) {
            LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Rendering SpriteRenderer2D");
            hasShape = true;
            
            // Extract Z-axis rotation using proper quaternion formula
            const float angleZ = std::atan2(
                2.0f * (rotation.W * rotation.Z + rotation.X * rotation.Y),
                1.0f - 2.0f * (rotation.Y * rotation.Y + rotation.Z * rotation.Z)
            );
            const bool rotated = std::abs(angleZ) > 0.001f;

            if (!rotated) {
                // Optimization: Fast path for axis-aligned sprites
                const glm::vec2 half = { scale.X * 0.5f, scale.Y * 0.5f };
                const glm::vec2 min = { position.X - half.x, position.Y - half.y };
                const glm::vec2 max = { position.X + half.x, position.Y + half.y };

                rendererSystem->SubmitWireframeQuad(min, max, selColor, worldThickness);
            }
            else {
                // For rotated sprites, apply full transform
                const Matrix4x4 m = TransformUtils::MakeTRS(position, rotation, scale);
                std::vector<glm::vec2> transformedCorners;
                transformedCorners.reserve(4);

                const Vector3D corners[4] = {
                    { -0.5f, -0.5f, 0.0f },
                    {  0.5f, -0.5f, 0.0f },
                    {  0.5f,  0.5f, 0.0f },
                    { -0.5f,  0.5f, 0.0f }
                };

                for (auto c : corners) {
                    const Vector4D hc = m * Vector4D{ c.X, c.Y, c.Z, 1.0f };
                    transformedCorners.push_back(glm::vec2(hc.X, hc.Y));
                }

                rendererSystem->SubmitWireframePolygon(transformedCorners.data(), transformedCorners.size(),
                                                       selColor, worldThickness, true);
            }
        }
        else if (world.Has<ECS::Components::ShapeCircle2D>(entity)) {
            LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Rendering ShapeCircle2D");
            hasShape = true;
            const auto& sc = world.Get<ECS::Components::ShapeCircle2D>(entity);
            const glm::vec2 center = {
                position.X + sc.Offset.X,
                position.Y + sc.Offset.Y
            };
            // For uniform scaling, use average
            const float radius = sc.Radius * ((scale.X + scale.Y) * 0.5f);

            rendererSystem->SubmitWireframeCircle(center, radius, selColor, worldThickness);
        }
        else if (world.Has<ECS::Components::ShapePolygon2D<32>>(entity)) {
            LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Rendering ShapePolygon2D");
            hasShape = true;
            const auto& pl = world.Get<ECS::Components::ShapePolygon2D<32>>(entity);
            if (pl.Count >= 2) {
                const auto m = TransformUtils::MakeTRS(position, rotation, scale);
                std::vector<glm::vec2> polyPoints;
                polyPoints.reserve(pl.Count);

                for (uint32_t i = 0; i < pl.Count; ++i) {
                    const Vector3D p3{ pl.Points[i].X, pl.Points[i].Y, 0.0f };
                    const Vector4D hp = m * Vector4D{ p3.X, p3.Y, p3.Z, 1.0f };
                    polyPoints.push_back(glm::vec2(hp.X, hp.Y));
                }

                rendererSystem->SubmitWireframePolygon(polyPoints.data(), polyPoints.size(),
                                                       selColor, worldThickness, true);
            }
        }
        else if (world.Has<ECS::Components::ShapeLine2D>(entity)) {
            LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Rendering ShapeLine2D");
            hasShape = true;
            const auto& sl = world.Get<ECS::Components::ShapeLine2D>(entity);
            const glm::vec2 a = { position.X + sl.A.X, position.Y + sl.A.Y };
            const glm::vec2 b = { position.X + sl.B.X, position.Y + sl.B.Y };

            rendererSystem->SubmitWireframeLine(a, b, selColor, worldThickness);
        }

        // Draw AABB covering the selected entity and its children
        glm::vec2 aabbMin, aabbMax;
        if (ComputeWorldAABB(world, entity, aabbMin, aabbMax)) {
            LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Drawing AABB: min=(" << aabbMin.x << "," 
                      << aabbMin.y << ") max=(" << aabbMax.x << "," << aabbMax.y << ")");
            rendererSystem->SubmitWireframeQuad(aabbMin, aabbMax, selColor, worldThickness);
        }
        else {
            // Fallback: Draw a small cross at the entity's position
            LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Entity " << entity.Index 
                      << " has no AABB; drawing position marker; hasShape=" << (hasShape ? "true" : "false"));
            const glm::vec2 pos = { position.X, position.Y };
            const float crossSize = worldThickness * 3.0f;
            
            // Draw horizontal and vertical lines
            rendererSystem->SubmitWireframeLine(
                glm::vec2(pos.x - crossSize, pos.y),
                glm::vec2(pos.x + crossSize, pos.y),
                selColor, worldThickness);
            rendererSystem->SubmitWireframeLine(
                glm::vec2(pos.x, pos.y - crossSize),
                glm::vec2(pos.x, pos.y + crossSize),
                selColor, worldThickness);
        }

        LOG_DEBUG("[SelectionOutlineRenderer::RenderOutline] Completed rendering outline");
    }

}
