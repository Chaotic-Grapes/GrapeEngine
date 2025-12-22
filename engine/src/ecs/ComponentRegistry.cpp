/* Start Header *****************************************************************/
/*!
\file    ComponentRegistry.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of static storage for ComponentRegistry to ensure single instance
across all compilation units. The static maps must be defined in exactly one .cpp
file to prevent ODR violations and ensure all code sees the same registry.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/ComponentRegistry.h"
#include "core/Logger.h"
#include <atomic>
#include <unordered_map>

namespace ECS {

// Define the static member functions that return references to static maps
// These MUST be defined in exactly one .cpp file to avoid ODR (One Definition Rule) violations
// If defined in a header, each .cpp that includes the header gets its own instance!

namespace ComponentRegistryImpl {
    // Global component metadata storage
    // Single instance shared by all code
    static std::unordered_map<ComponentTypeId, ComponentMeta>& GetMetasInstance() {
        static std::unordered_map<ComponentTypeId, ComponentMeta> m;
        return m;
    }

    // Global component type ID counter
    // Ensures unique IDs across all registrations
    static std::atomic<ComponentTypeId>& GetCounterInstance() {
        static std::atomic<ComponentTypeId> counter{0};
        return counter;
    }

    // Hash to ComponentId mapping for C# interop
    // Single instance
    static std::unordered_map<uint32_t, ComponentTypeId>& GetHashToIdInstance() {
        static std::unordered_map<uint32_t, ComponentTypeId> map;
        return map;
    }

    // ComponentTypeId to hash reverse mapping
    // Single instance
    static std::unordered_map<ComponentTypeId, uint32_t>& GetIdToHashInstance() {
        static std::unordered_map<ComponentTypeId, uint32_t> map;
        return map;
    }

    // Type hash to component name mapping - for C# components
    // Single instance
    static std::unordered_map<uint32_t, std::string>& GetHashToNameInstance() {
        static std::unordered_map<uint32_t, std::string> map;
        return map;
    }

    // Public accessors so the header can use these
    std::unordered_map<ComponentTypeId, ComponentMeta>& _metas() {
        return GetMetasInstance();
    }

    std::atomic<ComponentTypeId>& _nextIdCounter() {
        return GetCounterInstance();
    }

    std::unordered_map<uint32_t, ComponentTypeId>& _hashToId() {
        return GetHashToIdInstance();
    }

    std::unordered_map<ComponentTypeId, uint32_t>& _idToHash() {
        return GetIdToHashInstance();
    }

    std::unordered_map<uint32_t, std::string>& _hashToName() {
        return GetHashToNameInstance();
    }
}

}  // namespace ECS

// Define the static member functions of ECS::ComponentRegistry so they have
// external linkage and are visible to all translation units. These forward to
// the internal ComponentRegistryImpl instances above.
namespace ECS {

    std::unordered_map<ComponentTypeId, ComponentMeta>& ComponentRegistry::_metas() {
        return ComponentRegistryImpl::_metas();
    }

    std::atomic<ComponentTypeId>& ComponentRegistry::_nextIdCounter() {
        return ComponentRegistryImpl::_nextIdCounter();
    }

    std::unordered_map<uint32_t, ComponentTypeId>& ComponentRegistry::_hashToId() {
        return ComponentRegistryImpl::_hashToId();
    }

    std::unordered_map<ComponentTypeId, uint32_t>& ComponentRegistry::_idToHash() {
        return ComponentRegistryImpl::_idToHash();
    }

    std::unordered_map<uint32_t, std::string>& ComponentRegistry::_hashToName() {
        return ComponentRegistryImpl::_hashToName();
    }

}
