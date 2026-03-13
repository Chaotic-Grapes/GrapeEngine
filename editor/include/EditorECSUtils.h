/* Start Header *****************************************************************/
/*!
\file   EditorECSUtils.h
\author Muhammad Nur Fadzly Bin Zulkifli (95%)
        Samantha Leong (5%)
\par    muhammadnurfadzly.b@digipen.edu
        s.leong@digipen.edu
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
    /**
     * @brief FNV-1a hash function for string hashing.
     * @param str The input string.
     * @return The FNV-1a hash of the string.
     */
    uint32_t FNV1aHash(const char* str);

    /**
     * @brief Get the ComponentTypeId from component name.
     * @param name The name of the component.
     * @return The ComponentTypeId, or NULL_COMPONENT_ID if not found.
     */
    ECS::ComponentTypeId GetComponentIdFromName(const char* name);

    /**
     * @brief Get a pointer to the Name component of an entity.
     * @param world The ECS world.
     * @param entity The entity.
     * @return Pointer to the Name component, or nullptr if not found.
     */
    const ECS::Components::Name* GetNamePtr(ECS::World* world, ECS::Entity entity);

    /**
     * @brief Get a mutable pointer to the Name component of an entity.
     * @param world The ECS world.
     * @param entity The entity.
     * @return Mutable pointer to the Name component, or nullptr if not found.
     */
    ECS::Components::Name* GetNamePtrMutable(ECS::World& world, ECS::Entity entity);

    // =====================================================================
    // Set Entity Name
    // =====================================================================

    /**
     * @brief Set the name of an entity.
     * @param world The ECS world.
     * @param entity The entity.
     * @param value The new name as a C-string.
     */
    void SetEntityName(ECS::World& world, ECS::Entity entity, const char* value);

    /**
     * @brief Set the name of an entity.
     * @param world The ECS world.
     * @param entity The entity.
     * @param value The new name as a std::string.
     */
    void SetEntityName(ECS::World& world, ECS::Entity entity, const std::string& value);

    // =====================================================================
    // Component Manipulation by Name
    // =====================================================================

    /**
     * @brief Check if an entity has a component by name.
     * @param world The ECS world.
     * @param entity The entity.
     * @param name The name of the component.
     * @return True if the entity has the component, false otherwise.
     */
    inline bool HasComponent(ECS::World* world, ECS::Entity entity, const char* name) {
        if (!world)
            return false;
        if (!world->IsAlive(entity))
            return false;

        const ECS::ComponentTypeId id = GetComponentIdFromName(name);

        if (id == ECS::NULL_COMPONENT_ID)
            return false;

        return world->HasById(entity, id);
    }

    /**
     * @brief Get a pointer to a component of an entity by name.
     * @tparam T The component type.
     * @param world The ECS world.
     * @param entity The entity.
     * @param name The name of the component.
     * @return Pointer to the component, or nullptr if not found.
     */
    template<typename T>
    inline T* GetComponentPtr(ECS::World* world, ECS::Entity entity, const char* name) {
        if (!world)
            return nullptr;
        
            const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        
        if (id == ECS::NULL_COMPONENT_ID)
            return nullptr;
        
        return static_cast<T*>(world->GetRawComponentPtr(entity, id));
    }

    /**
     * @brief Get a const pointer to a component of an entity by name.
     * @tparam T The component type.
     * @param world The ECS world.
     * @param entity The entity.
     * @param name The name of the component.
     * @return Const pointer to the component, or nullptr if not found.
     */
    template<typename T>
    inline const T* GetComponentPtr(const ECS::World* world, ECS::Entity entity, const char* name) {
        if (!world)
            return nullptr;
        
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        
        if (id == ECS::NULL_COMPONENT_ID)
            return nullptr;
        
        return static_cast<const T*>(const_cast<ECS::World*>(world)->GetRawComponentPtr(entity, id));
    }

    /**
     * @brief Set the value of a component of an entity by name.
     * @tparam T The component type.
     * @param world The ECS world.
     * @param entity The entity.
     * @param name The name of the component.
     * @param value The new value to set.
     */
    template<typename T>
    inline void SetComponent(ECS::World* world, ECS::Entity entity, const char* name, const T& value) {
        if (!world)
            return;
        
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        
        if (id == ECS::NULL_COMPONENT_ID)
            return;
        
        // Try to get existing component pointer
        void* ptr = world->GetRawComponentPtr(entity, id);
        if (ptr) {
            *static_cast<T*>(ptr) = value;
        } else {
            world->AddComponentById(entity, id, const_cast<T*>(&value), sizeof(T));
        }
    }


    /**
     * @brief Add a component to an entity by name.
     * @tparam T The component type.
     * @param world The ECS world.
     * @param entity The entity.
     * @param name The name of the component.
     * @param value The value to add.
     */
    template<typename T>
    inline void AddComponent(ECS::World* world, ECS::Entity entity, const char* name, const T& value) {
        if (!world)
            return;
        
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        
        if (id == ECS::NULL_COMPONENT_ID)
            return;
        
        world->AddComponentById(entity, id, const_cast<T*>(&value), sizeof(T));
    }

    /**
     * @brief Remove a component from an entity by name.
     * @param world The ECS world.
     * @param entity The entity.
     * @param name The name of the component.
     */
    inline void RemoveComponent(ECS::World* world, ECS::Entity entity, const char* name) {
        if (!world)
            return;
        
        const ECS::ComponentTypeId id = GetComponentIdFromName(name);
        
        if (id == ECS::NULL_COMPONENT_ID)
            return;
        
        world->RemoveById(entity, id);
    }
}

#endif
