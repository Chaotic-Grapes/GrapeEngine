/* Start Header *****************************************************************/
/*!
\file    GUILayoutSystem.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implements the GUILayoutSystem which is responsible for calculating
the layout of GUI elements based on their properties and the current viewport.

\note
The computation is inaccurate and possibly incorrect!

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <glm/glm.hpp>
#include "ecs/Components.h"
#include "ecs/systems/GUILayoutSystem.h"
#include "ecs/systems/RendererSystem.h"

namespace ECS {
    void GUILayoutSystem::OnCreate(World& world) {
        (void)world;
    }

    // Compute canvas scale based on reference size and scale mode.
    static Vector2D ComputeScale(const Components::GUICanvas& canvas, const Vector2D& viewportSize) {
        Vector2D scale{ 1.0f, 1.0f };
        if (canvas.ReferenceSize.X <= 0.0f || canvas.ReferenceSize.Y <= 0.0f) {
            return scale;
        }

        // Calculate scale factors.
        const float scaleX = viewportSize.X / canvas.ReferenceSize.X;
        const float scaleY = viewportSize.Y / canvas.ReferenceSize.Y;
        float uniform = 1.0f;
        switch (canvas.ScaleMode) {
        case Components::GUIScaleMode::Fit:
            uniform = std::min(scaleX, scaleY);
            scale = { uniform, uniform };
            break;
        case Components::GUIScaleMode::Fill:
            uniform = std::max(scaleX, scaleY);
            scale = { uniform, uniform };
            break;
        case Components::GUIScaleMode::MatchWidth:
            scale = { scaleX, scaleX };
            break;
        case Components::GUIScaleMode::MatchHeight:
            scale = { scaleY, scaleY };
            break;
        }

        return scale;
    }

    // Resolve anchor position based on alignment within a given rect.
    static Vector2D AlignAnchor(const Vector2D& origin, const Vector2D& size, Components::GUIAlignment alignment) {
        switch (alignment) {
        case Components::GUIAlignment::Top:
            return { origin.X + size.X * 0.5f, origin.Y };
        case Components::GUIAlignment::TopRight:
            return { origin.X + size.X, origin.Y };
        case Components::GUIAlignment::Left:
            return { origin.X, origin.Y + size.Y * 0.5f };
        case Components::GUIAlignment::Center:
            return { origin.X + size.X * 0.5f, origin.Y + size.Y * 0.5f };
        case Components::GUIAlignment::Right:
            return { origin.X + size.X, origin.Y + size.Y * 0.5f };
        case Components::GUIAlignment::BottomLeft:
            return { origin.X, origin.Y + size.Y };
        case Components::GUIAlignment::Bottom:
            return { origin.X + size.X * 0.5f, origin.Y + size.Y };
        case Components::GUIAlignment::BottomRight:
            return { origin.X + size.X, origin.Y + size.Y };
        case Components::GUIAlignment::TopLeft:
        default:
            return origin;
        }
    }

    void GUILayoutSystem::OnUpdate(World& world) {
        auto* renderer = RendererSystem::GetInstance();
        if (!renderer) {
            return;
        }

        // Query the active GUI viewport (editor may override it).
        RendererSystem::GUIViewport viewport = renderer->GetGUIViewport();
        if (!viewport.Active || viewport.Size.X <= 0.0f || viewport.Size.Y <= 0.0f) {
            const Vector2D renderSize = renderer->GetRenderTargetSize();
            viewport.Origin = { 0.0f, 0.0f };
            viewport.Size = renderSize;
        }

        // Use the first canvas found; if none exists, fall back to viewport size.
        Components::GUICanvas canvas{};
        bool foundCanvas = false;
        world.Each<Components::GUICanvas>([&](Entity, const Components::GUICanvas& c) {
            canvas = c;
            foundCanvas = true;
        });
        if (!foundCanvas) {
            canvas.ReferenceSize = viewport.Size;
            canvas.Offset = { 0.0f, 0.0f };
            canvas.ScaleMode = Components::GUIScaleMode::Fit;
        }

        // Compute scale from reference size and scale mode.
        const Vector2D scale = ComputeScale(canvas, viewport.Size);
        const Vector2D contentSize = {
            canvas.ReferenceSize.X * scale.X,
            canvas.ReferenceSize.Y * scale.Y
        };
        const Vector2D contentOrigin = {
            viewport.Origin.X + (viewport.Size.X - contentSize.X) * 0.5f,
            viewport.Origin.Y + (viewport.Size.Y - contentSize.Y) * 0.5f
        };

        // Cache camera basis for world-space GUI layout.
        glm::vec3 cameraRight(1.0f, 0.0f, 0.0f);
        glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
        const bool hasCameraBasis = renderer->GetCameraBasis(world, cameraRight, cameraUp);
        auto worldToScreen = [&](const Vector3D& worldPos, Vector2D& outScreen) {
            return renderer->WorldToScreen(world, worldPos, viewport.Origin, viewport.Size, outScreen);
        };
        auto getWorldPosition = [&](Entity entity) {
            if (world.Has<Components::WorldTransform>(entity)) {
                const auto& wt = world.Get<Components::WorldTransform>(entity);
                return Vector3D{ wt.Matrix.m03, wt.Matrix.m13, wt.Matrix.m23 };
            }
            if (world.Has<Components::LocalTransform>(entity)) {
                return world.Get<Components::LocalTransform>(entity).Position;
            }
            return Vector3D{ 0.0f, 0.0f, 0.0f };
        };

        // Collect GUI elements for recursive layout.
        std::vector<Entity> elements;
        world.Each<Components::GUIElement>([&](Entity entity, Components::GUIElement&) {
            elements.push_back(entity);
        });

        std::unordered_map<Entity, bool, EntityHash> resolved;   // cache layout completion per element
        std::unordered_set<Entity, EntityHash> resolving;        // cycle guard for parent chains

        auto resolveRenderSpace = [&](Entity entity) {
            Entity current = entity;
            int depth = 0;
            while (!current.IsNull() && depth < 32) {
                if (world.Has<Components::GUIRenderMode>(current)) {
                    return world.Get<Components::GUIRenderMode>(current).Space;
                }
                if (!world.Has<Components::Parent>(current)) {
                    break;
                }
                const auto& parent = world.Get<Components::Parent>(current);
                current = parent.ParentEntity;
                if (current.IsNull() || !world.IsAlive(current) || !world.Has<Components::GUIElement>(current)) {
                    break;
                }
                ++depth;
            }
            return Components::GUIRenderSpace::Screen;
        };

        auto resolveElement = [&](auto&& self, Entity entity) -> void {
            // Skip elements already computed this frame.
            if (resolved[entity]) {
                return;
            }
            // Guard against recursive parent loops.
            if (resolving.count(entity) > 0) {
                return;
            }

            // If has parent, resolve parent first.
            resolving.insert(entity);
            auto& element = world.Get<Components::GUIElement>(entity);
            bool worldSpace = false;
            if (hasCameraBasis) {
                worldSpace = (resolveRenderSpace(entity) == Components::GUIRenderSpace::World);
            }

            // World-space GUI
            if (worldSpace) {
                Vector2D anchorOrigin{ 0.0f, 0.0f };
                Vector2D anchorSize{ 0.0f, 0.0f };
                bool hasParentAnchor = false;
                const bool hasTransform = world.Has<Components::WorldTransform>(entity)
                    || world.Has<Components::LocalTransform>(entity);
                Vector3D baseWorld = hasTransform ? getWorldPosition(entity) : Vector3D{ 0.0f, 0.0f, 0.0f };

                if (world.Has<Components::Parent>(entity)) {
                    const auto& parent = world.Get<Components::Parent>(entity);
                    const Entity parentEntity = parent.ParentEntity;
                    if (!parentEntity.IsNull() && world.IsAlive(parentEntity) &&
                        world.Has<Components::GUIElement>(parentEntity)) {
                        self(self, parentEntity);
                        const auto& parentElement = world.Get<Components::GUIElement>(parentEntity);
                        anchorOrigin = parentElement.ContentPosition;
                        anchorSize = parentElement.ContentSize;
                        hasParentAnchor = true;
                        if (!hasTransform) {
                            baseWorld = getWorldPosition(parentEntity);
                        }
                    }
                }

                Vector2D baseScreen{};
                if (!worldToScreen(baseWorld, baseScreen)) {
                    element.ResolvedPosition = { 0.0f, 0.0f };
                    element.ResolvedSize = { 0.0f, 0.0f };
                    element.ContentPosition = { 0.0f, 0.0f };
                    element.ContentSize = { 0.0f, 0.0f };
                    resolved[entity] = true;
                    resolving.erase(entity);
                    return;
                }

                if (!hasParentAnchor) {
                    anchorOrigin = baseScreen;
                    anchorSize = { 0.0f, 0.0f };
                }

                // Compute size and margins/padding in screen space.
                auto screenDeltaLength = [&](float dx, float dy) {
                    const Vector3D worldOffset = {
                        baseWorld.X + cameraRight.x * dx + cameraUp.x * dy,
                        baseWorld.Y + cameraRight.y * dx + cameraUp.y * dy,
                        baseWorld.Z + cameraRight.z * dx + cameraUp.z * dy
                    };
                    Vector2D screenPos{};
                    if (!worldToScreen(worldOffset, screenPos)) {
                        return 0.0f;
                    }
                    const float sx = screenPos.X - baseScreen.X;
                    const float sy = screenPos.Y - baseScreen.Y;
                    return std::sqrt(sx * sx + sy * sy);
                };

                // Calculate size, position, margins, and padding in screen space.
                const float marginLeft = screenDeltaLength(element.Margin.X, 0.0f);
                const float marginRight = screenDeltaLength(element.Margin.Z, 0.0f);
                const float marginTop = screenDeltaLength(0.0f, element.Margin.Y);
                const float marginBottom = screenDeltaLength(0.0f, element.Margin.W);
                const float paddingLeft = screenDeltaLength(element.Padding.X, 0.0f);
                const float paddingRight = screenDeltaLength(element.Padding.Z, 0.0f);
                const float paddingTop = screenDeltaLength(0.0f, element.Padding.Y);
                const float paddingBottom = screenDeltaLength(0.0f, element.Padding.W);

                // Calculate size after margins.
                Vector2D size = {
                    screenDeltaLength(element.Size.X, 0.0f),
                    screenDeltaLength(0.0f, element.Size.Y)
                };
                size.X = std::max(0.0f, size.X - marginLeft - marginRight);
                size.Y = std::max(0.0f, size.Y - marginTop - marginBottom);

                const Vector2D anchor = AlignAnchor(anchorOrigin, anchorSize, element.Alignment);
                Vector2D position = {
                    anchor.X + screenDeltaLength(element.Position.X, 0.0f),
                    anchor.Y + screenDeltaLength(0.0f, element.Position.Y)
                };
                switch (element.Alignment) {
                case Components::GUIAlignment::Top:
                    position.X -= size.X * 0.5f;
                    position.X += (marginLeft - marginRight) * 0.5f;
                    position.Y += marginTop;
                    break;
                case Components::GUIAlignment::TopRight:
                    position.X -= size.X;
                    position.X -= marginRight;
                    position.Y += marginTop;
                    break;
                case Components::GUIAlignment::Left:
                    position.Y -= size.Y * 0.5f;
                    position.X += marginLeft;
                    position.Y += (marginTop - marginBottom) * 0.5f;
                    break;
                case Components::GUIAlignment::Center:
                    position.X -= size.X * 0.5f;
                    position.Y -= size.Y * 0.5f;
                    position.X += (marginLeft - marginRight) * 0.5f;
                    position.Y += (marginTop - marginBottom) * 0.5f;
                    break;
                case Components::GUIAlignment::Right:
                    position.X -= size.X;
                    position.Y -= size.Y * 0.5f;
                    position.X -= marginRight;
                    position.Y += (marginTop - marginBottom) * 0.5f;
                    break;
                case Components::GUIAlignment::BottomLeft:
                    position.Y -= size.Y;
                    position.X += marginLeft;
                    position.Y -= marginBottom;
                    break;
                case Components::GUIAlignment::Bottom:
                    position.X -= size.X * 0.5f;
                    position.Y -= size.Y;
                    position.X += (marginLeft - marginRight) * 0.5f;
                    position.Y -= marginBottom;
                    break;
                case Components::GUIAlignment::BottomRight:
                    position.X -= size.X;
                    position.Y -= size.Y;
                    position.X -= marginRight;
                    position.Y -= marginBottom;
                    break;
                case Components::GUIAlignment::TopLeft:
                default:
                    position.X += marginLeft;
                    position.Y += marginTop;
                    break;
                }

                // Cache resolved rect for rendering and input.
                element.ResolvedPosition = position;
                element.ResolvedSize = size;
                element.ContentPosition = { position.X + paddingLeft, position.Y + paddingTop };
                element.ContentSize = {
                    std::max(0.0f, size.X - paddingLeft - paddingRight),
                    std::max(0.0f, size.Y - paddingTop - paddingBottom)
                };
            } else {
                // Default anchor origin and size come from canvas space.
                Vector2D anchorOrigin = { contentOrigin.X + canvas.Offset.X, contentOrigin.Y + canvas.Offset.Y };
                Vector2D anchorSize = contentSize;

                // If has parent, resolve parent first and use its content rect as anchor.
                if (world.Has<Components::Parent>(entity)) {
                    const auto& parent = world.Get<Components::Parent>(entity);
                    const Entity parentEntity = parent.ParentEntity;
                    if (!parentEntity.IsNull() && world.IsAlive(parentEntity) &&
                        world.Has<Components::GUIElement>(parentEntity)) {
                        // Ensure parent is laid out before applying child offsets.
                        self(self, parentEntity);
                        const auto& parentElement = world.Get<Components::GUIElement>(parentEntity);
                        anchorOrigin = parentElement.ContentPosition;
                        anchorSize = parentElement.ContentSize;
                    }
                }

                // Apply scale to size and margin/padding so layout matches rendering.
                Vector2D size = { element.Size.X * scale.X, element.Size.Y * scale.Y };
                Vector4D margin = {
                    element.Margin.X * scale.X,
                    element.Margin.Y * scale.Y,
                    element.Margin.Z * scale.X,
                    element.Margin.W * scale.Y
                };
                Vector4D padding = {
                    element.Padding.X * scale.X,
                    element.Padding.Y * scale.Y,
                    element.Padding.Z * scale.X,
                    element.Padding.W * scale.Y
                };

                // Margins reduce the final size and offset the anchor position.
                size.X = std::max(0.0f, size.X - margin.X - margin.Z);
                size.Y = std::max(0.0f, size.Y - margin.Y - margin.W);

                // Resolve the anchor point based on alignment within the anchor rect.
                Vector2D anchor = AlignAnchor(anchorOrigin, anchorSize, element.Alignment);
                Vector2D position = {
                    anchor.X + element.Position.X * scale.X,
                    anchor.Y + element.Position.Y * scale.Y
                };

                // Offset position by element size for alignments that are not top-left.
                switch (element.Alignment) {
                case Components::GUIAlignment::Top:
                    position.X -= size.X * 0.5f;
                    position.X += (margin.X - margin.Z) * 0.5f;
                    position.Y += margin.Y;
                    break;
                case Components::GUIAlignment::TopRight:
                    position.X -= size.X;
                    position.X -= margin.Z;
                    position.Y += margin.Y;
                    break;
                case Components::GUIAlignment::Left:
                    position.Y -= size.Y * 0.5f;
                    position.X += margin.X;
                    position.Y += (margin.Y - margin.W) * 0.5f;
                    break;
                case Components::GUIAlignment::Center:
                    position.X -= size.X * 0.5f;
                    position.Y -= size.Y * 0.5f;
                    position.X += (margin.X - margin.Z) * 0.5f;
                    position.Y += (margin.Y - margin.W) * 0.5f;
                    break;
                case Components::GUIAlignment::Right:
                    position.X -= size.X;
                    position.Y -= size.Y * 0.5f;
                    position.X -= margin.Z;
                    position.Y += (margin.Y - margin.W) * 0.5f;
                    break;
                case Components::GUIAlignment::BottomLeft:
                    position.Y -= size.Y;
                    position.X += margin.X;
                    position.Y -= margin.W;
                    break;
                case Components::GUIAlignment::Bottom:
                    position.X -= size.X * 0.5f;
                    position.Y -= size.Y;
                    position.X += (margin.X - margin.Z) * 0.5f;
                    position.Y -= margin.W;
                    break;
                case Components::GUIAlignment::BottomRight:
                    position.X -= size.X;
                    position.Y -= size.Y;
                    position.X -= margin.Z;
                    position.Y -= margin.W;
                    break;
                case Components::GUIAlignment::TopLeft:
                default:
                    position.X += margin.X;
                    position.Y += margin.Y;
                    break;
                }

                // Cache resolved rect for rendering and input.
                element.ResolvedPosition = position;
                element.ResolvedSize = size;

                // Content rect excludes padding for child alignment and text layout.
                Vector2D contentPos = { position.X + padding.X, position.Y + padding.Y };
                Vector2D contentSizeFinal = {
                    std::max(0.0f, size.X - padding.X - padding.Z),
                    std::max(0.0f, size.Y - padding.Y - padding.W)
                };
                element.ContentPosition = contentPos;
                element.ContentSize = contentSizeFinal;
            }

            resolved[entity] = true;
            resolving.erase(entity);
        };

        for (const auto& entity : elements) {
            resolveElement(resolveElement, entity);
        }
    }

    void GUILayoutSystem::OnDestroy(World& world) {
        (void)world;
    }

    SystemMetadata GUILayoutSystem::GetMetadata() const {
        ComponentAccessBuilder builder("GUILayoutSystem");
        builder.SetExecutionOrder(-20);
        return builder
            .ReadComponent<Components::GUICanvas>()
            .ReadComponent<Components::GUIRenderMode>()
            .ReadComponent<Components::Parent>()
            .ReadComponent<Components::LocalTransform>()
            .ReadComponent<Components::WorldTransform>()
            .WriteComponent<Components::GUIElement>()
            .Build();
    }
}
