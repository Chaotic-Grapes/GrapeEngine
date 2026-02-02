/* Start Header *****************************************************************/
/*!
\file   GUILayoutSystem.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of the GUI layout system.
*/
/* End Header *******************************************************************/

#include "ecs/systems/GUILayoutSystem.h"
#include "ecs/ui/GUIContext.h"
#include "ecs/ui/GUILayout.h"
#include "core/Application.h"
#include "core/Logger.h"
#include "ecs/Components.h"
#include "platform/IPlatformContext.h"
#include <algorithm>
#include <cstdint>
#include <functional>

namespace ECS {
    namespace {
        Vector2D GetCanvasSizeFromPlatform() {
            if (!Engine::CORE) {
                return { 0.0f, 0.0f };
            }

            auto* platform = Engine::CORE->GetPlatformContext();
            if (!platform) {
                return { 0.0f, 0.0f };
            }

            int width = 0;
            int height = 0;
            if (auto* renderDevice = platform->GetRenderDevice()) {
                width = renderDevice->GetViewportWidth();
                height = renderDevice->GetViewportHeight();
            }

            if ((width <= 0 || height <= 0) && platform->GetMainWindow()) {
                auto* window = platform->GetMainWindow();
                width = window->GetWidth();
                height = window->GetHeight();
            }

            if (width <= 0 || height <= 0) {
                return { 0.0f, 0.0f };
            }

            return { static_cast<float>(width), static_cast<float>(height) };
        }

        void UpdateCanvasMetrics(World& world) {
            auto& ctx = UI::GUIContext::Get();

            const Vector2D actualSize = GetCanvasSizeFromPlatform();
            if (actualSize.X > 0.0f && actualSize.Y > 0.0f) {
                if (ctx.CanvasSize.X != actualSize.X || ctx.CanvasSize.Y != actualSize.Y) {
                    ctx.CanvasSize = actualSize;
                    ctx.SpatialGridDirty = true;
                }
            }

            Components::GUICanvas canvasSettings{};
            bool found = false;
            uint32_t lowestIndex = UINT32_MAX;
            world.Each<Components::GUICanvas>([&](Entity entity, const Components::GUICanvas& canvas) {
                if (!found || entity.Index < lowestIndex) {
                    canvasSettings = canvas;
                    lowestIndex = entity.Index;
                    found = true;
                }
            });

            Vector2D referenceSize = ctx.CanvasSize;
            float scaleFactor = 1.0f;
            if (found) {
                referenceSize = canvasSettings.ReferenceSize;
                scaleFactor = canvasSettings.ScaleFactor;
            }

            if (referenceSize.X <= 0.0f || referenceSize.Y <= 0.0f) {
                referenceSize = ctx.CanvasSize;
            }

            float scaleX = ctx.CanvasSize.X / referenceSize.X;
            float scaleY = ctx.CanvasSize.Y / referenceSize.Y;
            float scale = 1.0f;
            switch (canvasSettings.ScaleMode) {
                case Components::GUICanvasScaleMode::Fill:
                    scale = std::max(scaleX, scaleY);
                    break;
                case Components::GUICanvasScaleMode::MatchWidth:
                    scale = scaleX;
                    break;
                case Components::GUICanvasScaleMode::MatchHeight:
                    scale = scaleY;
                    break;
                case Components::GUICanvasScaleMode::Fit:
                default:
                    scale = std::min(scaleX, scaleY);
                    break;
            }
            scale *= scaleFactor;
            if (scale <= 0.0f) {
                scale = 1.0f;
            }

            const float extraX = ctx.CanvasSize.X - referenceSize.X * scale;
            const float extraY = ctx.CanvasSize.Y - referenceSize.Y * scale;
            Vector2D baseOffset{ 0.0f, 0.0f };
            switch (canvasSettings.Alignment) {
                case Components::GUICanvasAlignment::TopLeft:
                    baseOffset = { 0.0f, 0.0f };
                    break;
                case Components::GUICanvasAlignment::Top:
                    baseOffset = { extraX * 0.5f, 0.0f };
                    break;
                case Components::GUICanvasAlignment::TopRight:
                    baseOffset = { extraX, 0.0f };
                    break;
                case Components::GUICanvasAlignment::Left:
                    baseOffset = { 0.0f, extraY * 0.5f };
                    break;
                case Components::GUICanvasAlignment::Right:
                    baseOffset = { extraX, extraY * 0.5f };
                    break;
                case Components::GUICanvasAlignment::BottomLeft:
                    baseOffset = { 0.0f, extraY };
                    break;
                case Components::GUICanvasAlignment::Bottom:
                    baseOffset = { extraX * 0.5f, extraY };
                    break;
                case Components::GUICanvasAlignment::BottomRight:
                    baseOffset = { extraX, extraY };
                    break;
                case Components::GUICanvasAlignment::Center:
                default:
                    baseOffset = { extraX * 0.5f, extraY * 0.5f };
                    break;
            }

            const Vector2D newOffset{
                baseOffset.X + canvasSettings.Offset.X,
                baseOffset.Y + canvasSettings.Offset.Y
            };
            if (ctx.LayoutCanvasSize.X != referenceSize.X || ctx.LayoutCanvasSize.Y != referenceSize.Y ||
                ctx.CanvasScale != scale ||
                ctx.CanvasOffset.X != newOffset.X || ctx.CanvasOffset.Y != newOffset.Y) {
                ctx.SpatialGridDirty = true;
            }

            ctx.LayoutCanvasSize = referenceSize;
            ctx.CanvasScale = scale;
            ctx.CanvasOffset = newOffset;
        }

        size_t HashFloat(float value) {
            return std::hash<float>{}(value);
        }

        size_t HashVector2D(const Vector2D& value) {
            size_t hash = HashFloat(value.X);
            hash ^= HashFloat(value.Y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }

        size_t HashGUIElement(const ECS::Components::GUIElement& element, ECS::Entity parentEntity) {
            size_t hash = HashVector2D(element.Position);
            hash ^= HashVector2D(element.Size) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= HashVector2D(element.AnchorMin) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= HashVector2D(element.AnchorMax) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= HashVector2D(element.Offset) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= HashFloat(element.PaddingLeft) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= HashFloat(element.PaddingRight) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= HashFloat(element.PaddingTop) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= HashFloat(element.PaddingBottom) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<int>{}(static_cast<int>(element.HAlign)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<int>{}(static_cast<int>(element.VAlign)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<int>{}(static_cast<int>(element.ElementType)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<int>{}(static_cast<int>(element.ZOrder)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<uint32_t>{}(parentEntity.Index) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }

        void RefreshLayoutInvalidation(World& world) {
            auto& ctx = UI::GUIContext::Get();

            world.Each<Components::GUIElement>([&](Entity entity, Components::GUIElement& element) {
                const auto* parent = world.TryGet<Components::Parent>(entity);
                ECS::Entity parentEntity = parent ? parent->ParentEntity : ECS::NULL_ENTITY;

                size_t hash = HashGUIElement(element, parentEntity);
                auto it = ctx.LayoutHashes.find(entity.Index);
                if (it == ctx.LayoutHashes.end() || it->second != hash) {
                    element.DirtyLayout = true;
                    ctx.LayoutHashes[entity.Index] = hash;
                }
            });
        }
    }

    void GUILayoutSystem::OnCreate(World& world) {
        (void)world;
        LOG_INFO("GUILayoutSystem initialized");
    }

    void GUILayoutSystem::OnUpdate(World& world) {
        UI::GUIContext& context = UI::GUIContext::Get();
        UpdateCanvasMetrics(world);
        RefreshLayoutInvalidation(world);
        UI::GUILayout::CalculateLayout(world, context.LayoutCanvasSize);
    }

    void GUILayoutSystem::OnDestroy(World& world) {
        (void)world;
        LOG_INFO("GUILayoutSystem destroyed");
    }

    SystemMetadata GUILayoutSystem::GetMetadata() const {
        return ComponentAccessBuilder("GUILayoutSystem")
            .WriteComponent<Components::GUIElement>()
            .ReadComponent<Components::GUICanvas>()
            .SetExecutionOrder(0)
            .SetGroup(SystemGroup::PreRender)
            .SetRunMode(SystemRunMode::Always)
            .SetEnabled(true)
            .Build();
    }

} // namespace ECS
