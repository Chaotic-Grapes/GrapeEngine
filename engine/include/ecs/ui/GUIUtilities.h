/* Start Header *****************************************************************/
/*!
\file    GUIUtilities.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Shared GUI helper utilities for layout/input/render systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_UTILITIES_H
#define GUI_UTILITIES_H

#include "ecs/Components.h"
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "math/Vector2D.h"
#include <algorithm>
#include <vector>

namespace ECS {
    namespace UI {

        inline bool IsPointInElement(Vector2D point, const Components::GUIElement& element) {
            return point.X >= element.WorldPosition.X &&
                   point.X <= element.WorldPosition.X + element.Size.X &&
                   point.Y >= element.WorldPosition.Y &&
                   point.Y <= element.WorldPosition.Y + element.Size.Y;
        }

        inline std::vector<Entity> GetSortedGUIElements(World& world) {
            std::vector<Entity> elements;

            world.Each<Components::GUIElement>([&](Entity entity, const Components::GUIElement& element) {
                if (element.Active && element.Visible) {
                    elements.push_back(entity);
                }
            });

            std::sort(elements.begin(), elements.end(), [&world](Entity a, Entity b) {
                auto& elemA = world.Get<Components::GUIElement>(a);
                auto& elemB = world.Get<Components::GUIElement>(b);
                return elemA.ZOrder < elemB.ZOrder;
            });

            return elements;
        }

    } // namespace UI
} // namespace ECS

#endif
