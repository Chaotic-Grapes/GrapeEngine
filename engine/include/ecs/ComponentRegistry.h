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

#include "core/Logger.h"
#include "Export.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <mutex>
#include <atomic>  // For atomic operations in lock-free component registration
#include <string>
#include <cstring>

namespace ECS {
  using ComponentTypeId = uint32_t;
  constexpr ComponentTypeId NULL_COMPONENT_ID = static_cast<ComponentTypeId>(-1);

  // Metadata for each registered component type
  struct ComponentMeta {
        size_t Size{0};               // Size of the component type in bytes
        size_t Align{0};              // Alignment requirement of the component type
        uint32_t TypeHash{0};         // FNV-1a hash of component type name (for C# interop)
        bool IsManaged{false};        // True if this component originated from managed (C#) registration
        void (*ctor)(void*){nullptr}; // Constructor function pointer
        void (*dtor)(void*){nullptr}; // Destructor function pointer
  };

  // Registry for component types, providing unique IDs and metadata
    class GRAPEENGINE_API ComponentRegistry {
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
            std::call_once(registrationFlag, [](ComponentTypeId capturedId) { _detailRegister<T>(capturedId); }, id);
            
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
            static const ComponentMeta empty{};
            if (id == NULL_COMPONENT_ID) {
                LOG_ERROR("[ComponentRegistry] Meta lookup requested for NULL_COMPONENT_ID");
                return empty;
            }
            auto& metas = _metas();
            const auto it = metas.find(id);
            if (it == metas.end()) {
                LOG_ERROR("[ComponentRegistry] Meta lookup failed for unknown component id " << id);
                return empty;
            }
            return it->second;
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
          // Call the implementation in ComponentRegistry.cpp
          return _nextIdCounter().fetch_add(1, std::memory_order_relaxed);
      }

      // These are defined in ComponentRegistry.cpp to ensure single instance across all compilation units
      // If they were defined here as inline statics, each .cpp file would get its own instance!
      static std::unordered_map<ComponentTypeId, ComponentMeta>& _metas();
      static std::atomic<ComponentTypeId>& _nextIdCounter();
      static std::unordered_map<uint32_t, ComponentTypeId>& _hashToId();
      static std::unordered_map<ComponentTypeId, uint32_t>& _idToHash();
      static std::unordered_map<uint32_t, std::string>& _hashToName();

  public:
      /**
       * @brief Register a component type with its type name hash (for C# interop)
       * @tparam T The component type
       * @param typeHash FNV-1a hash of the component type name
       */
      template<typename T>
      static void RegisterWithHash(uint32_t typeHash) {
          ComponentTypeId id = Type<T>();
          auto& hashMap = _hashToId();
          const auto it = hashMap.find(typeHash);
          if (it != hashMap.end() && it->second != id) {
              LOG_ERROR("[ComponentRegistry] Hash remap detected during native registration (hash=0x"
                  << std::hex << typeHash << std::dec << ", existing id=" << it->second
                  << ", new id=" << id << ")");
          }
          hashMap[typeHash] = id;
          _idToHash()[id] = typeHash;
          auto& meta = _metas()[id];
          meta.TypeHash = typeHash;
          meta.IsManaged = false;
      }

      /**
       * @brief Register a component type with its type name hash and name (for C# interop + editor)
       * @tparam T The component type
       * @param typeHash FNV-1a hash of the component type name
       * @param typeName Component type name (no namespace)
       */
      template<typename T>
      static void RegisterWithHash(uint32_t typeHash, const char* typeName) {
          RegisterWithHash<T>(typeHash);
          _hashToName()[typeHash] = typeName;
      }

      /**
       * @brief Ensure the hash mapping points to the provided component id.
       * Logs when an existing mapping disagrees and overwrites it.
       * @param typeHash FNV-1a hash of the component type name
       * @param id Component type id to enforce
       * @param typeName Component type name (no namespace)
       */
      static void EnsureHashMapping(uint32_t typeHash, ComponentTypeId id, const char* typeName) {
          auto& hashMap = _hashToId();
          auto it = hashMap.find(typeHash);
          if (it != hashMap.end() && it->second != id) {
              LOG_ERROR("[ComponentRegistry] Hash mapping mismatch for '" << typeName
                  << "' (hash=0x" << std::hex << typeHash << std::dec
                  << ", existing id=" << it->second << ", enforced id=" << id << ")");
          }
          hashMap[typeHash] = id;
          _idToHash()[id] = typeHash;
          if (typeName && *typeName) {
              _hashToName()[typeHash] = typeName;
          }
      }

      /**
       * @brief Get ComponentTypeId from type name hash
       * @param typeHash FNV-1a hash of component type name
       * @return ComponentTypeId or NULL_COMPONENT_ID if not found
       */
      static ComponentTypeId GetComponentIdFromHash(uint32_t typeHash) {
          auto& map = _hashToId();
          auto it = map.find(typeHash);
          return (it != map.end()) ? it->second : NULL_COMPONENT_ID;
      }

      /**
       * @brief Get all registered component IDs
       * @return Vector of all ComponentTypeIds that have been registered
       */
      static std::vector<ComponentTypeId> GetAllComponentIds() {
          std::vector<ComponentTypeId> ids;
          const auto& metas = _metas();
          
          for (const auto& [id, meta] : metas) {
              ids.push_back(id);
          }
          
          return ids;
      }

      /**
       * @brief Get component type name from its hash
       * @param typeHash FNV-1a hash of component type name
       * @return Component name or empty string if not found
       */
      static std::string GetComponentNameFromHash(uint32_t typeHash) {
          auto& nameMap = _hashToName();
          auto it = nameMap.find(typeHash);
          return (it != nameMap.end()) ? it->second : "";
      }

      /**
       * @brief Get ComponentTypeId from type name.
       * @param typeName Component type name (no namespace)
       * @return ComponentTypeId or NULL_COMPONENT_ID if not found
       */
      static ComponentTypeId GetComponentIdFromName(const std::string& typeName) {
          if (typeName.empty()) {
              return NULL_COMPONENT_ID;
          }

          auto allIds = GetAllComponentIds();
          for (ComponentTypeId id : allIds) {
              const auto& meta = Meta(id);
              if (meta.TypeHash == 0) {
                  continue;
              }
              const std::string name = GetComponentNameFromHash(meta.TypeHash);
              if (name == typeName) {
                  return id;
              }
          }

          // Fallback to hash lookup when name map is not ready.
          uint32_t hash = 2166136261u;
          for (char c : typeName) {
              hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
              hash *= 16777619u;
          }

          ComponentTypeId id = GetComponentIdFromHash(hash);
          if (id == NULL_COMPONENT_ID) {
              return NULL_COMPONENT_ID;
          }

          LOG_ERROR("[ComponentRegistry] Name lookup fell back to hash for '" << typeName
              << "' (hash=0x" << std::hex << hash << std::dec << "). Registry names not ready?");
          return id;
      }

      /**
       * @brief Register a component type with only its type name hash (for C# runtime components)
       * This creates a synthetic entry for C# components that don't have C++ type equivalents.
       * @param typeHash FNV-1a hash of the component type name
       * @param size Size of the component in bytes
       * @param alignment Alignment requirement of the component
       * @return ComponentTypeId of the registered component
       */
      static ComponentTypeId RegisterCSharpComponent(uint32_t typeHash, size_t size, size_t alignment) {
          auto& metas = _metas();
          auto& hashMap = _hashToId();
          auto& idToHash = _idToHash();
          
          // Check if this component has already been registered (e.g., in a previous hot reload)
          // If so, reuse the existing ID and update the metadata
          auto existingIt = hashMap.find(typeHash);
          if (existingIt != hashMap.end()) {
              ComponentTypeId existingId = existingIt->second;
              auto& meta = metas[existingId];
              if (!meta.IsManaged) {
                  LOG_ERROR("[ComponentRegistry::RegisterCSharpComponent] Managed registration hit native mapping (hash=0x"
                      << std::hex << typeHash << std::dec << ", id=" << existingId
                      << "). Possible registration order or hot-reload issue.");
                  return existingId;
              }
              LOG_INFO("[ComponentRegistry::RegisterCSharpComponent] Updating existing C# component with hash 0x" << std::hex << typeHash << std::dec << " (ID " << existingId << ")");
              
              // Update the existing metadata with new size/alignment (schema may have changed)
              meta.Size = size;
              meta.Align = alignment;
              meta.TypeHash = typeHash;
              meta.IsManaged = true;
              // Keep the same ctor/dtor (zero-initialize)
              idToHash[existingId] = typeHash;
              
              return existingId;
          }
          
          // New component - create a new ID
          ComponentTypeId id = _nextId();
          
          LOG_INFO("[ComponentRegistry::RegisterCSharpComponent] BEFORE: _metas has " << metas.size() << " entries");
          
          // Create a synthetic metadata entry for the C# component
          ComponentMeta meta;
          meta.Size = size;
          meta.Align = alignment;
          meta.TypeHash = typeHash;
          meta.IsManaged = true;
          // C# components don't have C++ constructors/destructors
          meta.ctor = nullptr;
          meta.dtor = [](void*) { /* No cleanup for C# components */ };
          
          metas[id] = meta;
          hashMap[typeHash] = id;
          idToHash[id] = typeHash;
          
          LOG_INFO("[ComponentRegistry::RegisterCSharpComponent] AFTER: _metas has " << metas.size() << " entries, added ID " << id);
          
          return id;
      }

      /**
       * @brief Register a C# component with its name
       */
      static ComponentTypeId RegisterCSharpComponentWithName(uint32_t typeHash, size_t size, size_t alignment, const std::string& typeName) {
          ComponentTypeId id = RegisterCSharpComponent(typeHash, size, alignment);
          auto& nameMap = _hashToName();
          nameMap[typeHash] = typeName;
          LOG_INFO("[ComponentRegistry::RegisterCSharpComponentWithName] Registered " << typeName << " with hash 0x" << std::hex << typeHash << std::dec);
          return id;
      }

      /**
       * @brief Get all registered component hashes
       * @return Vector of all type hashes that have been registered with C# interop
       */
      static std::vector<uint32_t> GetAllComponentHashes() {
          std::vector<uint32_t> hashes;
          const auto& hashMap = _hashToId();
          for (const auto& [hash, id] : hashMap) {
              hashes.push_back(hash);
          }
          return hashes;
      }
  };

  template<typename T>
  inline ComponentTypeId _typeId() { return ComponentRegistry::Type<T>(); }

  inline const ComponentMeta& _componentMeta(const ComponentTypeId id) { return ComponentRegistry::Meta(id); }
}

#endif
