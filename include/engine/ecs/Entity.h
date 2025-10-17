#ifndef ENTITY_H
#define ENTITY_H

#include <cstdint>
#include <limits>
#include <functional>

// Unique, stable ids per component type at runtime.
using TypeId = uint32_t;
using EntityId = uint32_t;

namespace ECS {
    struct Entity {
        EntityId Index = std::numeric_limits<EntityId>::max();
        EntityId Generation = 0;

        static constexpr EntityId NPOS32 = std::numeric_limits<EntityId>::max();

        constexpr bool IsNull() const noexcept { return Index == NPOS32; }
        friend constexpr bool operator==(Entity a, Entity b) noexcept {
            return a.Index == b.Index && a.Generation == b.Generation;
        }
        friend constexpr bool operator!=(Entity a, Entity b) noexcept {
            return !(a == b);
        }
        friend bool operator<(Entity a, Entity b) noexcept {
            return a.Index < b.Index || (a.Index == b.Index && a.Generation < b.Generation);
        }
        friend bool operator>(Entity a, Entity b) noexcept {
            return b < a;
        }
    };

    static inline constexpr Entity NULL_ENTITY{};

    struct EntityHash {
        size_t operator()(const Entity& e) const noexcept {
            return (static_cast<size_t>(e.Index) << 32ull) ^ static_cast<size_t>(e.Generation);
        }
    };

    inline TypeId TypeIdNext() {
        static std::atomic<TypeId> counter{0};
        return counter++;
    }

    template<typename T>
    inline TypeId TypeIdOf() {
        static const TypeId id = TypeIdNext();
        return id;
    }

    template<typename... Ts>
    struct TypeList {};
}

#include "Entity.inl"

#endif
