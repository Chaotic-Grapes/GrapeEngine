/* Start Header *****************************************************************/
/*!
\file    AudioSystemRegistry.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the global registry for mapping ECS worlds to their 
corresponding AudioSystem instances. This allows scenes and systems to 
access the appropriate audio system for a given world.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef AUDIOSYSTEMREGISTRY_H
#define AUDIOSYSTEMREGISTRY_H

#include <unordered_map>

// Forward declarations
namespace ECS { class World; }
class AudioSystem;

namespace Audio {
    /**
     * @brief Global registry mapping ECS worlds to their AudioSystem instances.
     * 
     * This allows scenes and systems to look up the appropriate audio system
     * for a given world, enabling proper audio management across scene switches.
     */
    extern std::unordered_map<ECS::World*, AudioSystem*> AUDIO_MAP;
}

#endif
