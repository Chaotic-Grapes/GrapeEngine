/* Start Header *****************************************************************/
/*!
\file   AudioSystemRegistry.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Implementation file for the AudioSystemRegistry global.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "audio/AudioSystemRegistry.h"

namespace Audio {
    std::unordered_map<ECS::World*, AudioSystem*> AUDIO_MAP = {};
}
