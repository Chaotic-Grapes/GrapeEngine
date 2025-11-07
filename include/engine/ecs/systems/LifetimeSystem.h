/* Start Header *****************************************************************/
/*!
\file   LifetimeSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th October 2025
\brief
Declares the LifetimeSystem which manages entity lifetimes in the ECS framework.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef LIFETIMESYSTEM_H
#define LIFETIMESYSTEM_H

#include <cstdint>
#include <vector>
#include "ecs/World.h"
#include "ecs/Components.h"

namespace ECS {
    class LifetimeSystem {
    public:
        // Decrease Lifetime and destroy expired entities (safe: destroys after iteration)
        static void Update(World& world, float dt);

        // Same, for a specific layer
        static void UpdateForLayer(World& world, float dt, uint16_t layerId);
    };
}

#endif
