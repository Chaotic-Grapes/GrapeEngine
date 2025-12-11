/* Start Header *****************************************************************/
/*!
\file    PrefabUtils.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains utility functions for manipulating PrefabInstanceMetadata
component flags safely. These functions are external to the component to maintain
ECS architectural purity (components are pure data only).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef PREFAB_UTILS_H
#define PREFAB_UTILS_H

#include "ecs/Components.h"

namespace PrefabUtils {

    /**
     * @brief Check if a prefab instance has been modified from its source prefab.
     * @param meta PrefabInstanceMetadata component of the entity
     * @return true if the instance has modifications, false otherwise
     */
    inline bool IsModified(const ECS::Components::PrefabInstanceMetadata& meta) {
        return (meta.Flags & 0x1) != 0;
    }

    /**
     * @brief Set the modified flag for a prefab instance.
     * @param meta PrefabInstanceMetadata component of the entity
     * @param modified true to mark as modified, false to clear
     */
    inline void SetModified(ECS::Components::PrefabInstanceMetadata& meta, bool modified) {
        if (modified) {
            meta.Flags |= 0x1;
        }
        else {
            meta.Flags &= ~0x1;
        }
    }

    /**
     * @brief Check if a prefab instance is synced with its source prefab.
     * @param meta PrefabInstanceMetadata component of the entity
     * @return true if the instance is synced, false otherwise
     */
    inline bool IsSynced(const ECS::Components::PrefabInstanceMetadata& meta) {
        return (meta.Flags & 0x2) != 0;
    }

    /**
     * @brief Set the synced flag for a prefab instance.
     * @param meta PrefabInstanceMetadata component of the entity
     * @param synced true to mark as synced, false to clear
     */
    inline void SetSynced(ECS::Components::PrefabInstanceMetadata& meta, bool synced) {
        if (synced) {
            meta.Flags |= 0x2;
        }
        else {
            meta.Flags &= ~0x2;
        }
    }

}

#endif
