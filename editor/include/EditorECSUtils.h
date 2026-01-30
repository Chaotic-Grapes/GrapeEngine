/* Start Header *****************************************************************/
/*!
\file   EditorECSUtils.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Shared ECS helper utilities for editor-side code.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_ECS_UTILS_H
#define EDITOR_ECS_UTILS_H

#include <cstdint>
#include <string>
#include "ecs/Components.h"
#include "ecs/Entity.h"
#include "ecs/World.h"

namespace Editor::ECSUtils {
    uint32_t FNV1aHash(const char* str);
    ECS::ComponentTypeId GetComponentIdFromName(const char* name);

    const ECS::Components::Name* GetNamePtr(ECS::World* world, ECS::Entity entity);
    ECS::Components::Name* GetNamePtrMutable(ECS::World& world, ECS::Entity entity);

    void SetEntityName(ECS::World& world, ECS::Entity entity, const char* value);
    void SetEntityName(ECS::World& world, ECS::Entity entity, const std::string& value);
}

#endif
