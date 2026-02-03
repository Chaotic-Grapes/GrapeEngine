/* Start Header *****************************************************************/
/*!
\file    GUILayout.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Declares the GUI layout calculation system for hierarchical positioning and sizing.

The layout system is responsible for:
- Computing world positions based on anchoring
- Calculating element sizes based on content, constraints, and parent size
- Managing parent-child relationships and layout groups
- Handling different layout modes (Box, Grid, Absolute)
- Efficient layout invalidation and dirty tracking

The layout system operates independently and can be used without the full GUI pipeline
for headless layout calculations or testing.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_LAYOUT_H
#define GUI_LAYOUT_H

#include "ecs/World.h"
#include "ecs/Components.h"
#include "math/Vector2D.h"
#include <vector>

namespace ECS {
    namespace UI {

        /**
         * @brief Layout calculation result for a single element
         */
        struct LayoutResult {
            Vector2D Position;           // Calculated world position
            Vector2D Size;               // Calculated size
            Vector2D MinSize;            // Minimum size constraint
            Vector2D MaxSize;            // Maximum size constraint (0 = unlimited)
            Vector2D PreferredSize;      // Preferred size from content
            bool NeedsRecalculation;     // Whether this element needs layout recalc
        };

        /**
         * @brief Layout calculation context
         * Passed through layout hierarchy to maintain state during calculation
         */
        struct LayoutContext {
            Vector2D ParentPosition;
            Vector2D ParentSize;
            Vector2D CanvasSize;
            float DeltaTime;
            bool ForceRecalculate = false;

            LayoutContext(Vector2D canvasSize = {1920.0f, 1080.0f})
                : ParentPosition(0.0f, 0.0f), ParentSize(canvasSize), CanvasSize(canvasSize),
                  DeltaTime(0.0f) {}
        };

        /**
         * @brief GUI Layout Engine
         * Handles hierarchical layout calculation for GUI elements
         */
        class GUILayout {
        public:
            /**
             * @brief Calculate layout for all GUI elements
             * @param world The ECS world
             * @param canvasSize The root canvas size
             */
            static void CalculateLayout(World& world, Vector2D canvasSize);

            /**
             * @brief Calculate layout for a single element and its children
             * @param world The ECS world
             * @param entity The element to calculate layout for
             * @param context Layout context containing parent info
             * @return The calculated layout result
             */
            static LayoutResult CalculateElementLayout(World& world, Entity entity,
                                                      const LayoutContext& context);

            /**
             * @brief Mark an element's layout as dirty (needs recalculation)
             * Automatically invalidates parents up the hierarchy
             * @param world The ECS world
             * @param entity The element to invalidate
             */
            static void InvalidateLayout(World& world, Entity entity);

            /**
             * @brief Mark an element and all children as dirty
             * @param world The ECS world
             * @param entity The root element
             */
            static void InvalidateLayoutRecursive(World& world, Entity entity);

            /**
             * @brief Calculate preferred size for an element based on content
             * @param world The ECS world
             * @param entity The element
             * @return Preferred {width, height}
             */
            static Vector2D CalculatePreferredSize(World& world, Entity entity);

            /**
             * @brief Calculate world position from anchoring
             * @param localPos Local position relative to parent
             * @param anchorMin Anchor minimum (0,0 = top-left, 1,1 = bottom-right)
             * @param anchorMax Anchor maximum
             * @param parentPos Parent's world position
             * @param parentSize Parent's size
             * @param elementSize Element's size
             * @return Calculated world position
             */
            static Vector2D CalculateAnchoredPosition(
                Vector2D localPos,
                Vector2D anchorMin,
                Vector2D anchorMax,
                Vector2D parentPos,
                Vector2D parentSize,
                Vector2D elementSize);

            /**
             * @brief Calculate stretched size from anchors
             * @param anchorMin Anchor minimum
             * @param anchorMax Anchor maximum
             * @param parentSize Parent size
             * @return Calculated size
             */
            static Vector2D CalculateAnchoredSize(
                Vector2D anchorMin,
                Vector2D anchorMax,
                Vector2D parentSize);

            /**
             * @brief Get all child entities of a container
             */
            static std::vector<Entity> GetContainerChildren(
                World& world,
                Entity containerEntity,
                const Components::GUIContainer& container);

        private:
            /**
             * @brief Calculate layout for a container's children
             * @param world The ECS world
             * @param containerEntity The container entity
             * @param container The container component
             * @param element The element component
             * @param context Layout context
             */
            static void CalculateContainerLayout(
                World& world,
                Entity containerEntity,
                const Components::GUIContainer& container,
                const Components::GUIElement& element,
                LayoutContext& context);

            /**
             * @brief Calculate horizontal box layout (children arranged left-to-right)
             */
            static void CalculateHorizontalBoxLayout(
                World& world,
                const std::vector<Entity>& children,
                const Components::GUIContainer& container,
                Vector2D containerSize,
                Vector2D containerPos);

            /**
             * @brief Calculate vertical box layout (children arranged top-to-bottom)
             */
            static void CalculateVerticalBoxLayout(
                World& world,
                const std::vector<Entity>& children,
                const Components::GUIContainer& container,
                Vector2D containerSize,
                Vector2D containerPos);

            /**
             * @brief Calculate grid layout
             */
            static void CalculateGridLayout(
                World& world,
                const std::vector<Entity>& children,
                const Components::GUIContainer& container,
                Vector2D containerSize,
                Vector2D containerPos);

            /**
             * @brief Apply alignment to an element
             */
            static Vector2D ApplyAlignment(
                Components::HorizontalAlignment hAlign,
                Components::VerticalAlignment vAlign,
                Vector2D elementSize,
                Vector2D availableSize);

            /**
             * @brief Clamp size to min/max constraints
             */
            static Vector2D ClampSize(
                Vector2D size,
                Vector2D minSize,
                Vector2D maxSize);

            /**
             * @brief Calculate flex size for stretching elements
             */
            static float CalculateFlexSize(
                float preferredSize,
                float minSize,
                float maxSize,
                float flexFactor,
                float availableSpace);
        };

        /**
         * @brief Layout group resolver
         * Calculates preferred sizes for layout groups
         */
        class GUILayoutGroupResolver {
        public:
            /**
             * @brief Calculate preferred size for a layout group
             * @param world The ECS world
             * @param entity The element with layout group
             * @return Preferred size
             */
            static Vector2D CalculatePreferredSize(World& world, Entity entity);

            /**
             * @brief Mark layout group as needing recalculation
             */
            static void InvalidatePreferredSize(World& world, Entity entity);

        private:
            /**
             * @brief Calculate size based on child content
             */
            static Vector2D CalculateSizeFromChildren(
                World& world,
                Entity containerEntity);
        };

    } // namespace UI
} // namespace ECS

#endif
