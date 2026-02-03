#include "ecs/systems/GUIRenderSystem.h"

#include <algorithm>
#include <string>
#include <vector>
#include "ecs/Components.h"
#include "ecs/systems/RendererSystem.h"

namespace ECS {
    void GUIRenderSystem::OnCreate(World& world) {
        (void)world;
    }

    void GUIRenderSystem::OnUpdate(World& world) {
        auto* renderer = RendererSystem::GetInstance();
        if (!renderer) {
            return;
        }

        RendererSystem::GUIViewport viewport = renderer->GetGUIViewport();
        if (!viewport.Active || viewport.Size.X <= 0.0f || viewport.Size.Y <= 0.0f) {
            const Vector2D renderSize = renderer->GetRenderTargetSize();
            viewport.Origin = { 0.0f, 0.0f };
            viewport.Size = renderSize;
        }

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

        Vector2D scale{ 1.0f, 1.0f };
        if (canvas.ReferenceSize.X > 0.0f && canvas.ReferenceSize.Y > 0.0f) {
            const float scaleX = viewport.Size.X / canvas.ReferenceSize.X;
            const float scaleY = viewport.Size.Y / canvas.ReferenceSize.Y;
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
        }

        const Vector2D contentSize = {
            canvas.ReferenceSize.X * scale.X,
            canvas.ReferenceSize.Y * scale.Y
        };
        const Vector2D contentOrigin = {
            viewport.Origin.X + (viewport.Size.X - contentSize.X) * 0.5f,
            viewport.Origin.Y + (viewport.Size.Y - contentSize.Y) * 0.5f
        };


        struct RenderItem {
            int16_t zOrder;
            Entity entity;
        };

        std::vector<RenderItem> panelItems;
        std::vector<RenderItem> textItems;
        world.Each<Components::GUIElement, Components::GUIPanel>(
            [&](Entity entity, const Components::GUIElement& element, const Components::GUIPanel&) {
                if (!element.Visible) {
                    return;
                }
                panelItems.push_back(RenderItem{ element.ZOrder, entity });
            });

        world.Each<Components::GUIElement, Components::GUIText>(
            [&](Entity entity, const Components::GUIElement& element, const Components::GUIText&) {
                if (!element.Visible) {
                    return;
                }
                textItems.push_back(RenderItem{ element.ZOrder, entity });
            });

        std::sort(panelItems.begin(), panelItems.end(),
            [](const RenderItem& a, const RenderItem& b) { return a.zOrder < b.zOrder; });
        std::sort(textItems.begin(), textItems.end(),
            [](const RenderItem& a, const RenderItem& b) { return a.zOrder < b.zOrder; });

        const Vector2D anchorOrigin = viewport.Origin;
        const Vector2D anchorSize = viewport.Size;

        for (const auto& item : panelItems) {
            const auto& element = world.Get<Components::GUIElement>(item.entity);
            const auto& panel = world.Get<Components::GUIPanel>(item.entity);

            Vector2D size = { element.Size.X * scale.X, element.Size.Y * scale.Y };
            Vector2D anchor = anchorOrigin;

            switch (element.Alignment) {
            case Components::GUIAlignment::TopLeft:
                anchor = anchorOrigin;
                break;
            case Components::GUIAlignment::Top:
                anchor = { anchorOrigin.X + anchorSize.X * 0.5f, anchorOrigin.Y };
                break;
            case Components::GUIAlignment::TopRight:
                anchor = { anchorOrigin.X + anchorSize.X, anchorOrigin.Y };
                break;
            case Components::GUIAlignment::Left:
                anchor = { anchorOrigin.X, anchorOrigin.Y + anchorSize.Y * 0.5f };
                break;
            case Components::GUIAlignment::Center:
                anchor = { anchorOrigin.X + anchorSize.X * 0.5f, anchorOrigin.Y + anchorSize.Y * 0.5f };
                break;
            case Components::GUIAlignment::Right:
                anchor = { anchorOrigin.X + anchorSize.X, anchorOrigin.Y + anchorSize.Y * 0.5f };
                break;
            case Components::GUIAlignment::BottomLeft:
                anchor = { anchorOrigin.X, anchorOrigin.Y + anchorSize.Y };
                break;
            case Components::GUIAlignment::Bottom:
                anchor = { anchorOrigin.X + anchorSize.X * 0.5f, anchorOrigin.Y + anchorSize.Y };
                break;
            case Components::GUIAlignment::BottomRight:
                anchor = { anchorOrigin.X + anchorSize.X, anchorOrigin.Y + anchorSize.Y };
                break;
            }

            Vector2D position = {
                anchor.X + canvas.Offset.X + element.Position.X * scale.X,
                anchor.Y + canvas.Offset.Y + element.Position.Y * scale.Y
            };

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

            renderer->SubmitGUIPanel(position, size, panel.Color, panel.CornerRadius);
        }

        for (const auto& item : textItems) {
            const auto& element = world.Get<Components::GUIElement>(item.entity);
            const auto& text = world.Get<Components::GUIText>(item.entity);

            Vector2D size = { element.Size.X * scale.X, element.Size.Y * scale.Y };
            Vector2D anchor = anchorOrigin;

            switch (element.Alignment) {
            case Components::GUIAlignment::TopLeft:
                anchor = anchorOrigin;
                break;
            case Components::GUIAlignment::Top:
                anchor = { anchorOrigin.X + anchorSize.X * 0.5f, anchorOrigin.Y };
                break;
            case Components::GUIAlignment::TopRight:
                anchor = { anchorOrigin.X + anchorSize.X, anchorOrigin.Y };
                break;
            case Components::GUIAlignment::Left:
                anchor = { anchorOrigin.X, anchorOrigin.Y + anchorSize.Y * 0.5f };
                break;
            case Components::GUIAlignment::Center:
                anchor = { anchorOrigin.X + anchorSize.X * 0.5f, anchorOrigin.Y + anchorSize.Y * 0.5f };
                break;
            case Components::GUIAlignment::Right:
                anchor = { anchorOrigin.X + anchorSize.X, anchorOrigin.Y + anchorSize.Y * 0.5f };
                break;
            case Components::GUIAlignment::BottomLeft:
                anchor = { anchorOrigin.X, anchorOrigin.Y + anchorSize.Y };
                break;
            case Components::GUIAlignment::Bottom:
                anchor = { anchorOrigin.X + anchorSize.X * 0.5f, anchorOrigin.Y + anchorSize.Y };
                break;
            case Components::GUIAlignment::BottomRight:
                anchor = { anchorOrigin.X + anchorSize.X, anchorOrigin.Y + anchorSize.Y };
                break;
            }

            Vector2D position = {
                anchor.X + canvas.Offset.X + element.Position.X * scale.X,
                anchor.Y + canvas.Offset.Y + element.Position.Y * scale.Y
            };

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

            const std::string textValue = text.GetText();
            if (textValue.empty()) {
                continue;
            }

            const float fontSize = text.FontSize * scale.X;
            renderer->SubmitGUIText(position, textValue, text.GetFontPath(), fontSize, text.Color);
        }
    }

    void GUIRenderSystem::OnDestroy(World& world) {
        (void)world;
    }

    SystemMetadata GUIRenderSystem::GetMetadata() const {
        ComponentAccessBuilder builder("GUIRenderSystem");
        builder.SetExecutionOrder(-10);
        return builder
            .ReadComponent<Components::GUICanvas>()
            .ReadComponent<Components::GUIElement>()
            .ReadComponent<Components::GUIPanel>()
            .ReadComponent<Components::GUIText>()
            .Build();
    }
}
