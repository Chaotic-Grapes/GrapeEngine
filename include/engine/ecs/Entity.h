/* Start Header *****************************************************************/
/*!
\file    Entity.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of the Entity struct, representing an entity
in the Entity-Component-System (ECS) architecture. It provides a unique
identifier for each entity, as well as methods for comparing entities and
checking their validity. 

The entities are used to associate components and systems
within the ECS framework. Entities comprise only an index and a generation counter
to ensure lightweight and efficient handling.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef ENTITY_H
#define ENTITY_H

#include <cstdint>
#include <limits>
#include <functional>
#include <atomic>

// Unique, stable ids per component type at runtime.
using TypeId = uint32_t;
using EntityId = uint32_t;
using PackedEntityId = uint64_t;

namespace ECS {
    struct Entity {
        // Unique identifier for the entity instance
        // Do not modify the values directly! Only get the values!
        EntityId Index = std::numeric_limits<EntityId>::max();

        // Generation counter to track entity version
        // Do not modify the values directly! Only get the values!
        EntityId Generation = 0;

        // Special constant representing a null or invalid entity
        static constexpr EntityId NPOS32 = std::numeric_limits<EntityId>::max();

        /**
         * @brief Checks if the entity is null (invalid).
         * @return True if the entity is null, false otherwise.
         */
        constexpr bool IsNull() const noexcept { return Index == NPOS32; }

        friend constexpr bool operator==(const Entity a, const Entity b) noexcept {
            return a.Index == b.Index && a.Generation == b.Generation;
        }

        friend constexpr bool operator!=(const Entity a, const Entity b) noexcept {
            return !(a == b);
        }

        friend bool operator<(const Entity a, const Entity b) noexcept {
            return a.Index < b.Index || (a.Index == b.Index && a.Generation < b.Generation);
        }

        friend bool operator>(const Entity a, const Entity b) noexcept {
            return b < a;
        }
    };

    // Constant representing a null or invalid entity
    static inline constexpr Entity NULL_ENTITY{};

    // Hash function for Entity to be used in unordered containers
    struct EntityHash {
        size_t operator()(const Entity& e) const noexcept {
            // Combine index and generation into a single size_t hash
            // This assumes size_t is at least 64 bits; adjust as needed for other architectures
            return (static_cast<size_t>(e.Index) << 32ull) ^ static_cast<size_t>(e.Generation);
        }
    };

    /**
     * @brief Generates a unique TypeId for each component type at runtime.
     * @return A unique TypeId.
     */
    inline TypeId TypeIdNext() {
        // Thread-safe unique id generation
        // std::atomic ensures that even in multithreaded contexts,
        // each call gets a unique value
        static std::atomic<TypeId> counter{0};
        return counter++;
    }

    /**
     * @brief Gets the unique TypeId for the specified component type T.
     * @tparam T The component type.
     * @return The unique TypeId for type T.
     */
    template<typename T>
    inline TypeId TypeIdOf() {
        static const TypeId id = TypeIdNext();
        return id;
    }

    /**
     * @brief TypeList is a helper struct to hold a list of types as a parameter pack.
     * This is useful for template metaprogramming and type manipulations.
     */
    template<typename... Ts>
    struct TypeList {};
}

#endif
