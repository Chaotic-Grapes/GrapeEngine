/* Start Header *****************************************************************/
/*!
\file   TextureFilter.h
\author Muhammad Nur Fadzly Bin Zulkifli
\par    muhammadnurfadzly.b@digipen.edu
\date   5th January 2026
\brief
Defines texture sampling filters that can be selected per draw call.
*/
/* End Header *******************************************************************/

#pragma once
#include <cstdint>

namespace Graphics {
    enum class TextureFilter : uint8_t {
        Nearest = 0,
        Linear = 1
    };
}
