/* Start Header *****************************************************************/
/*!
\file   EntityUtils.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Provides utility functions for working with entities in the ECS system, including
packing and unpacking entity IDs and resolving entities within a world context. 

This is meant to be a lightweight utility class that can be used by both engine 
and editor code without introducing heavy dependencies, especially to avoid 
pulling in editor-specific headers into engine code.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ENTITYUTILS_H
#define ENTITYUTILS_H

#include <cstdint>
#include "ecs/World.h"

namespace ECS {
    // Utility to pack/unpack/resolve entity ids (Index + Generation)
    // Trying to keep it separate
    class EntityUtils {
    public:
        // Pack as [63..32]=Generation, [31..0]=Index
        static uint64_t Pack(Entity e) {
            return (uint64_t(e.Generation) << 32) | uint64_t(e.Index);
        }

        static Entity Unpack(uint64_t id) {
            Entity e;
            e.Index = static_cast<uint32_t>(id & 0xFFFFFFFFull);
            e.Generation = static_cast<uint32_t>(id >> 32);
            return e;
        }

        // Validate id against world; returns true if alive and sets out
        static bool TryResolve(const World& world, uint64_t id, Entity& out) {
            Entity e = Unpack(id);
            if (world.IsAlive(e)) {
                out = e;
                return true;
            }

            out = NULL_ENTITY;
            return false;
        }
    };
}

#endif
