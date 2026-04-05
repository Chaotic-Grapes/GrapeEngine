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

    /**
     * @brief Compute a 32-bit FNV-1a hash of a null-terminated string.
     * @param str Null-terminated string to hash.
     * @return 32-bit FNV-1a hash value.
     */
    uint32_t FNV1aHash(const char* str) {
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint32_t>(*str);
            hash *= 16777619u;
            ++str;
        }
        return hash;
    }

    /**
     * @brief Look up the component type ID registered under the given name.
     * @param name Null-terminated component name string (e.g. "LocalTransform").
     * @return The registered ComponentTypeId, or ECS::NULL_COMPONENT_ID if not found or name is empty.
     */
    ECS::ComponentTypeId GetComponentIdFromName(const char* name) {
        if (!name || !*name) return ECS::NULL_COMPONENT_ID;
        return ECS::ComponentRegistry::GetComponentIdFromName(name);
    }

    /**
     * @brief Return the component type ID for the Name component, caching it after the first lookup.
     * @return ComponentTypeId for "Name", or ECS::NULL_COMPONENT_ID if not yet registered.
     */
    static ECS::ComponentTypeId GetNameComponentId() {
        static ECS::ComponentTypeId id = ECS::NULL_COMPONENT_ID;
        if (id == ECS::NULL_COMPONENT_ID) {
            id = GetComponentIdFromName("Name");
        }
        return id;
    }

    /**
     * @brief Return a read-only pointer to the Name component of the given entity, or nullptr if absent.
     * @param world Pointer to the ECS world (may be nullptr).
     * @param entity Entity whose Name component to retrieve.
     * @return Const pointer to the Name component, or nullptr if the world is null or the component is missing.
     */
    const ECS::Components::Name* GetNamePtr(ECS::World* world, ECS::Entity entity) {
        if (!world) return nullptr;
        const ECS::ComponentTypeId id = GetNameComponentId();
        if (id == ECS::NULL_COMPONENT_ID) return nullptr;
        return static_cast<const ECS::Components::Name*>(world->GetRawComponentPtr(entity, id));
    }

    /**
     * @brief Return a mutable pointer to the Name component of the given entity, or nullptr if absent.
     * @param world Reference to the ECS world.
     * @param entity Entity whose Name component to retrieve.
     * @return Mutable pointer to the Name component, or nullptr if the component is missing.
     */
    ECS::Components::Name* GetNamePtrMutable(ECS::World& world, ECS::Entity entity) {
        const ECS::ComponentTypeId id = GetNameComponentId();
        if (id == ECS::NULL_COMPONENT_ID) return nullptr;
        return static_cast<ECS::Components::Name*>(world.GetRawComponentPtr(entity, id));
    }

    /**
     * @brief Set or add the Name component on the given entity to the specified string value.
     * @param world Reference to the ECS world.
     * @param entity Entity whose name to set.
     * @param value Null-terminated string to intern and assign; does nothing if null.
     */
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

    /**
     * @brief Set or add the Name component on the given entity to the specified string value.
     * @param world Reference to the ECS world.
     * @param entity Entity whose name to set.
     * @param value String to intern and assign.
     */
    void SetEntityName(ECS::World& world, ECS::Entity entity, const std::string& value) {
        SetEntityName(world, entity, value.c_str());
    }
}
