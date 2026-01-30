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

    inline bool HasComponent(ECS::World* world, ECS::Entity entity, const char* name) {
        if (!world) return false;
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        if (id == ECS::NULL_COMPONENT_ID) return false;
        return world->HasById(entity, id);
    }

    template<typename T>
    inline T* GetComponentPtr(ECS::World* world, ECS::Entity entity, const char* name) {
        if (!world) return nullptr;
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        if (id == ECS::NULL_COMPONENT_ID) return nullptr;
        return static_cast<T*>(world->GetRawComponentPtr(entity, id));
    }

    template<typename T>
    inline const T* GetComponentPtr(const ECS::World* world, ECS::Entity entity, const char* name) {
        if (!world) return nullptr;
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        if (id == ECS::NULL_COMPONENT_ID) return nullptr;
        return static_cast<const T*>(const_cast<ECS::World*>(world)->GetRawComponentPtr(entity, id));
    }

    template<typename T>
    inline void SetComponent(ECS::World* world, ECS::Entity entity, const char* name, const T& value) {
        if (!world) return;
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        if (id == ECS::NULL_COMPONENT_ID) return;
        void* ptr = world->GetRawComponentPtr(entity, id);
        if (ptr) {
            *static_cast<T*>(ptr) = value;
        } else {
            world->AddComponentById(entity, id, const_cast<T*>(&value), sizeof(T));
        }
    }

    template<typename T>
    inline void AddComponent(ECS::World* world, ECS::Entity entity, const char* name, const T& value) {
        if (!world) return;
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        if (id == ECS::NULL_COMPONENT_ID) return;
        world->AddComponentById(entity, id, const_cast<T*>(&value), sizeof(T));
    }

    inline void RemoveComponent(ECS::World* world, ECS::Entity entity, const char* name) {
        if (!world) return;
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        if (id == ECS::NULL_COMPONENT_ID) return;
        world->RemoveById(entity, id);
    }
}

#endif
