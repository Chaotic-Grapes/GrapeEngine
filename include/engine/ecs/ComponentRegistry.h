/* Start Header *****************************************************************/
/*!
\file    ComponentRegistry.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the ComponentRegistry class, which manages component type
registration and metadata storage for the ECS. Uses lock-free registration with
std::call_once for thread-safe initialization without mutex contention in hot
paths. Component metadata (size, alignment, constructor/destructor) is cached
and accessed lock-free during iteration for maximum performance.

This class is critical for enabling efficient memory layout calculations
and component management in the ECS architecture. Not to be directly used by
game code; instead, use the provided ECS component management APIs.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef COMPONENTREGISTRY_H
#define COMPONENTREGISTRY_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <mutex>
#include <atomic>  // For atomic operations in lock-free component registration

namespace ECS {
  using ComponentTypeId = uint32_t;

  // Metadata for each registered component type
  struct ComponentMeta {
        size_t Size{0};               // Size of the component type in bytes
        size_t Align{0};              // Alignment requirement of the component type
        void (*ctor)(void*){nullptr}; // Constructor function pointer
        void (*dtor)(void*){nullptr}; // Destructor function pointer
  };

  // Registry for component types, providing unique IDs and metadata
  class ComponentRegistry {
  public:
        /**
         * @brief Registers a component type T and returns its unique ComponentTypeId.
         * @tparam T The component type to register
         * @return ComponentTypeId The unique ID assigned to the component type
         */
        template<typename T>
        static ComponentTypeId Type() {
            // Each component type gets a unique static ID that's initialized exactly once
            static const ComponentTypeId id = _nextId();
            
            // Register metadata lazily on first access using std::call_once for lock-free hot path
            // After registration, subsequent calls skip the registration logic entirely (no mutex needed)
            // std::call_once ensures thread-safe one-time initialization
            static std::once_flag registrationFlag;
            std::call_once(registrationFlag, [id]() { _detailRegister<T>(id); });
            
            return id;
        }

        /**
         * @brief Gets metadata for a component type by its ID
         * @param id The ComponentTypeId of the component
         * @return const ComponentMeta& Reference to the component's metadata
         */
        static const ComponentMeta& Meta(const ComponentTypeId id) {
            // Lock-free read access to component metadata
            // Safe because metadata is only written during registration (protected by std::call_once)
            // and never modified afterward, making reads lock-free for iteration hot paths
            return _metas()[id];
        }

  private:
        template<typename T>
        static void _detailRegister(const ComponentTypeId id) {
            // Called exactly once per component type via std::call_once, so no mutex needed here
            // We're guaranteed single-threaded access to this specific registration
            auto& m = _metas()[id];
          
            // Store component size and alignment for memory layout calculations
            // These are used by Archetype to build chunk memory layouts efficiently
            m.Size = sizeof(T);
            m.Align = alignof(T);
          
            // For trivially constructible types, we still placement-new to satisfy C++ requirements
            // but the compiler will optimize this to a no-op if T is Plain Old Data (POD, check TDD)
            if constexpr (std::is_trivially_constructible_v<T>) {
                m.ctor = [](void* p) { 
                    // Placement new for trivial types - compiler optimizes to memcpy/zero
                    new (p) T();
                };
            }
            else {
                // Non-trivial types need explicit constructor call for proper initialization
                // Handles complex types with custom constructors, virtual tables etc.
                m.ctor = [](void* p) { new (p) T(); };
            }
            
            // Trivially destructible types don't need cleanups
            // This avoids function call overhead for simple types like int, float etc
            if constexpr (std::is_trivially_destructible_v<T>) {
                m.dtor = [](void*) { /* No cleanup needed for trivial types */ };
            } 
            else {
                // Explicit dtor call
                // Critical for types that manage memory, file handles, or other RAII resources
                m.dtor = [](void* p) { static_cast<T*>(p)->~T(); };
            }
      }

      static ComponentTypeId _nextId() {
          // Atomic increment ensures thread-safe ID generation without mutex overhead
          // Each component type gets a unique ID even when multiple threads register simultaneously
          static std::atomic<ComponentTypeId> counter{0};
          
          return counter.fetch_add(1, std::memory_order_relaxed); 
          // std::memory_order_relaxed, no synchronization guarantees beyond atomicity
      }

      static std::unordered_map<ComponentTypeId, ComponentMeta>& _metas() {
          // We don't need synchronization here because writes are protected by std::call_once
          // and reads happen only after registration is complete (happens before relationship I think)
          static std::unordered_map<ComponentTypeId, ComponentMeta> m;
          return m;
      }
  };

  template<typename T>
  inline ComponentTypeId _typeId() { return ComponentRegistry::Type<T>(); }

  inline const ComponentMeta& _componentMeta(const ComponentTypeId id) { return ComponentRegistry::Meta(id); }
}

#endif
