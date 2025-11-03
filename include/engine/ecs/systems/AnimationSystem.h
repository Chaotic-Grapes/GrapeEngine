/* Start Header *****************************************************************/
/*!
\file   AnimationSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   1st November 2025
\brief
Defines the AnimationSystem which is responsible for updating sprite sheet
animations in the ECS framework.

The system processes entities with SpriteSheetAnimation2D and AnimationState2D
components, advancing frames based on elapsed time and updating the associated
SpriteRenderer2D component with the correct UV coordinates for the current frame.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ANIMATIONSYSTEM_H
#define ANIMATIONSYSTEM_H

#include "ecs/World.h"

namespace ECS {
    class AnimationSystem {
    public:
        /**
         * @brief Update all sprite sheet animations in the world
         * @param world The ECS world containing entities and components
         * @param dt Delta time in seconds
         */
        static void Update(World& world, float dt);
    };
}

#endif
