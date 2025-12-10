/* Start Header *****************************************************************/
/*!
\file   LayerMask.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Declares layer mask utilities for physics collision filtering.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_LAYERMASK_H
#define ENGINE_LAYERMASK_H

#include <cstdint>

namespace Engine {

    inline uint32_t LayerIdToBit(uint16_t layerId) noexcept {
        if (layerId >= 32) return 0u;
        return (1u << static_cast<uint32_t>(layerId));
    }

    inline bool CanCollide(uint32_t maskA, uint16_t layerAId, uint32_t maskB, uint16_t layerBId) noexcept {
        const uint32_t bitA = LayerIdToBit(layerAId);
        const uint32_t bitB = LayerIdToBit(layerBId);
        
        // Out-of-range layer ids are treated as non-collidable to avoid UB.
        if (bitA == 0u || bitB == 0u)
            return false;
          
        // Require both masks to include the other's layer bit (symmetrical test).
        return ((maskA & bitB) != 0u) && ((maskB & bitA) != 0u);
    }

}

#endif
