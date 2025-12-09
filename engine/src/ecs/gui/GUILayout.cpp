/* Start Header *****************************************************************/
/*!
\file   GUILayout.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation of the GUI layout calculation system for hierarchical positioning
and sizing of GUI elements.

This implementation handles:
- Anchoring-based responsive positioning
- Multiple layout modes (Absolute, HorizontalBox, VerticalBox, Grid)
- Recursive layout calculation for element hierarchies
- Layout invalidation and dirty tracking
- Preferred size calculation from content

The layout system operates independently of rendering and can be used for
headless layout calculations or testing.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/gui/GUILayout.h"
#include "ecs/World.h"
#include "core/Logger.h"
#include <algorithm>
#include <cmath>

namespace ECS {
    namespace GUI {

        // ====================================================================
        // GUILayout Implementation
        // ====================================================================

        void GUILayout::CalculateLayout(World& world, Vector2D canvasSize) {
            LayoutContext context(canvasSize);

            // Find all root GUI elements (those without parents or with invalid parents)
            world.Each<GUIElement, GUIContainer>([&](Entity entity, GUIElement& element, GUIContainer& container) {
                if (container.Parent == 0) {
                    // Root element - calculate from canvas
                    CalculateElementLayout(world, entity, context);
                }
            });

            // Also handle elements without containers
            world.Each<GUIElement>([&](Entity entity, GUIElement& element) {
                if (!world.Has<GUIContainer>(entity)) {
                    // Orphaned element - calculate from canvas
                    CalculateElementLayout(world, entity, context);
                }
            });
        }

        LayoutResult GUILayout::CalculateElementLayout(World& world, Entity entity,
                                                       const LayoutContext& context) {
            if (entity.IsNull() || !world.IsAlive(entity)) {
                return LayoutResult{ {0, 0}, {100, 100}, {0, 0}, {0, 0}, {100, 100}, false };
            }

            if (!world.Has<GUIElement>(entity)) {
                return LayoutResult{ {0, 0}, {100, 100}, {0, 0}, {0, 0}, {100, 100}, false };
            }

            auto& element = world.Get<GUIElement>(entity);

            // Skip if layout is clean and not forced
            if (!element.DirtyLayout && !context.ForceRecalculate) {
                return LayoutResult{
                    element.WorldPosition,
                    element.Size,
                    {0, 0},
                    {0, 0},
                    element.Size,
                    false
                };
            }

            // Calculate world position from anchoring
            Vector2D worldPos = CalculateAnchoredPosition(
                element.Position,
                element.AnchorMin,
                element.AnchorMax,
                context.ParentPosition,
                context.ParentSize,
                element.Size
            );

            element.WorldPosition = worldPos;

            // If this is a container, layout children
            if (world.Has<GUIContainer>(entity)) {
                LayoutContext childContext = context;
                childContext.ParentPosition = worldPos;
                childContext.ParentSize = element.Size;

                auto& container = world.Get<GUIContainer>(entity);
                CalculateContainerLayout(world, entity, container, element, childContext);
            }

            element.DirtyLayout = false;

            return LayoutResult{
                worldPos,
                element.Size,
                {0, 0},
                {0, 0},
                element.Size,
                false
            };
        }

        Vector2D GUILayout::CalculateAnchoredPosition(
            Vector2D localPos,
            Vector2D anchorMin,
            Vector2D anchorMax,
            Vector2D parentPos,
            Vector2D parentSize,
            Vector2D elementSize) {
            
            // Clamp anchor values to 0-1
            anchorMin.X = std::max(0.0f, std::min(1.0f, anchorMin.X));
            anchorMin.Y = std::max(0.0f, std::min(1.0f, anchorMin.Y));
            anchorMax.X = std::max(0.0f, std::min(1.0f, anchorMax.X));
            anchorMax.Y = std::max(0.0f, std::min(1.0f, anchorMax.Y));

            // Calculate anchor point
            Vector2D anchorPos{
                parentPos.X + anchorMin.X * parentSize.X,
                parentPos.Y + anchorMin.Y * parentSize.Y
            };

            // If anchors are stretched, element fills that space
            if (std::abs(anchorMax.X - anchorMin.X) > 0.01f) {
                // Width is stretched
                float stretchWidth = (anchorMax.X - anchorMin.X) * parentSize.X;
                // Position from anchor min
                return {
                    anchorPos.X + localPos.X,
                    anchorPos.Y + localPos.Y
                };
            }

            // Otherwise, position from anchor point
            return {
                anchorPos.X + localPos.X,
                anchorPos.Y + localPos.Y
            };
        }

        Vector2D GUILayout::CalculateAnchoredSize(
            Vector2D anchorMin,
            Vector2D anchorMax,
            Vector2D parentSize) {
            
            // If anchors define a region, element stretches to fill it
            Vector2D stretchSize{
                (anchorMax.X - anchorMin.X) * parentSize.X,
                (anchorMax.Y - anchorMin.Y) * parentSize.Y
            };

            // Clamp to minimum 0
            stretchSize.X = std::max(0.0f, stretchSize.X);
            stretchSize.Y = std::max(0.0f, stretchSize.Y);

            return stretchSize;
        }

        void GUILayout::CalculateContainerLayout(
            World& world,
            Entity containerEntity,
            const GUIContainer& container,
            const GUIElement& element,
            LayoutContext& context) {
            
            switch (container.Layout) {
                case LayoutType::HorizontalBox:
                    CalculateHorizontalBoxLayout(world, GetContainerChildren(world, container),
                                               container, element.Size, element.WorldPosition);
                    break;

                case LayoutType::VerticalBox:
                    CalculateVerticalBoxLayout(world, GetContainerChildren(world, container),
                                             container, element.Size, element.WorldPosition);
                    break;

                case LayoutType::Grid:
                    CalculateGridLayout(world, GetContainerChildren(world, container),
                                      container, element.Size, element.WorldPosition);
                    break;

                case LayoutType::Absolute:
                default:
                    // Absolute positioning - each child uses own Position/Size
                    for (uint16_t i = 0; i < container.ChildCount && i < GUIContainer::MaxChildren; ++i) {
                        if (container.Children[i] != 0) {
                            Entity child{container.Children[i]};
                            CalculateElementLayout(world, child, context);
                        }
                    }
                    break;
            }
        }

        void GUILayout::CalculateHorizontalBoxLayout(
            World& world,
            const std::vector<Entity>& children,
            const GUIContainer& container,
            Vector2D containerSize,
            Vector2D containerPos) {
            
            if (children.empty()) {
                return;
            }

            // Calculate available space
            float availableWidth = containerSize.X - (container.Spacing * (children.size() - 1));
            float totalPreferredWidth = 0.0f;

            // Calculate total preferred width
            for (const Entity& child : children) {
                if (world.Has<GUIElement>(child)) {
                    totalPreferredWidth += world.Get<GUIElement>(child).Size.X;
                }
            }

            // Distribute width
            float currentX = containerPos.X + container.Spacing;
            for (const Entity& child : children) {
                if (!world.Has<GUIElement>(child)) {
                    continue;
                }

                auto& childElement = world.Get<GUIElement>(child);
                childElement.WorldPosition = {currentX, containerPos.Y};

                // Apply padding
                if (world.Has<GUILayoutGroup>(child)) {
                    // Use flexible sizing
                    if (container.ChildForceExpandWidth) {
                        childElement.Size.X = availableWidth / children.size();
                    }
                }

                currentX += childElement.Size.X + container.Spacing;

                // Recursively layout children
                if (world.Has<GUIContainer>(child)) {
                    LayoutContext childContext;
                    childContext.ParentPosition = childElement.WorldPosition;
                    childContext.ParentSize = childElement.Size;
                    CalculateContainerLayout(world, child, world.Get<GUIContainer>(child),
                                           childElement, childContext);
                }
            }
        }

        void GUILayout::CalculateVerticalBoxLayout(
            World& world,
            const std::vector<Entity>& children,
            const GUIContainer& container,
            Vector2D containerSize,
            Vector2D containerPos) {
            
            if (children.empty()) {
                return;
            }

            // Calculate available space
            float availableHeight = containerSize.Y - (container.Spacing * (children.size() - 1));
            float totalPreferredHeight = 0.0f;

            // Calculate total preferred height
            for (const Entity& child : children) {
                if (world.Has<GUIElement>(child)) {
                    totalPreferredHeight += world.Get<GUIElement>(child).Size.Y;
                }
            }

            // Distribute height (top to bottom)
            float currentY = containerPos.Y + container.Spacing;
            for (const Entity& child : children) {
                if (!world.Has<GUIElement>(child)) {
                    continue;
                }

                auto& childElement = world.Get<GUIElement>(child);
                childElement.WorldPosition = {containerPos.X, currentY};

                // Apply padding
                if (world.Has<GUILayoutGroup>(child)) {
                    // Use flexible sizing
                    if (container.ChildForceExpandHeight) {
                        childElement.Size.Y = availableHeight / children.size();
                    }
                }

                currentY += childElement.Size.Y + container.Spacing;

                // Recursively layout children
                if (world.Has<GUIContainer>(child)) {
                    LayoutContext childContext;
                    childContext.ParentPosition = childElement.WorldPosition;
                    childContext.ParentSize = childElement.Size;
                    CalculateContainerLayout(world, child, world.Get<GUIContainer>(child),
                                           childElement, childContext);
                }
            }
        }

        void GUILayout::CalculateGridLayout(
            World& world,
            const std::vector<Entity>& children,
            const GUIContainer& container,
            Vector2D containerSize,
            Vector2D containerPos) {
            
            if (children.empty() || container.GridColumns == 0) {
                return;
            }

            // Calculate cell size
            float cellWidth = (containerSize.X / container.GridColumns) - container.GridCellPaddingX;
            float cellHeight = cellWidth;  // Square cells by default

            // Layout children in grid
            uint32_t childIndex = 0;
            for (uint32_t row = 0; row < (children.size() + container.GridColumns - 1) / container.GridColumns; ++row) {
                for (uint32_t col = 0; col < container.GridColumns && childIndex < children.size(); ++col) {
                    const Entity& child = children[childIndex];
                    if (!world.Has<GUIElement>(child)) {
                        childIndex++;
                        continue;
                    }

                    auto& childElement = world.Get<GUIElement>(child);
                    childElement.Size = {cellWidth, cellHeight};
                    childElement.WorldPosition = {
                        containerPos.X + col * (cellWidth + container.GridCellPaddingX),
                        containerPos.Y + row * (cellHeight + container.GridCellPaddingY)
                    };

                    childIndex++;
                }
            }
        }

        std::vector<Entity> GUILayout::GetContainerChildren(
            World& world,
            const GUIContainer& container) {
            
            std::vector<Entity> children;
            for (uint16_t i = 0; i < container.ChildCount && i < GUIContainer::MaxChildren; ++i) {
                if (container.Children[i] != 0) {
                    Entity child{container.Children[i]};
                    if (world.IsAlive(child)) {
                        children.push_back(child);
                    }
                }
            }
            return children;
        }

        void GUILayout::InvalidateLayout(World& world, Entity entity) {
            if (entity.IsNull() || !world.IsAlive(entity)) {
                return;
            }

            if (world.Has<GUIElement>(entity)) {
                world.Get<GUIElement>(entity).DirtyLayout = true;
            }

            // Invalidate parent
            if (world.Has<GUIContainer>(entity)) {
                uint32_t parentId = world.Get<GUIContainer>(entity).Parent;
                if (parentId != 0) {
                    Entity parent{parentId};
                    InvalidateLayout(world, parent);
                }
            }
        }

        void GUILayout::InvalidateLayoutRecursive(World& world, Entity entity) {
            if (entity.IsNull() || !world.IsAlive(entity)) {
                return;
            }

            if (world.Has<GUIElement>(entity)) {
                world.Get<GUIElement>(entity).DirtyLayout = true;
            }

            // Invalidate children
            if (world.Has<GUIContainer>(entity)) {
                const auto& container = world.Get<GUIContainer>(entity);
                for (uint16_t i = 0; i < container.ChildCount && i < GUIContainer::MaxChildren; ++i) {
                    if (container.Children[i] != 0) {
                        Entity child{container.Children[i]};
                        InvalidateLayoutRecursive(world, child);
                    }
                }
            }
        }

        Vector2D GUILayout::CalculatePreferredSize(World& world, Entity entity) {
            if (entity.IsNull() || !world.IsAlive(entity)) {
                return {100.0f, 100.0f};
            }

            if (!world.Has<GUIElement>(entity)) {
                return {100.0f, 100.0f};
            }

            // If has content, measure it
            if (world.Has<GUI::GUIText>(entity)) {
                // TODO: Measure text dimensions
                return {200.0f, 30.0f};
            }

            // If container, sum children
            if (world.Has<GUIContainer>(entity)) {
                const auto& container = world.Get<GUIContainer>(entity);
                Vector2D totalSize{0, 0};

                for (uint16_t i = 0; i < container.ChildCount && i < GUIContainer::MaxChildren; ++i) {
                        if (container.Children[i] != 0) {
                        Entity child{container.Children[i]};
                        Vector2D childSize = CalculatePreferredSize(world, child);

                        if (container.Layout == LayoutType::HorizontalBox) {
                            totalSize.X += childSize.X;
                            totalSize.Y = std::max(totalSize.Y, childSize.Y);
                        } else {  // VerticalBox and others
                            totalSize.X = std::max(totalSize.X, childSize.X);
                            totalSize.Y += childSize.Y;
                        }
                    }
                }

                return totalSize;
            }

            return {100.0f, 100.0f};
        }

        // ====================================================================
        // GUILayoutGroupResolver Implementation
        // ====================================================================

        Vector2D GUILayoutGroupResolver::CalculatePreferredSize(World& world, Entity entity) {
            if (entity.IsNull() || !world.IsAlive(entity)) {
                return {100.0f, 100.0f};
            }

            if (world.Has<GUILayoutGroup>(entity)) {
                auto& group = world.Get<GUILayoutGroup>(entity);
                if (!group.DirtyPreferredSize) {
                    return { group.PreferredWidth, group.PreferredHeight };
                }
            }

            Vector2D size = CalculateSizeFromChildren(world, entity);

            if (world.Has<GUILayoutGroup>(entity)) {
                auto& group = world.Get<GUILayoutGroup>(entity);
                group.PreferredWidth = size.X;
                group.PreferredHeight = size.Y;
                group.DirtyPreferredSize = false;
            }

            return size;
        }

        void GUILayoutGroupResolver::InvalidatePreferredSize(World& world, Entity entity) {
            if (entity.IsNull() || !world.IsAlive(entity)) {
                return;
            }

            if (world.Has<GUILayoutGroup>(entity)) {
                world.Get<GUILayoutGroup>(entity).DirtyPreferredSize = true;
            }

            if (world.Has<GUIContainer>(entity)) {
                const auto& container = world.Get<GUIContainer>(entity);
                for (uint16_t i = 0; i < container.ChildCount && i < GUIContainer::MaxChildren; ++i) {
                    if (container.Children[i] != 0) {
                        Entity child{container.Children[i]};
                        InvalidatePreferredSize(world, child);
                    }
                }
            }
        }

        Vector2D GUILayoutGroupResolver::CalculateSizeFromChildren(World& world, Entity containerEntity) {
            Vector2D totalSize{0.0f, 0.0f};

            if (containerEntity.IsNull() || !world.IsAlive(containerEntity)) {
                return totalSize;
            }

            if (!world.Has<GUIContainer>(containerEntity)) {
                return totalSize;
            }

            const auto& container = world.Get<GUIContainer>(containerEntity);

            for (uint16_t i = 0; i < container.ChildCount && i < GUIContainer::MaxChildren; ++i) {
                if (container.Children[i] == 0) continue;
                Entity child{container.Children[i]};
                if (!world.IsAlive(child)) continue;

                Vector2D childSize = GUILayout::CalculatePreferredSize(world, child);

                if (container.Layout == LayoutType::HorizontalBox) {
                    totalSize.X += childSize.X;
                    totalSize.Y = std::max(totalSize.Y, childSize.Y);
                } else { // VerticalBox, Grid, Absolute - treat as stacking vertically by default
                    totalSize.X = std::max(totalSize.X, childSize.X);
                    totalSize.Y += childSize.Y;
                }
            }

            return totalSize;
        }

    } // namespace GUI
} // namespace ECS
