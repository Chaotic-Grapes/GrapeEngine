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
        EntityId Index = std::numeric_limits<EntityId>::max();
        EntityId Generation = 0;

        static constexpr EntityId NPOS32 = std::numeric_limits<EntityId>::max();

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

    static inline constexpr Entity NULL_ENTITY{};

    struct EntityHash {
        size_t operator()(const Entity& e) const noexcept {
            // Combine index and generation into a single size_t hash
            // This assumes size_t is at least 64 bits; adjust as needed for other architectures
            return (static_cast<size_t>(e.Index) << 32ull) ^ static_cast<size_t>(e.Generation);
        }
    };

    inline TypeId TypeIdNext() {
        // Thread-safe unique id generation
        // std::atomic ensures that even in multithreaded contexts,
        // each call gets a unique value
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

#endif
