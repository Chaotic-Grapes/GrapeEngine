#include <algorithm>
#include <unordered_map>
#include <unordered_set>
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

            resolving.insert(entity);

            auto& element = world.Get<Components::GUIElement>(entity);
            // Default anchor origin and size come from canvas space.
            Vector2D anchorOrigin = { contentOrigin.X + canvas.Offset.X, contentOrigin.Y + canvas.Offset.Y };
            Vector2D anchorSize = contentSize;

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
                anchor.X + element.Position.X * scale.X + margin.X,
                anchor.Y + element.Position.Y * scale.Y + margin.Y
            };

            // Offset position by element size for alignments that are not top-left.
            switch (element.Alignment) {
            case Components::GUIAlignment::Top:
                position.X -= size.X * 0.5f;
                break;
            case Components::GUIAlignment::TopRight:
                position.X -= size.X;
                break;
            case Components::GUIAlignment::Left:
                position.Y -= size.Y * 0.5f;
                break;
            case Components::GUIAlignment::Center:
                position.X -= size.X * 0.5f;
                position.Y -= size.Y * 0.5f;
                break;
            case Components::GUIAlignment::Right:
                position.X -= size.X;
                position.Y -= size.Y * 0.5f;
                break;
            case Components::GUIAlignment::BottomLeft:
                position.Y -= size.Y;
                break;
            case Components::GUIAlignment::Bottom:
                position.X -= size.X * 0.5f;
                position.Y -= size.Y;
                break;
            case Components::GUIAlignment::BottomRight:
                position.X -= size.X;
                position.Y -= size.Y;
                break;
            case Components::GUIAlignment::TopLeft:
            default:
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
            .ReadComponent<Components::Parent>()
            .WriteComponent<Components::GUIElement>()
            .Build();
    }
}
