/* Start Header *****************************************************************/
/*!
\file   EditorECSUtils.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Shared ECS helper utilities for editor-side code.
*/
/* End Header *******************************************************************/

#include "EditorECSUtils.h"
#include "ecs/ComponentRegistry.h"
#include "ecs/StringTable.h"

namespace Editor::ECSUtils {
    uint32_t FNV1aHash(const char* str) {
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint32_t>(*str);
            hash *= 16777619u;
            ++str;
        }
        return hash;
    }

    ECS::ComponentTypeId GetComponentIdFromName(const char* name) {
        if (!name || !*name) return ECS::NULL_COMPONENT_ID;
        return ECS::ComponentRegistry::GetComponentIdFromName(name);
    }

    static ECS::ComponentTypeId GetNameComponentId() {
        static ECS::ComponentTypeId id = ECS::NULL_COMPONENT_ID;
        if (id == ECS::NULL_COMPONENT_ID) {
            id = GetComponentIdFromName("Name");
        }
        return id;
    }

    const ECS::Components::Name* GetNamePtr(ECS::World* world, ECS::Entity entity) {
        if (!world) return nullptr;
        const ECS::ComponentTypeId id = GetNameComponentId();
        if (id == ECS::NULL_COMPONENT_ID) return nullptr;
        return static_cast<const ECS::Components::Name*>(world->GetRawComponentPtr(entity, id));
    }

    ECS::Components::Name* GetNamePtrMutable(ECS::World& world, ECS::Entity entity) {
        const ECS::ComponentTypeId id = GetNameComponentId();
        if (id == ECS::NULL_COMPONENT_ID) return nullptr;
        return static_cast<ECS::Components::Name*>(world.GetRawComponentPtr(entity, id));
    }

    void SetEntityName(ECS::World& world, ECS::Entity entity, const char* value) {
        if (!value) return;
        const ECS::ComponentTypeId id = GetNameComponentId();
        if (id == ECS::NULL_COMPONENT_ID) return;

        ECS::Components::Name nm{};
        nm.Value = ECS::StringTable::Intern(value);
        void* ptr = world.GetRawComponentPtr(entity, id);
        if (ptr) {
            *static_cast<ECS::Components::Name*>(ptr) = nm;
        } else {
            world.AddComponentById(entity, id, &nm, sizeof(ECS::Components::Name));
        }
    }

    void SetEntityName(ECS::World& world, ECS::Entity entity, const std::string& value) {
        SetEntityName(world, entity, value.c_str());
    }
}
