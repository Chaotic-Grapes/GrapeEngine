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
#include "ecs/systems/GUIRenderUtils.h"
#include "ecs/systems/RendererSystem.h"
#include "graphics/renderer.hpp"

namespace ECS {
    void GUILayoutSystem::OnCreate(World& world) {
        (void)world;
    }

    /**
     * @brief Compute the scale factor for the GUI canvas based on the viewport size and scale mode.
     * @param canvas The GUICanvas component defining reference size and scale mode.
     * @param viewportSize The current size of the GUI viewport.
     * @return The computed scale factor as a Vector2D.
     */
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

    /**
     * @brief Align an anchor point within a rectangle based on the specified alignment.
     * @param origin The top-left origin of the rectangle.
     * @param size The size of the rectangle.
     * @param alignment The desired alignment.
     * @return The computed anchor point position.
     */
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

    static void NormalizeAnchorRange(Vector2D& anchorMin, Vector2D& anchorMax) {
        anchorMin.X = std::clamp(anchorMin.X, 0.0f, 1.0f);
        anchorMin.Y = std::clamp(anchorMin.Y, 0.0f, 1.0f);
        anchorMax.X = std::clamp(anchorMax.X, 0.0f, 1.0f);
        anchorMax.Y = std::clamp(anchorMax.Y, 0.0f, 1.0f);
        if (anchorMin.X > anchorMax.X) std::swap(anchorMin.X, anchorMax.X);
        if (anchorMin.Y > anchorMax.Y) std::swap(anchorMin.Y, anchorMax.Y);
    }

    static bool TryResolveInheritedCanvas(const World& world, Entity entity, Components::GUICanvas& outCanvas) {
        Entity current = entity;
        int depth = 0;
        while (!current.IsNull() && depth < 64) {
            if (world.Has<Components::GUICanvas>(current)) {
                outCanvas = world.Get<Components::GUICanvas>(current);
                return true;
            }
            if (!world.Has<Components::Parent>(current)) {
                break;
            }
            const auto& parent = world.Get<Components::Parent>(current);
            current = parent.ParentEntity;
            if (current.IsNull() || !world.IsAlive(current)) {
                break;
            }
            ++depth;
        }
        return false;
    }

    static Entity FindAnchorParentEntity(const World& world, Entity entity, Components::GUIRenderSpace targetSpace) {
        Entity current = entity;
        int depth = 0;
        while (!current.IsNull() && depth < 64) {
            if (!world.Has<Components::Parent>(current)) {
                break;
            }
            const auto& parent = world.Get<Components::Parent>(current);
            current = parent.ParentEntity;
            if (current.IsNull() || !world.IsAlive(current)) {
                break;
            }
            if (world.Has<Components::GUIElement>(current) &&
                ResolveGUIRenderSpace(world, current) == targetSpace) {
                return current;
            }
            ++depth;
        }
        return NULL_ENTITY;
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

        // Cache a default canvas; elements may still inherit a nearer one from hierarchy.
        Components::GUICanvas defaultCanvas{};
        bool foundCanvas = false;
        world.Each<Components::GUICanvas>([&](Entity, const Components::GUICanvas& c) {
            if (!foundCanvas) {
                defaultCanvas = c;
                foundCanvas = true;
            }
        });
        if (!foundCanvas) {
            defaultCanvas.ReferenceSize = viewport.Size;
            defaultCanvas.Offset = { 0.0f, 0.0f };
            defaultCanvas.ScaleMode = Components::GUIScaleMode::Fit;
        }

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
                worldSpace = (ResolveGUIRenderSpace(world, entity) == Components::GUIRenderSpace::World);
            }

            // World-space GUI
            if (worldSpace) {
                const glm::vec3 right3 = cameraRight;
                const glm::vec3 down3 = -cameraUp;
                const Vector2D basisRight{ right3.x, right3.y };
                const Vector2D basisDown{ down3.x, down3.y };

                // Helper to offset a 2D point in world space along the camera basis directions.
                auto offsetWorld2 = [&](const Vector2D& origin, float dx, float dy) {
                    return Vector2D{
                        origin.X + basisRight.X * dx + basisDown.X * dy,
                        origin.Y + basisRight.Y * dx + basisDown.Y * dy
                    };
                };
                // Helper to offset a 3D point in world space along the camera basis directions.
                auto offsetWorld3 = [&](const glm::vec3& origin, float dx, float dy) {
                    return origin + right3 * dx + down3 * dy;
                };
                // Helper to align an anchor point in world space based on the element's alignment setting.
                auto alignAnchorWorld = [&](const Vector2D& origin, const Vector2D& size, Components::GUIAlignment alignment) {
                    float xFactor = 0.0f;
                    float yFactor = 0.0f;
                    switch (alignment) {
                    case Components::GUIAlignment::Top:
                        xFactor = 0.5f; yFactor = 0.0f; break;
                    case Components::GUIAlignment::TopRight:
                        xFactor = 1.0f; yFactor = 0.0f; break;
                    case Components::GUIAlignment::Left:
                        xFactor = 0.0f; yFactor = 0.5f; break;
                    case Components::GUIAlignment::Center:
                        xFactor = 0.5f; yFactor = 0.5f; break;
                    case Components::GUIAlignment::Right:
                        xFactor = 1.0f; yFactor = 0.5f; break;
                    case Components::GUIAlignment::BottomLeft:
                        xFactor = 0.0f; yFactor = 1.0f; break;
                    case Components::GUIAlignment::Bottom:
                        xFactor = 0.5f; yFactor = 1.0f; break;
                    case Components::GUIAlignment::BottomRight:
                        xFactor = 1.0f; yFactor = 1.0f; break;
                    case Components::GUIAlignment::TopLeft:
                    default:
                        xFactor = 0.0f; yFactor = 0.0f; break;
                    }
                    return offsetWorld2(origin, size.X * xFactor, size.Y * yFactor);
                };

                // Determine parent/canvas anchor rect in world space.
                Vector2D anchorBaseOrigin{ 0.0f, 0.0f };
                Vector2D anchorBaseSize{ 0.0f, 0.0f };
                bool hasParentAnchor = false;
                const bool hasTransform = world.Has<Components::WorldTransform>(entity)
                    || world.Has<Components::LocalTransform>(entity);
                Vector3D baseWorld = hasTransform ? getWorldPosition(entity) : Vector3D{ 0.0f, 0.0f, 0.0f };
                glm::vec3 baseWorld3{ baseWorld.X, baseWorld.Y, baseWorld.Z };

                // Resolve against nearest GUIElement ancestor in the same render space.
                const Entity parentAnchorEntity = FindAnchorParentEntity(world, entity, Components::GUIRenderSpace::World);
                if (!parentAnchorEntity.IsNull()) {
                    self(self, parentAnchorEntity);
                    const auto& parentElement = world.Get<Components::GUIElement>(parentAnchorEntity);
                    anchorBaseOrigin = parentElement.ContentPosition;
                    anchorBaseSize = parentElement.ContentSize;
                    hasParentAnchor = true;
                    if (!hasTransform) {
                        baseWorld = getWorldPosition(parentAnchorEntity);
                        baseWorld3 = glm::vec3(baseWorld.X, baseWorld.Y, baseWorld.Z);
                    }
                }

                // If no valid parent anchor, use world position as origin with zero size (i.e. align to world position based on element alignment).
                if (!hasParentAnchor) {
                    anchorBaseOrigin = { baseWorld.X, baseWorld.Y };
                    anchorBaseSize = { 0.0f, 0.0f };
                }

                Vector2D anchorMin = element.AnchorMin;
                Vector2D anchorMax = element.AnchorMax;
                NormalizeAnchorRange(anchorMin, anchorMax);
                const Vector2D anchorOrigin = offsetWorld2(
                    anchorBaseOrigin,
                    anchorBaseSize.X * anchorMin.X,
                    anchorBaseSize.Y * anchorMin.Y
                );
                const Vector2D anchorSize = {
                    anchorBaseSize.X * (anchorMax.X - anchorMin.X),
                    anchorBaseSize.Y * (anchorMax.Y - anchorMin.Y)
                };

                // World-space GUI uses pixel-authored values converted into world units.
                const float marginLeft = PixelsToUnits(element.Margin.X);
                const float marginRight = PixelsToUnits(element.Margin.Z);
                const float marginTop = PixelsToUnits(element.Margin.Y);
                const float marginBottom = PixelsToUnits(element.Margin.W);
                const float paddingLeft = PixelsToUnits(element.Padding.X);
                const float paddingRight = PixelsToUnits(element.Padding.Z);
                const float paddingTop = PixelsToUnits(element.Padding.Y);
                const float paddingBottom = PixelsToUnits(element.Padding.W);
                const float positionX = PixelsToUnits(element.Position.X);
                const float positionY = PixelsToUnits(element.Position.Y);

                // Calculate size after margins.
                Vector2D size = { PixelsToUnits(element.Size.X), PixelsToUnits(element.Size.Y) };
                size.X = std::max(0.0f, size.X - marginLeft - marginRight);
                size.Y = std::max(0.0f, size.Y - marginTop - marginBottom);

                // Calculate anchor point in world space based on alignment, then offset by element position.
                const Vector2D anchor = alignAnchorWorld(anchorOrigin, anchorSize, element.Alignment);
                Vector2D position = offsetWorld2(anchor, positionX, positionY);
                glm::vec3 position3 = offsetWorld3(glm::vec3(anchor.X, anchor.Y, baseWorld3.z), positionX, positionY);
                switch (element.Alignment) {
                case Components::GUIAlignment::Top:
                    position = offsetWorld2(position, -size.X * 0.5f + (marginLeft - marginRight) * 0.5f, marginTop);
                    position3 = offsetWorld3(position3, -size.X * 0.5f + (marginLeft - marginRight) * 0.5f, marginTop);
                    break;
                case Components::GUIAlignment::TopRight:
                    position = offsetWorld2(position, -size.X - marginRight, marginTop);
                    position3 = offsetWorld3(position3, -size.X - marginRight, marginTop);
                    break;
                case Components::GUIAlignment::Left:
                    position = offsetWorld2(position, marginLeft, -size.Y * 0.5f + (marginTop - marginBottom) * 0.5f);
                    position3 = offsetWorld3(position3, marginLeft, -size.Y * 0.5f + (marginTop - marginBottom) * 0.5f);
                    break;
                case Components::GUIAlignment::Center:
                    position = offsetWorld2(position,
                        -size.X * 0.5f + (marginLeft - marginRight) * 0.5f,
                        -size.Y * 0.5f + (marginTop - marginBottom) * 0.5f);
                    position3 = offsetWorld3(position3,
                        -size.X * 0.5f + (marginLeft - marginRight) * 0.5f,
                        -size.Y * 0.5f + (marginTop - marginBottom) * 0.5f);
                    break;
                case Components::GUIAlignment::Right:
                    position = offsetWorld2(position,
                        -size.X - marginRight,
                        -size.Y * 0.5f + (marginTop - marginBottom) * 0.5f);
                    position3 = offsetWorld3(position3,
                        -size.X - marginRight,
                        -size.Y * 0.5f + (marginTop - marginBottom) * 0.5f);
                    break;
                case Components::GUIAlignment::BottomLeft:
                    position = offsetWorld2(position, marginLeft, -size.Y - marginBottom);
                    position3 = offsetWorld3(position3, marginLeft, -size.Y - marginBottom);
                    break;
                case Components::GUIAlignment::Bottom:
                    position = offsetWorld2(position,
                        -size.X * 0.5f + (marginLeft - marginRight) * 0.5f,
                        -size.Y - marginBottom);
                    position3 = offsetWorld3(position3,
                        -size.X * 0.5f + (marginLeft - marginRight) * 0.5f,
                        -size.Y - marginBottom);
                    break;
                case Components::GUIAlignment::BottomRight:
                    position = offsetWorld2(position, -size.X - marginRight, -size.Y - marginBottom);
                    position3 = offsetWorld3(position3, -size.X - marginRight, -size.Y - marginBottom);
                    break;
                case Components::GUIAlignment::TopLeft:
                default:
                    position = offsetWorld2(position, marginLeft, marginTop);
                    position3 = offsetWorld3(position3, marginLeft, marginTop);
                    break;
                }

                // Cache resolved rect for rendering and input.
                element.ResolvedPosition = position;
                element.ResolvedSize = size;
                const Vector2D contentPosition = offsetWorld2(position, paddingLeft, paddingTop);
                element.ContentPosition = contentPosition;
                element.ContentSize = {
                    std::max(0.0f, size.X - paddingLeft - paddingRight),
                    std::max(0.0f, size.Y - paddingTop - paddingBottom)
                };

                // Project world-space rect to screen-space for input.
                Vector2D screen0{}, screen1{}, screen2{}, screen3{};
                const glm::vec3 topLeft = position3;
                const glm::vec3 topRight = position3 + right3 * size.X;
                const glm::vec3 bottomLeft = position3 + down3 * size.Y;
                const glm::vec3 bottomRight = position3 + right3 * size.X + down3 * size.Y;
                const bool projected =
                    worldToScreen(Vector3D{ topLeft.x, topLeft.y, topLeft.z }, screen0) &&
                    worldToScreen(Vector3D{ topRight.x, topRight.y, topRight.z }, screen1) &&
                    worldToScreen(Vector3D{ bottomLeft.x, bottomLeft.y, bottomLeft.z }, screen2) &&
                    worldToScreen(Vector3D{ bottomRight.x, bottomRight.y, bottomRight.z }, screen3);
                
                // Compute axis-aligned bounding box of the projected corners for screen-space rect, or set to zero if projection failed (e.g. behind camera).
                if (projected) {
                    const float minX = std::min(std::min(screen0.X, screen1.X), std::min(screen2.X, screen3.X));
                    const float minY = std::min(std::min(screen0.Y, screen1.Y), std::min(screen2.Y, screen3.Y));
                    const float maxX = std::max(std::max(screen0.X, screen1.X), std::max(screen2.X, screen3.X));
                    const float maxY = std::max(std::max(screen0.Y, screen1.Y), std::max(screen2.Y, screen3.Y));
                    element.ScreenPosition = { minX, minY };
                    element.ScreenSize = { std::max(0.0f, maxX - minX), std::max(0.0f, maxY - minY) };
                } else { // If projection failed (e.g. behind camera), set screen rect to zero to avoid input hits.
                    element.ScreenPosition = { 0.0f, 0.0f };
                    element.ScreenSize = { 0.0f, 0.0f };
                }
            } else {
                Components::GUICanvas activeCanvas = defaultCanvas;
                (void)TryResolveInheritedCanvas(world, entity, activeCanvas);

                const Vector2D scale = ComputeScale(activeCanvas, viewport.Size);
                const Vector2D canvasContentSize = {
                    activeCanvas.ReferenceSize.X * scale.X,
                    activeCanvas.ReferenceSize.Y * scale.Y
                };
                const Vector2D canvasContentOrigin = {
                    viewport.Origin.X + (viewport.Size.X - canvasContentSize.X) * 0.5f + activeCanvas.Offset.X,
                    viewport.Origin.Y + (viewport.Size.Y - canvasContentSize.Y) * 0.5f + activeCanvas.Offset.Y
                };

                // Default anchor origin and size come from canvas space.
                Vector2D anchorBaseOrigin = canvasContentOrigin;
                Vector2D anchorBaseSize = canvasContentSize;

                // Resolve against nearest GUIElement ancestor in the same render space.
                const Entity parentAnchorEntity = FindAnchorParentEntity(world, entity, Components::GUIRenderSpace::Screen);
                if (!parentAnchorEntity.IsNull()) {
                    self(self, parentAnchorEntity);
                    const auto& parentElement = world.Get<Components::GUIElement>(parentAnchorEntity);
                    anchorBaseOrigin = parentElement.ContentPosition;
                    anchorBaseSize = parentElement.ContentSize;
                }

                Vector2D anchorMin = element.AnchorMin;
                Vector2D anchorMax = element.AnchorMax;
                NormalizeAnchorRange(anchorMin, anchorMax);
                const Vector2D anchorOrigin = {
                    anchorBaseOrigin.X + anchorBaseSize.X * anchorMin.X,
                    anchorBaseOrigin.Y + anchorBaseSize.Y * anchorMin.Y
                };
                const Vector2D anchorSize = {
                    anchorBaseSize.X * (anchorMax.X - anchorMin.X),
                    anchorBaseSize.Y * (anchorMax.Y - anchorMin.Y)
                };

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
                element.ScreenPosition = position;
                element.ScreenSize = size;
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
